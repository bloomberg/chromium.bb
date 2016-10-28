// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/enumerate_modules_model.h"

#include <softpub.h>
#include <stddef.h>
#include <stdint.h>
#include <tlhelp32.h>
#include <wincrypt.h>
#include <wintrust.h>
#include <mscat.h>  // NOLINT: This must be after wincrypt and wintrust.

#include <algorithm>
#include <set>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/debug/leak_annotations.h"
#include "base/environment.h"
#include "base/file_version_info.h"
#include "base/i18n/case_conversion.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/scoped_generic.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "chrome/browser/net/service_providers_win.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/crash_keys.h"
#include "chrome/grit/generated_resources.h"
#include "crypto/sha2.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;

// The path to the Shell Extension key in the Windows registry.
static const wchar_t kRegPath[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved";

// A sort method that sorts by bad modules first, then by full name (including
// path).
static bool ModuleSort(const ModuleEnumerator::Module& a,
                       const ModuleEnumerator::Module& b) {
  if (a.status != b.status)
    return a.status > b.status;

  if (a.location == b.location)
    return a.name < b.name;

  return a.location < b.location;
}

namespace {

// The default amount of time between module inspections. This prevents
// certificate inspection and validation from using a large amount of CPU and
// battery immediately after startup.
constexpr base::TimeDelta kDefaultPerModuleDelay =
    base::TimeDelta::FromSeconds(1);

// A struct to help de-duping modules before adding them to the enumerated
// modules vector.
struct FindModule {
 public:
  explicit FindModule(const ModuleEnumerator::Module& x)
    : module(x) {}
  bool operator()(const ModuleEnumerator::Module& module_in) const {
    return (module.location == module_in.location);
  }

  const ModuleEnumerator::Module& module;
};

// Returns the long path name given a short path name. A short path name is a
// path that follows the 8.3 convention and has ~x in it. If the path is already
// a long path name, the function returns the current path without modification.
bool ConvertToLongPath(const base::string16& short_path,
                       base::string16* long_path) {
  wchar_t long_path_buf[MAX_PATH];
  DWORD return_value = GetLongPathName(short_path.c_str(), long_path_buf,
                                       MAX_PATH);
  if (return_value != 0 && return_value < MAX_PATH) {
    *long_path = long_path_buf;
    return true;
  }

  return false;
}

// Helper for scoped tracking an HCERTSTORE.
struct ScopedHCERTSTORETraits {
  static HCERTSTORE InvalidValue() { return nullptr; }
  static void Free(HCERTSTORE store) {
    ::CertCloseStore(store, 0);
  }
};
using ScopedHCERTSTORE =
    base::ScopedGeneric<HCERTSTORE, ScopedHCERTSTORETraits>;

// Helper for scoped tracking an HCRYPTMSG.
struct ScopedHCRYPTMSGTraits {
  static HCRYPTMSG InvalidValue() { return nullptr; }
  static void Free(HCRYPTMSG message) {
    ::CryptMsgClose(message);
  }
};
using ScopedHCRYPTMSG =
    base::ScopedGeneric<HCRYPTMSG, ScopedHCRYPTMSGTraits>;

// Returns the "Subject" field from the digital signature in the provided
// binary, if any is present. Returns an empty string on failure.
base::string16 GetSubjectNameInFile(const base::FilePath& filename) {
  ScopedHCERTSTORE store;
  ScopedHCRYPTMSG message;

  // Find the crypto message for this filename.
  {
    HCERTSTORE temp_store = nullptr;
    HCRYPTMSG temp_message = nullptr;
    bool result = !!CryptQueryObject(CERT_QUERY_OBJECT_FILE,
      filename.value().c_str(),
      CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
      CERT_QUERY_FORMAT_FLAG_BINARY,
      0,
      nullptr,
      nullptr,
      nullptr,
      &temp_store,
      &temp_message,
      nullptr);
    store.reset(temp_store);
    message.reset(temp_message);
    if (!result)
      return base::string16();
  }

  // Determine the size of the signer info data.
  DWORD signer_info_size = 0;
  bool result = !!CryptMsgGetParam(message.get(),
    CMSG_SIGNER_INFO_PARAM,
    0,
    nullptr,
    &signer_info_size);
  if (!result)
    return base::string16();

  // Allocate enough space to hold the signer info.
  std::unique_ptr<BYTE[]> signer_info_buffer(new BYTE[signer_info_size]);
  CMSG_SIGNER_INFO* signer_info =
      reinterpret_cast<CMSG_SIGNER_INFO*>(signer_info_buffer.get());

  // Obtain the signer info.
  result = !!CryptMsgGetParam(message.get(),
    CMSG_SIGNER_INFO_PARAM,
    0,
    signer_info,
    &signer_info_size);
  if (!result)
    return base::string16();

  // Search for the signer certificate.
  CERT_INFO CertInfo = {0};
  PCCERT_CONTEXT cert_context = nullptr;
  CertInfo.Issuer = signer_info->Issuer;
  CertInfo.SerialNumber = signer_info->SerialNumber;

  cert_context = CertFindCertificateInStore(
    store.get(),
    X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
    0,
    CERT_FIND_SUBJECT_CERT,
    &CertInfo,
    nullptr);
  if (!cert_context)
    return base::string16();

  // Determine the size of the Subject name.
  DWORD subject_name_size = CertGetNameString(
    cert_context, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr, nullptr, 0);
  if (!subject_name_size)
    return base::string16();

  base::string16 subject_name;
  subject_name.resize(subject_name_size);

  // Get subject name.
  if (!(CertGetNameString(cert_context,
    CERT_NAME_SIMPLE_DISPLAY_TYPE,
    0,
    nullptr,
    const_cast<LPWSTR>(subject_name.c_str()),
    subject_name_size))) {
    return base::string16();
  }

  return subject_name;
}

// Helper for scoped tracking a catalog admin context.
struct CryptCATContextScopedTraits {
  static PVOID InvalidValue() { return nullptr; }
  static void Free(PVOID context) {
    CryptCATAdminReleaseContext(context, 0);
  }
};
using ScopedCryptCATContext =
    base::ScopedGeneric<PVOID, CryptCATContextScopedTraits>;

// Helper for scoped tracking of a catalog context. A catalog context is only
// valid with an associated admin context, so this is effectively a std::pair.
// A custom operator!= is required in order for a null |catalog_context| but
// non-null |context| to compare equal to the InvalidValue exposed by the
// traits class.
class CryptCATCatalogContext {
 public:
  CryptCATCatalogContext(PVOID context, PVOID catalog_context)
      : context_(context), catalog_context_(catalog_context) {}

  bool operator!=(const CryptCATCatalogContext& rhs) const {
    return catalog_context_ != rhs.catalog_context_;
  }

  PVOID context() const { return context_; }
  PVOID catalog_context() const { return catalog_context_; }

 private:
  PVOID context_;
  PVOID catalog_context_;
};

struct CryptCATCatalogContextScopedTraits {
  static CryptCATCatalogContext InvalidValue() {
    return CryptCATCatalogContext(nullptr, nullptr);
  }
  static void Free(const CryptCATCatalogContext& c) {
    CryptCATAdminReleaseCatalogContext(
        c.context(), c.catalog_context(), 0);
  }
};
using ScopedCryptCATCatalogContext = base::ScopedGeneric<
    CryptCATCatalogContext, CryptCATCatalogContextScopedTraits>;

// Extracts the subject name and catalog path if the provided file is present in
// a catalog file.
void GetCatalogCertificateInfo(const base::FilePath& filename,
                               ModuleEnumerator::CertificateInfo* cert_info) {
  // Get a crypt context for signature verification.
  ScopedCryptCATContext context;
  {
    PVOID raw_context = nullptr;
    if (!CryptCATAdminAcquireContext(&raw_context, nullptr, 0))
      return;
    context.reset(raw_context);
  }

  // Open the file of interest.
  base::win::ScopedHandle file_handle(CreateFileW(
    filename.value().c_str(), GENERIC_READ,
    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
    nullptr, OPEN_EXISTING, 0, nullptr));
  if (!file_handle.IsValid())
    return;

  // Get the size we need for our hash.
  DWORD hash_size = 0;
  CryptCATAdminCalcHashFromFileHandle(
      file_handle.Get(), &hash_size, nullptr, 0);
  if (hash_size == 0)
    return;

  // Calculate the hash. If this fails then bail.
  std::vector<BYTE> buffer(hash_size);
  if (!CryptCATAdminCalcHashFromFileHandle(file_handle.Get(), &hash_size,
          buffer.data(), 0)) {
    return;
  }

  // Get catalog for our context.
  ScopedCryptCATCatalogContext catalog_context(CryptCATCatalogContext(
      context.get(),
      CryptCATAdminEnumCatalogFromHash(context.get(), buffer.data(), hash_size,
                                       0, nullptr)));
  if (!catalog_context.is_valid())
    return;

  // Get the catalog info. This includes the path to the catalog itself, which
  // contains the signature of interest.
  CATALOG_INFO catalog_info = {};
  catalog_info.cbStruct = sizeof(catalog_info);
  if (!CryptCATCatalogInfoFromContext(
      catalog_context.get().catalog_context(), &catalog_info, 0)) {
    return;
  }

  // Attempt to get the "Subject" field from the signature of the catalog file
  // itself.
  base::FilePath catalog_path(catalog_info.wszCatalogFile);
  base::string16 subject = GetSubjectNameInFile(catalog_path);

  if (subject.empty())
    return;

  cert_info->type = ModuleEnumerator::CERTIFICATE_IN_CATALOG;
  cert_info->path = catalog_path;
  cert_info->subject = subject;
}

// Extracts information about the certificate of the given file, if any is
// found.
void GetCertificateInfo(const base::FilePath& filename,
                        ModuleEnumerator::CertificateInfo* cert_info) {
  DCHECK_EQ(ModuleEnumerator::NO_CERTIFICATE, cert_info->type);
  DCHECK(cert_info->path.empty());
  DCHECK(cert_info->subject.empty());

  GetCatalogCertificateInfo(filename, cert_info);
  if (cert_info->type == ModuleEnumerator::CERTIFICATE_IN_CATALOG)
    return;

  base::string16 subject = GetSubjectNameInFile(filename);
  if (subject.empty())
    return;

  cert_info->type = ModuleEnumerator::CERTIFICATE_IN_FILE;
  cert_info->path = filename;
  cert_info->subject = subject;
}

}  // namespace

ModuleEnumerator::CertificateInfo::CertificateInfo() : type(NO_CERTIFICATE) {}

ModuleEnumerator::Module::Module() {
}

ModuleEnumerator::Module::Module(const Module& rhs) = default;

ModuleEnumerator::Module::Module(ModuleType type,
                                 ModuleStatus status,
                                 const base::string16& location,
                                 const base::string16& name,
                                 const base::string16& product_name,
                                 const base::string16& description,
                                 const base::string16& version,
                                 RecommendedAction recommended_action)
    : type(type),
      status(status),
      location(location),
      name(name),
      product_name(product_name),
      description(description),
      version(version),
      recommended_action(recommended_action),
      duplicate_count(0) {}

ModuleEnumerator::Module::~Module() {
}

// -----------------------------------------------------------------------------

// static
void ModuleEnumerator::NormalizeModule(Module* module) {
  base::string16 path = module->location;
  if (!ConvertToLongPath(path, &module->location))
    module->location = path;

  module->location = base::i18n::ToLower(module->location);

  // Location contains the filename, so the last slash is where the path
  // ends.
  size_t last_slash = module->location.find_last_of(L"\\");
  if (last_slash != base::string16::npos) {
    module->name = module->location.substr(last_slash + 1);
    module->location = module->location.substr(0, last_slash + 1);
  } else {
    module->name = module->location;
    module->location.clear();
  }

  // Some version strings use ", " instead ".". Convert those.
  base::ReplaceSubstringsAfterOffset(&module->version, 0, L", ", L".");

  // Some version strings have things like (win7_rtm.090713-1255) appended
  // to them. Remove that.
  size_t first_space = module->version.find_first_of(L" ");
  if (first_space != base::string16::npos)
    module->version = module->version.substr(0, first_space);

  // The signer may be returned with trailing nulls.
  size_t first_null = module->cert_info.subject.find(L'\0');
  if (first_null != base::string16::npos)
    module->cert_info.subject.resize(first_null);
}

ModuleEnumerator::ModuleEnumerator(EnumerateModulesModel* observer)
    : enumerated_modules_(nullptr),
      observer_(observer),
      per_module_delay_(kDefaultPerModuleDelay) {
}

ModuleEnumerator::~ModuleEnumerator() {
}

void ModuleEnumerator::ScanNow(ModulesVector* list) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  enumerated_modules_ = list;

  // This object can't be reaped until it has finished scanning, so its safe
  // to post a raw pointer to another thread. It will simply be leaked if the
  // scanning has not been finished before shutdown.
  BrowserThread::GetBlockingPool()->PostWorkerTaskWithShutdownBehavior(
      FROM_HERE,
      base::Bind(&ModuleEnumerator::ScanImplStart,
                 base::Unretained(this)),
      base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN);
}

void ModuleEnumerator::SetPerModuleDelayToZero() {
  // Set the delay to zero so the modules enumerate as quickly as possible.
  per_module_delay_ = base::TimeDelta::FromSeconds(0);
}

void ModuleEnumerator::ScanImplStart() {
  base::TimeTicks start_time = base::TimeTicks::Now();

  // The provided destination for the enumerated modules should be empty, as it
  // should only be populated once, by a single ModuleEnumerator instance. See
  // EnumerateModulesModel::ScanNow for details.
  DCHECK(enumerated_modules_->empty());

  // Make sure the path mapping vector is setup so we can collapse paths.
  PreparePathMappings();

  // Enumerating loaded modules must happen first since the other types of
  // modules check for duplication against the loaded modules.
  base::TimeTicks checkpoint = base::TimeTicks::Now();
  EnumerateLoadedModules();
  base::TimeTicks checkpoint2 = base::TimeTicks::Now();
  UMA_HISTOGRAM_TIMES("Conflicts.EnumerateLoadedModules",
                      checkpoint2 - checkpoint);

  checkpoint = checkpoint2;
  EnumerateShellExtensions();
  checkpoint2 = base::TimeTicks::Now();
  UMA_HISTOGRAM_TIMES("Conflicts.EnumerateShellExtensions",
                      checkpoint2 - checkpoint);

  checkpoint = checkpoint2;
  EnumerateWinsockModules();
  checkpoint2 = base::TimeTicks::Now();
  UMA_HISTOGRAM_TIMES("Conflicts.EnumerateWinsockModules",
                      checkpoint2 - checkpoint);

  enumeration_total_time_ = base::TimeTicks::Now() - start_time;

  // Post a delayed task to scan the first module. This forwards directly to
  // ScanImplFinish if there are no modules to scan.
  BrowserThread::GetBlockingPool()->PostDelayedWorkerTask(
      FROM_HERE,
      base::Bind(&ModuleEnumerator::ScanImplModule,
                 base::Unretained(this),
                 0),
      per_module_delay_);
}

void ModuleEnumerator::ScanImplDelay(size_t index) {
  // Bounce this over to a CONTINUE_ON_SHUTDOWN task in the same pool. This is
  // necessary to prevent shutdown hangs while inspecting a module.
  BrowserThread::GetBlockingPool()->PostWorkerTaskWithShutdownBehavior(
      FROM_HERE,
      base::Bind(&ModuleEnumerator::ScanImplModule,
                 base::Unretained(this),
                 index),
      base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN);
}

void ModuleEnumerator::ScanImplModule(size_t index) {
  while (index < enumerated_modules_->size()) {
    base::TimeTicks start_time = base::TimeTicks::Now();
    Module& entry = enumerated_modules_->at(index);
    PopulateModuleInformation(&entry);
    NormalizeModule(&entry);
    CollapsePath(&entry);
    base::TimeDelta elapsed = base::TimeTicks::Now() - start_time;
    enumeration_inspection_time_ += elapsed;
    enumeration_total_time_ += elapsed;

    // With a non-zero delay, bounce back over to ScanImplDelay, which will
    // bounce back to this function and inspect the next module.
    if (!per_module_delay_.is_zero()) {
      BrowserThread::GetBlockingPool()->PostDelayedWorkerTask(
          FROM_HERE,
          base::Bind(&ModuleEnumerator::ScanImplDelay,
                     base::Unretained(this),
                     index + 1),
          per_module_delay_);
      return;
    }

    // If the delay has been set to zero then simply finish the rest of the
    // enumeration in this already started task.
    ++index;
  }

  // Getting here means that all of the modules have been inspected.
  return ScanImplFinish();
}

void ModuleEnumerator::ScanImplFinish() {
  // TODO(chrisha): Annotate any modules that are suspicious/bad.

  ReportThirdPartyMetrics();

  std::sort(enumerated_modules_->begin(),
            enumerated_modules_->end(), ModuleSort);

  UMA_HISTOGRAM_TIMES("Conflicts.EnumerationInspectionTime",
                      enumeration_inspection_time_);
  UMA_HISTOGRAM_TIMES("Conflicts.EnumerationTotalTime",
                      enumeration_total_time_);

  // Send a reply back on the UI thread. The |observer_| outlives this
  // enumerator, so posting a raw pointer is safe. This is done last as
  // DoneScanning will then reap this ModuleEnumerator.
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&EnumerateModulesModel::DoneScanning,
                                     base::Unretained(observer_)));
}

void ModuleEnumerator::EnumerateLoadedModules() {
  // Get all modules in the current process.
  base::win::ScopedHandle snap(::CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,
                               ::GetCurrentProcessId()));
  if (!snap.Get())
    return;

  // Walk the module list.
  MODULEENTRY32 module = { sizeof(module) };
  if (!::Module32First(snap.Get(), &module))
    return;

  do {
    // It would be weird to present chrome.exe as a loaded module.
    if (_wcsicmp(chrome::kBrowserProcessExecutableName, module.szModule) == 0)
      continue;

    Module entry;
    entry.type = LOADED_MODULE;
    entry.location = module.szExePath;
    enumerated_modules_->push_back(entry);
  } while (::Module32Next(snap.Get(), &module));
}

void ModuleEnumerator::EnumerateShellExtensions() {
  ReadShellExtensions(HKEY_LOCAL_MACHINE);
  ReadShellExtensions(HKEY_CURRENT_USER);
}

void ModuleEnumerator::ReadShellExtensions(HKEY parent) {
  base::win::RegistryValueIterator registration(parent, kRegPath);
  while (registration.Valid()) {
    std::wstring key(std::wstring(L"CLSID\\") + registration.Name() +
        L"\\InProcServer32");
    base::win::RegKey clsid;
    if (clsid.Open(HKEY_CLASSES_ROOT, key.c_str(), KEY_READ) != ERROR_SUCCESS) {
      ++registration;
      continue;
    }
    base::string16 dll;
    if (clsid.ReadValue(L"", &dll) != ERROR_SUCCESS) {
      ++registration;
      continue;
    }
    clsid.Close();

    Module entry;
    entry.type = SHELL_EXTENSION;
    entry.location = dll;
    AddToListWithoutDuplicating(entry);

    ++registration;
  }
}

void ModuleEnumerator::EnumerateWinsockModules() {
  // Add to this list the Winsock LSP DLLs.
  WinsockLayeredServiceProviderList layered_providers;
  GetWinsockLayeredServiceProviders(&layered_providers);
  for (size_t i = 0; i < layered_providers.size(); ++i) {
    Module entry;
    entry.type = WINSOCK_MODULE_REGISTRATION;
    entry.status = NOT_MATCHED;
    entry.location = layered_providers[i].path;
    entry.description = layered_providers[i].name;
    entry.recommended_action = NONE;
    entry.duplicate_count = 0;

    wchar_t expanded[MAX_PATH];
    DWORD size = ExpandEnvironmentStrings(
        entry.location.c_str(), expanded, MAX_PATH);
    if (size != 0 && size <= MAX_PATH)
      entry.location = expanded;
    entry.version = base::IntToString16(layered_providers[i].version);
    AddToListWithoutDuplicating(entry);
  }
}

void ModuleEnumerator::PopulateModuleInformation(Module* module) {
  module->status = NOT_MATCHED;
  module->duplicate_count = 0;
  GetCertificateInfo(base::FilePath(module->location), &module->cert_info);
  module->recommended_action = NONE;
  std::unique_ptr<FileVersionInfo> version_info(
      FileVersionInfo::CreateFileVersionInfo(base::FilePath(module->location)));
  if (version_info) {
    module->description = version_info->file_description();
    module->version = version_info->file_version();
    module->product_name = version_info->product_name();
  }
}

void ModuleEnumerator::AddToListWithoutDuplicating(const Module& module) {
  // These are registered modules, not loaded modules so the same module
  // can be registered multiple times, often dozens of times. There is no need
  // to list each registration, so we just increment the count for each module
  // that is counted multiple times.
  ModulesVector::iterator iter;
  iter = std::find_if(enumerated_modules_->begin(),
                      enumerated_modules_->end(),
                      FindModule(module));
  if (iter != enumerated_modules_->end()) {
    iter->duplicate_count++;
    iter->type = static_cast<ModuleType>(iter->type | module.type);
  } else {
    enumerated_modules_->push_back(module);
  }
}

void ModuleEnumerator::PreparePathMappings() {
  path_mapping_.clear();

  std::unique_ptr<base::Environment> environment(base::Environment::Create());
  std::vector<base::string16> env_vars;
  env_vars.push_back(L"LOCALAPPDATA");
  env_vars.push_back(L"ProgramFiles");
  env_vars.push_back(L"ProgramData");
  env_vars.push_back(L"USERPROFILE");
  env_vars.push_back(L"SystemRoot");
  env_vars.push_back(L"TEMP");
  env_vars.push_back(L"TMP");
  env_vars.push_back(L"CommonProgramFiles");
  for (std::vector<base::string16>::const_iterator variable = env_vars.begin();
       variable != env_vars.end(); ++variable) {
    std::string path;
    if (environment->GetVar(base::UTF16ToASCII(*variable).c_str(), &path)) {
      path_mapping_.push_back(
          std::make_pair(base::i18n::ToLower(base::UTF8ToUTF16(path)) + L"\\",
                         L"%" + base::i18n::ToLower(*variable) + L"%"));
    }
  }
}

void ModuleEnumerator::CollapsePath(Module* entry) {
  // Take the path and see if we can use any of the substitution values
  // from the vector constructed above to replace c:\windows with, for
  // example, %systemroot%. The most collapsed path (the one with the
  // minimum length) wins.
  size_t min_length = MAXINT;
  base::string16 location = entry->location;
  for (PathMapping::const_iterator mapping = path_mapping_.begin();
       mapping != path_mapping_.end(); ++mapping) {
    const base::string16& prefix = mapping->first;
    if (base::StartsWith(base::i18n::ToLower(location),
                         base::i18n::ToLower(prefix),
                         base::CompareCase::SENSITIVE)) {
      base::string16 new_location = mapping->second +
                              location.substr(prefix.length() - 1);
      size_t length = new_location.length() - mapping->second.length();
      if (length < min_length) {
        entry->location = new_location;
        min_length = length;
      }
    }
  }
}

void ModuleEnumerator::ReportThirdPartyMetrics() {
  static const wchar_t kMicrosoft[] = L"Microsoft ";
  static const wchar_t kGoogle[] = L"Google Inc";

  // Used for counting unique certificates that need to be validated. A
  // catalog counts as a single certificate, as does a file with a baked in
  // certificate.
  std::set<base::FilePath> unique_certificates;
  size_t microsoft_certificates = 0;
  size_t signed_modules = 0;
  size_t microsoft_modules = 0;
  size_t catalog_modules = 0;
  size_t third_party_loaded = 0;
  size_t third_party_not_loaded = 0;
  for (const auto& module : *enumerated_modules_) {
    if (module.cert_info.type != ModuleEnumerator::NO_CERTIFICATE) {
      ++signed_modules;

      if (module.cert_info.type == ModuleEnumerator::CERTIFICATE_IN_CATALOG)
        ++catalog_modules;

      // The first time this certificate is encountered it will be inserted
      // into the set.
      bool new_certificate =
          unique_certificates.insert(module.cert_info.path).second;

      // Check if the signer name begins with "Microsoft ". Signatures are
      // typically "Microsoft Corporation" or "Microsoft Windows", but others
      // may exist.
      if (module.cert_info.subject.compare(0, arraysize(kMicrosoft) - 1,
                                           kMicrosoft) == 0) {
        ++microsoft_modules;
        if (new_certificate)
          ++microsoft_certificates;
      } else if (module.cert_info.subject == kGoogle) {
        // No need to count these explicitly.
      } else {
        // Count modules that are neither signed by Google nor Microsoft.
        // These are considered "third party" modules.
        if (module.type & LOADED_MODULE) {
          ++third_party_loaded;
        } else {
          ++third_party_not_loaded;
        }
      }
    }
  }

  // Indicate the presence of third party modules in crash data. This allows
  // comparing how much third party modules affect crash rates compared to
  // the regular user distribution.
  base::debug::SetCrashKeyValue(crash_keys::kThirdPartyModulesLoaded,
                                base::SizeTToString(third_party_loaded));
  base::debug::SetCrashKeyValue(crash_keys::kThirdPartyModulesNotLoaded,
                                base::SizeTToString(third_party_not_loaded));

  // Report back some metrics regarding third party modules and certificates.
  UMA_HISTOGRAM_CUSTOM_COUNTS("ThirdPartyModules.Certificates.Total",
                              unique_certificates.size(), 1, 500, 50);
  UMA_HISTOGRAM_CUSTOM_COUNTS("ThirdPartyModules.Certificates.Microsoft",
                              microsoft_certificates, 1, 500, 50);
  UMA_HISTOGRAM_CUSTOM_COUNTS("ThirdPartyModules.Modules.Loaded",
                              third_party_loaded, 1, 500, 50);
  UMA_HISTOGRAM_CUSTOM_COUNTS("ThirdPartyModules.Modules.NotLoaded",
                              third_party_not_loaded, 1, 500, 50);
  UMA_HISTOGRAM_CUSTOM_COUNTS("ThirdPartyModules.Modules.Signed",
                              signed_modules, 1, 500, 50);
  UMA_HISTOGRAM_CUSTOM_COUNTS("ThirdPartyModules.Modules.Signed.Microsoft",
                              microsoft_modules, 1, 500, 50);
  UMA_HISTOGRAM_CUSTOM_COUNTS("ThirdPartyModules.Modules.Signed.Catalog",
                              catalog_modules, 1, 500, 50);
  UMA_HISTOGRAM_CUSTOM_COUNTS("ThirdPartyModules.Modules.Total",
                              enumerated_modules_->size(), 1, 500, 50);
}

//  ----------------------------------------------------------------------------

// static
EnumerateModulesModel* EnumerateModulesModel::GetInstance() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  static EnumerateModulesModel* model = nullptr;
  if (!model) {
    model = new EnumerateModulesModel();
    ANNOTATE_LEAKING_OBJECT_PTR(model);
  }
  return model;
}

void EnumerateModulesModel::AddObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observers_.AddObserver(observer);
}

// Removes an |observer| from the enumerator. May only be called from the UI
// thread and callbacks will also occur on the UI thread.
void EnumerateModulesModel::RemoveObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observers_.RemoveObserver(observer);
}

bool EnumerateModulesModel::ShouldShowConflictWarning() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // If the user has acknowledged the conflict notification, then we don't need
  // to show it again (because the scanning only happens once per the lifetime
  // of the process). If we were to run the scanning more than once, then we'd
  // need to clear the flag somewhere when we are ready to show it again.
  if (conflict_notification_acknowledged_)
    return false;

  return confirmed_bad_modules_detected_ > 0;
}

void EnumerateModulesModel::AcknowledgeConflictNotification() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!conflict_notification_acknowledged_) {
    conflict_notification_acknowledged_ = true;
    for (Observer& observer : observers_)
      observer.OnConflictsAcknowledged();
  }
}

int EnumerateModulesModel::suspected_bad_modules_detected() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return suspected_bad_modules_detected_;
}

// Returns the number of confirmed bad modules found in the last scan.
// Returns 0 if no scan has taken place yet.
int EnumerateModulesModel::confirmed_bad_modules_detected() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return confirmed_bad_modules_detected_;
}

// Returns how many modules to notify the user about.
int EnumerateModulesModel::modules_to_notify_about() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return modules_to_notify_about_;
}

void EnumerateModulesModel::MaybePostScanningTask() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  static bool done = false;
  if (!done) {
    BrowserThread::PostAfterStartupTask(
        FROM_HERE,
        BrowserThread::GetTaskRunnerForThread(BrowserThread::UI),
        base::Bind(&EnumerateModulesModel::ScanNow,
                   base::Unretained(this),
                   true));
    done = true;
  }
}

void EnumerateModulesModel::ScanNow(bool background_mode) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // |module_enumerator_| is used as a lock to know whether or not there are
  // active/pending blocking pool tasks. If a module enumerator exists then a
  // scan is already underway. Otherwise, either no scan has been completed or
  // a scan has terminated.
  if (module_enumerator_) {
    // If a scan is in progress and this request is for immediate results, then
    // inform the background scan. This is done without any locks because the
    // other thread only reads from the value that is being modified, and on
    // Windows its an atomic write.
    if (!background_mode)
      module_enumerator_->SetPerModuleDelayToZero();
    return;
  }

  // Only allow a single scan per process lifetime. Immediately notify any
  // observers that the scan is complete. At this point |enumerated_modules_| is
  // safe to access as no potentially racing blocking pool task can exist.
  if (!enumerated_modules_.empty()) {
    for (Observer& observer : observers_)
      observer.OnScanCompleted();
    return;
  }

  // ScanNow does not block, rather it simply schedules a task.
  module_enumerator_.reset(new ModuleEnumerator(this));
  if (!background_mode)
    module_enumerator_->SetPerModuleDelayToZero();
  module_enumerator_->ScanNow(&enumerated_modules_);
}

base::ListValue* EnumerateModulesModel::GetModuleList() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // If a |module_enumerator_| is still around then scanning has not yet
  // completed, and it is unsafe to read from |enumerated_modules_|.
  if (module_enumerator_.get())
    return nullptr;

  if (enumerated_modules_.empty())
    return nullptr;

  base::ListValue* list = new base::ListValue();

  for (ModuleEnumerator::ModulesVector::const_iterator module =
           enumerated_modules_.begin();
       module != enumerated_modules_.end(); ++module) {
    base::DictionaryValue* data = new base::DictionaryValue();
    data->SetInteger("type", module->type);
    base::string16 type_string;
    if ((module->type & ModuleEnumerator::LOADED_MODULE) == 0) {
      // Module is not loaded, denote type of module.
      if (module->type & ModuleEnumerator::SHELL_EXTENSION)
        type_string = L"Shell Extension";
      if (module->type & ModuleEnumerator::WINSOCK_MODULE_REGISTRATION) {
        if (!type_string.empty())
          type_string += L", ";
        type_string += L"Winsock";
      }
      // Must be one of the above type.
      DCHECK(!type_string.empty());
      type_string += L" -- ";
      type_string += l10n_util::GetStringUTF16(IDS_CONFLICTS_NOT_LOADED_YET);
    }
    data->SetString("type_description", type_string);
    data->SetInteger("status", module->status);
    data->SetString("location", module->location);
    data->SetString("name", module->name);
    data->SetString("product_name", module->product_name);
    data->SetString("description", module->description);
    data->SetString("version", module->version);
    data->SetString("digital_signer", module->cert_info.subject);

    // Figure out the possible resolution help string.
    base::string16 actions;
    base::string16 separator = L" " +
        l10n_util::GetStringUTF16(
            IDS_CONFLICTS_CHECK_POSSIBLE_ACTION_SEPARATOR) +
        L" ";

    if (module->recommended_action & ModuleEnumerator::INVESTIGATING) {
      actions = l10n_util::GetStringUTF16(
          IDS_CONFLICTS_CHECK_INVESTIGATING);
    } else {
      if (module->recommended_action & ModuleEnumerator::UNINSTALL) {
        actions = l10n_util::GetStringUTF16(
            IDS_CONFLICTS_CHECK_POSSIBLE_ACTION_UNINSTALL);
      }
      if (module->recommended_action & ModuleEnumerator::UPDATE) {
        if (!actions.empty())
          actions += separator;
        actions += l10n_util::GetStringUTF16(
            IDS_CONFLICTS_CHECK_POSSIBLE_ACTION_UPDATE);
      }
      if (module->recommended_action & ModuleEnumerator::DISABLE) {
        if (!actions.empty())
          actions += separator;
        actions += l10n_util::GetStringUTF16(
            IDS_CONFLICTS_CHECK_POSSIBLE_ACTION_DISABLE);
      }
    }
    base::string16 possible_resolution;
    if (!actions.empty()) {
      possible_resolution =
          l10n_util::GetStringUTF16(IDS_CONFLICTS_CHECK_POSSIBLE_ACTIONS) +
          L" " + actions;
    }
    data->SetString("possibleResolution", possible_resolution);
    // TODO(chrisha): Set help_url when we have a meaningful place for users
    // to land.

    list->Append(data);
  }

  return list;
}

GURL EnumerateModulesModel::GetConflictUrl() {
  // For now, simply bring up the chrome://conflicts page, which has detailed
  // information about each module.
  if (ShouldShowConflictWarning())
    return GURL(L"chrome://conflicts");
  return GURL();
}

EnumerateModulesModel::EnumerateModulesModel()
    : conflict_notification_acknowledged_(false),
      confirmed_bad_modules_detected_(0),
      modules_to_notify_about_(0),
      suspected_bad_modules_detected_(0) {
}

EnumerateModulesModel::~EnumerateModulesModel() {
}

void EnumerateModulesModel::DoneScanning() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(module_enumerator_.get());

  confirmed_bad_modules_detected_ = 0;
  suspected_bad_modules_detected_ = 0;
  modules_to_notify_about_ = 0;
  for (ModuleEnumerator::ModulesVector::const_iterator module =
       enumerated_modules_.begin();
       module != enumerated_modules_.end(); ++module) {
    if (module->status == ModuleEnumerator::CONFIRMED_BAD) {
      ++confirmed_bad_modules_detected_;
      if (module->recommended_action & ModuleEnumerator::NOTIFY_USER)
        ++modules_to_notify_about_;
    } else if (module->status == ModuleEnumerator::SUSPECTED_BAD) {
      ++suspected_bad_modules_detected_;
      if (module->recommended_action & ModuleEnumerator::NOTIFY_USER)
        ++modules_to_notify_about_;
    }
  }

  module_enumerator_.reset();

  UMA_HISTOGRAM_COUNTS_100("Conflicts.SuspectedBadModules",
                           suspected_bad_modules_detected_);
  UMA_HISTOGRAM_COUNTS_100("Conflicts.ConfirmedBadModules",
                           confirmed_bad_modules_detected_);

  // Forward the callback to any registered observers.
  for (Observer& observer : observers_)
    observer.OnScanCompleted();
}
