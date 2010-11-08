// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enumerate_modules_model_win.h"

#include <Tlhelp32.h>
#include <wintrust.h>

#include "app/l10n_util.h"
#include "app/win_util.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/file_path.h"
#include "base/file_version_info_win.h"
#include "base/scoped_handle.h"
#include "base/sha2.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/net/service_providers_win.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_service.h"
#include "grit/generated_resources.h"

// The period of time (in milliseconds) to wait until checking to see if any
// incompatible modules exist.
static const int kModuleCheckDelayMs = 60 * 1000;

// A sort method that sorts by ModuleType ordinal (loaded module at the top),
// then by full name (including path).
static bool ModuleSort(const ModuleEnumerator::Module& a,
                       const ModuleEnumerator::Module& b) {
  if (a.type != b.type)
    return a.type < b.type;
  if (a.location == b.location)
    return a.name < b.name;

  return a.location < b.location;
}

namespace {

// Used to protect the LoadedModuleVector which is accessed
// from both the UI thread and the FILE thread.
Lock* lock = NULL;

}

// The browser process module blacklist. This lists all modules that are known
// to cause compatibility issues within the browser process. When adding to this
// list, make sure that all paths are lower-case, in long pathname form, end
// with a slash and use environments variables (or just look at one of the
// comments below and keep it consistent with that). When adding an entry with
// an environment variable not currently used in the list below, make sure to
// update the list in PreparePathMappings. Filename, Description/Signer, and
// Location must be entered as hashes (see GenerateHash). Filename is mandatory.
// Entries without any Description, Signer info, or Location will never be
// marked as confirmed bad (only as suspicious).
const ModuleEnumerator::BlacklistEntry ModuleEnumerator::kModuleBlacklist[] = {
  // Test DLLs, to demonstrate the feature. Will be removed soon.
  {     // apphelp.dll, "%systemroot%\\system32\\"
    "f5fda581", "23d01d5b", "", "", "", NONE
  }, {  // rsaenh.dll, "%systemroot%\\system32\\", "Microsoft Windows"
    "6af212cb", "23d01d5b", "7b47bf79", "", "",
        static_cast<RecommendedAction>(UPDATE | DISABLE | SEE_LINK)
  },

  // NOTE: Please keep this list sorted by dll name, then location.

  // foldersizecolumn.dll.
  {"5ec91bd7", "", "", "", "", NONE},

  // idmmbc.dll, "%programfiles%\\internet download manager\\", "Tonec Inc.".
  // See: http://crbug.com/26892/.
  {"b8dce5c3", "94541bf5", "d33ad640", "", "", NONE},

  // imon.dll.  See: http://crbug.com/21715.
  {"8f42f22e", "", "", "", "", NONE},

  // is3lsp.dll.  See: http://crbug.com/26892.
  {"7ffbdce9", "", "", "", "", NONE},

  // nvlsp.dll.  See: http://crbug.com/22083.
  {"37f907e2", "", "", "", "", NONE},

  // nvshell.dll.  See: http://crbug.com/3269.
  {"9290318f", "", "", "", "", NONE},

  // securenet.dll.  See: http://crbug.com/5165.
  {"9b266e1c", "", "", "", "", NONE},

  // sgprxy.dll.
  {"005965ea", "", "", "", "", NONE},

  // vaproxyd.dll.  See: http://crbug.com/42445.
  {"0a1c7f81", "", "", "", "", NONE},

  // vlsp.dll.  See: http://crbug.com/22826.
  {"2e4eb93d", "", "", "", "", NONE},
};

// Generates an 8 digit hash from the input given.
static void GenerateHash(const std::string& input, std::string* output) {
  if (input.empty()) {
    *output = "";
    return;
  }

  uint8 hash[4];
  base::SHA256HashString(input, hash, sizeof(hash));
  *output = StringToLowerASCII(base::HexEncode(hash, sizeof(hash)));
}

// -----------------------------------------------------------------------------

// static
void ModuleEnumerator::NormalizeModule(Module* module) {
  string16 path = module->location;
  if (!win_util::ConvertToLongPath(path, &module->location))
    module->location = path;

  module->location = l10n_util::ToLower(module->location);

  // Location contains the filename, so the last slash is where the path
  // ends.
  size_t last_slash = module->location.find_last_of(L"\\");
  if (last_slash != string16::npos) {
    module->name = module->location.substr(last_slash + 1);
    module->location = module->location.substr(0, last_slash + 1);
  } else {
    module->name = module->location;
    module->location.clear();
  }

  // Some version strings have things like (win7_rtm.090713-1255) appended
  // to them. Remove that.
  size_t first_space = module->version.find_first_of(L" ");
  if (first_space != string16::npos)
    module->version = module->version.substr(0, first_space);

  module->normalized = true;
}

// static
ModuleEnumerator::ModuleStatus ModuleEnumerator::Match(
    const ModuleEnumerator::Module& module,
    const ModuleEnumerator::BlacklistEntry& blacklisted) {
  // All modules must be normalized before matching against blacklist.
  DCHECK(module.normalized);
  // Filename is mandatory and version should not contain spaces.
  DCHECK(strlen(blacklisted.filename) > 0);
  DCHECK(!strstr(blacklisted.version_from, " "));
  DCHECK(!strstr(blacklisted.version_to, " "));

  std::string filename_hash, location_hash;
  GenerateHash(WideToUTF8(module.name), &filename_hash);
  GenerateHash(WideToUTF8(module.location), &location_hash);

  // Filenames are mandatory. Location is mandatory if given.
  if (filename_hash == blacklisted.filename &&
          (std::string(blacklisted.location).empty() ||
          location_hash == blacklisted.location)) {
    // We have a name match against the blacklist (and possibly location match
    // also), so check version.
    scoped_ptr<Version> module_version(
        Version::GetVersionFromString(module.version));
    scoped_ptr<Version> version_min(
        Version::GetVersionFromString(blacklisted.version_from));
    scoped_ptr<Version> version_max(
        Version::GetVersionFromString(blacklisted.version_to));
    bool version_ok = !version_min.get() && !version_max.get();
    if (!version_ok) {
      bool too_low = version_min.get() &&
          (!module_version.get() ||
          module_version->CompareTo(*version_min.get()) < 0);
      bool too_high = version_max.get() &&
          (!module_version.get() ||
          module_version->CompareTo(*version_max.get()) > 0);
      version_ok = !too_low && !too_high;
    }

    if (version_ok) {
      // At this point, the names match and there is no version specified
      // or the versions also match.

      std::string desc_or_signer(blacklisted.desc_or_signer);
      std::string signer_hash, description_hash;
      GenerateHash(WideToUTF8(module.digital_signer), &signer_hash);
      GenerateHash(WideToUTF8(module.description), &description_hash);

      // If signatures match, we have a winner.
      if (!desc_or_signer.empty() && signer_hash == desc_or_signer)
        return CONFIRMED_BAD;

      // If description matches and location, then we also have a match.
      if (!desc_or_signer.empty() && description_hash == desc_or_signer &&
          !location_hash.empty() && location_hash == blacklisted.location) {
        return CONFIRMED_BAD;
      }

      // We are not sure, but it is likely bad.
      return SUSPECTED_BAD;
    }
  }

  return NOT_MATCHED;
}

ModuleEnumerator::ModuleEnumerator(EnumerateModulesModel* observer)
: observer_(observer) {
  CHECK(BrowserThread::GetCurrentThreadIdentifier(&callback_thread_id_));
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::FILE));
}

ModuleEnumerator::~ModuleEnumerator() {
}

void ModuleEnumerator::ScanNow(ModulesVector* list) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::FILE));
  enumerated_modules_ = list;
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
          NewRunnableMethod(this, &ModuleEnumerator::ScanOnFileThread));
}

void ModuleEnumerator::ScanOnFileThread() {
  enumerated_modules_->clear();

  // Make sure the path mapping vector is setup so we can collapse paths.
  PreparePathMappings();

  // Get all modules in the current process.
  ScopedHandle snap(::CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,
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
    entry.status = NOT_MATCHED;
    entry.normalized = false;
    entry.location = module.szExePath;
    entry.digital_signer =
        GetSubjectNameFromDigitalSignature(FilePath(entry.location));
    entry.recommended_action = NONE;
    scoped_ptr<FileVersionInfo> version_info(
        FileVersionInfo::CreateFileVersionInfo(FilePath(entry.location)));
    if (version_info.get()) {
      FileVersionInfoWin* version_info_win =
          static_cast<FileVersionInfoWin*>(version_info.get());

      VS_FIXEDFILEINFO* fixed_file_info = version_info_win->fixed_file_info();
      if (fixed_file_info) {
        entry.description = version_info_win->file_description();
        entry.version = version_info_win->file_version();
        entry.product_name = version_info_win->product_name();
      }
    }

    NormalizeModule(&entry);
    CollapsePath(&entry);
    enumerated_modules_->push_back(entry);
  } while (::Module32Next(snap.Get(), &module));

  // Add to this list the Winsock LSP DLLs.
  WinsockLayeredServiceProviderList layered_providers;
  GetWinsockLayeredServiceProviders(&layered_providers);
  for (size_t i = 0; i < layered_providers.size(); ++i) {
    Module entry;
    entry.type = WINSOCK_MODULE_REGISTRATION;
    entry.status = NOT_MATCHED;
    entry.normalized = false;
    entry.location = layered_providers[i].path;
    entry.description = layered_providers[i].name;
    entry.recommended_action = NONE;

    wchar_t expanded[MAX_PATH];
    DWORD size = ExpandEnvironmentStrings(
        entry.location.c_str(), expanded, MAX_PATH);
    if (size != 0 && size <= MAX_PATH) {
      entry.digital_signer =
          GetSubjectNameFromDigitalSignature(FilePath(expanded));
    }
    entry.version = base::IntToString16(layered_providers[i].version);

    // Paths have already been collapsed.
    NormalizeModule(&entry);
    enumerated_modules_->push_back(entry);
  }

  MatchAgainstBlacklist();

  std::sort(enumerated_modules_->begin(),
            enumerated_modules_->end(), ModuleSort);

  // Send a reply back on the UI thread.
  BrowserThread::PostTask(
      callback_thread_id_, FROM_HERE,
      NewRunnableMethod(this, &ModuleEnumerator::ReportBack));
}

void ModuleEnumerator::PreparePathMappings() {
  path_mapping_.clear();

  scoped_ptr<base::Environment> environment(base::Environment::Create());
  std::vector<string16> env_vars;
  env_vars.push_back(L"LOCALAPPDATA");
  env_vars.push_back(L"ProgramFiles");
  env_vars.push_back(L"USERPROFILE");
  env_vars.push_back(L"SystemRoot");
  env_vars.push_back(L"TEMP");
  for (std::vector<string16>::const_iterator variable = env_vars.begin();
       variable != env_vars.end(); ++variable) {
    std::string path;
    if (environment->GetVar(WideToASCII(*variable).c_str(), &path)) {
      path_mapping_.push_back(
          std::make_pair(l10n_util::ToLower(UTF8ToWide(path)) + L"\\",
                         L"%" + l10n_util::ToLower(*variable) + L"%"));
    }
  }
}

void ModuleEnumerator::CollapsePath(Module* entry) {
  // Take the path and see if we can use any of the substitution values
  // from the vector constructed above to replace c:\windows with, for
  // example, %systemroot%.
  for (PathMapping::const_iterator mapping = path_mapping_.begin();
       mapping != path_mapping_.end(); ++mapping) {
    string16 prefix = mapping->first;
    if (StartsWith(entry->location, prefix, false)) {
      entry->location = mapping->second +
                        entry->location.substr(prefix.length() - 1);
      return;
    }
  }
}

void ModuleEnumerator::MatchAgainstBlacklist() {
  for (size_t m = 0; m < enumerated_modules_->size(); ++m) {
    // Match this module against the blacklist.
    Module* module = &(*enumerated_modules_)[m];
    module->status = GOOD;  // We change this below potentially.
    for (size_t i = 0; i < arraysize(kModuleBlacklist); ++i) {
      #if !defined(NDEBUG)
        // This saves time when constructing the blacklist.
        std::string hashes(kModuleBlacklist[i].filename);
        std::string hash1, hash2, hash3;
        GenerateHash(kModuleBlacklist[i].filename, &hash1);
        hashes += " - " + hash1;
        GenerateHash(kModuleBlacklist[i].location, &hash2);
        hashes += " - " + hash2;
        GenerateHash(kModuleBlacklist[i].desc_or_signer, &hash3);
        hashes += " - " + hash3;
      #endif

      ModuleStatus status = Match(*module, kModuleBlacklist[i]);
      if (status != NOT_MATCHED) {
        // We have a match against the blacklist. Mark it as such.
        module->status = status;
        module->recommended_action = kModuleBlacklist[i].help_tip;
        break;
      }
    }
  }
}

void ModuleEnumerator::ReportBack() {
  DCHECK(BrowserThread::CurrentlyOn(callback_thread_id_));
  observer_->DoneScanning();
}

string16 ModuleEnumerator::GetSubjectNameFromDigitalSignature(
    const FilePath& filename) {
  HCERTSTORE store = NULL;
  HCRYPTMSG message = NULL;

  // Find the crypto message for this filename.
  bool result = !!CryptQueryObject(CERT_QUERY_OBJECT_FILE,
                                   filename.value().c_str(),
                                   CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
                                   CERT_QUERY_FORMAT_FLAG_BINARY,
                                   0,
                                   NULL,
                                   NULL,
                                   NULL,
                                   &store,
                                   &message,
                                   NULL);
  if (!result)
    return string16();

  // Determine the size of the signer info data.
  DWORD signer_info_size = 0;
  result = !!CryptMsgGetParam(message,
                              CMSG_SIGNER_INFO_PARAM,
                              0,
                              NULL,
                              &signer_info_size);
  if (!result)
    return string16();

  // Allocate enough space to hold the signer info.
  scoped_array<BYTE> signer_info_buffer(new BYTE[signer_info_size]);
  CMSG_SIGNER_INFO* signer_info =
      reinterpret_cast<CMSG_SIGNER_INFO*>(signer_info_buffer.get());

  // Obtain the signer info.
  result = !!CryptMsgGetParam(message,
                              CMSG_SIGNER_INFO_PARAM,
                              0,
                              signer_info,
                              &signer_info_size);
  if (!result)
    return string16();

  // Search for the signer certificate.
  CERT_INFO CertInfo = {0};
  PCCERT_CONTEXT cert_context = NULL;
  CertInfo.Issuer = signer_info->Issuer;
  CertInfo.SerialNumber = signer_info->SerialNumber;

  cert_context = CertFindCertificateInStore(
      store,
      X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
      0,
      CERT_FIND_SUBJECT_CERT,
      &CertInfo,
      NULL);
  if (!cert_context)
    return string16();

  // Determine the size of the Subject name.
  DWORD subject_name_size = 0;
  if (!(subject_name_size = CertGetNameString(cert_context,
                                              CERT_NAME_SIMPLE_DISPLAY_TYPE,
                                              0,
                                              NULL,
                                              NULL,
                                              0))) {
    return string16();
  }

  string16 subject_name;
  subject_name.resize(subject_name_size);

  // Get subject name.
  if (!(CertGetNameString(cert_context,
                          CERT_NAME_SIMPLE_DISPLAY_TYPE,
                          0,
                          NULL,
                          const_cast<LPWSTR>(subject_name.c_str()),
                          subject_name_size))) {
    return string16();
  }

  return subject_name;
}

//  ----------------------------------------------------------------------------

void EnumerateModulesModel::ScanNow() {
  if (scanning_)
    return;  // A scan is already in progress.

  lock->Acquire();  // Balanced in DoneScanning();

  scanning_ = true;

  // Instruct the ModuleEnumerator class to load this on the File thread.
  // ScanNow does not block.
  if (!module_enumerator_)
    module_enumerator_ = new ModuleEnumerator(this);
  module_enumerator_->ScanNow(&enumerated_modules_);
}

ListValue* EnumerateModulesModel::GetModuleList() {
  if (scanning_)
    return NULL;

  lock->Acquire();

  if (enumerated_modules_.empty()) {
    lock->Release();
    return NULL;
  }

  ListValue* list = new ListValue();

  for (ModuleEnumerator::ModulesVector::const_iterator module =
           enumerated_modules_.begin();
       module != enumerated_modules_.end(); ++module) {
    DictionaryValue* data = new DictionaryValue();
    data->SetInteger("type", module->type);
    data->SetString("type_description",
        (module->type == ModuleEnumerator::WINSOCK_MODULE_REGISTRATION) ?
        ASCIIToWide("Winsock") : ASCIIToWide(""));
    data->SetInteger("status", module->status);
    data->SetString("location", module->location);
    data->SetString("name", module->name);
    data->SetString("product_name", module->product_name);
    data->SetString("description", module->description);
    data->SetString("version", module->version.empty() ? ASCIIToWide("") :
        l10n_util::GetStringF(IDS_CONFLICTS_CHECK_VERSION_STRING,
                   module->version));
    data->SetString("digital_signer", module->digital_signer);

    // Figure out the possible resolution help string.
    string16 actions;
    string16 separator = ASCIIToWide(" ") + l10n_util::GetStringUTF16(
        IDS_CONFLICTS_CHECK_POSSIBLE_ACTION_SEPERATOR) +
        ASCIIToWide(" ");

    if (module->recommended_action & ModuleEnumerator::NONE) {
      actions = l10n_util::GetStringUTF16(
          IDS_CONFLICTS_CHECK_INVESTIGATING);
    }
    if (module->recommended_action & ModuleEnumerator::UNINSTALL) {
      if (!actions.empty())
        actions += separator;
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
    string16 possible_resolution = actions.empty() ? ASCIIToWide("") :
        l10n_util::GetStringUTF16(IDS_CONFLICTS_CHECK_POSSIBLE_ACTIONS) +
        ASCIIToWide(" ") +
        actions;
    data->SetString("possibleResolution", possible_resolution);
    data->SetString("help_url", ConstructHelpCenterUrl(*module).spec().c_str());

    list->Append(data);
  }

  lock->Release();
  return list;
}

EnumerateModulesModel::EnumerateModulesModel()
: scanning_(false),
confirmed_bad_modules_detected_(0),
suspected_bad_modules_detected_(0) {
  const CommandLine& cmd_line = *CommandLine::ForCurrentProcess();
  if (cmd_line.HasSwitch(switches::kConflictingModulesCheck)) {
    check_modules_timer_.Start(
      base::TimeDelta::FromMilliseconds(kModuleCheckDelayMs),
      this, &EnumerateModulesModel::ScanNow);
  }

  lock = new Lock();
}

EnumerateModulesModel::~EnumerateModulesModel() {
  delete lock;
}

void EnumerateModulesModel::DoneScanning() {
  confirmed_bad_modules_detected_ = 0;
  suspected_bad_modules_detected_ = 0;
  for (ModuleEnumerator::ModulesVector::const_iterator module =
    enumerated_modules_.begin();
    module != enumerated_modules_.end(); ++module) {
      if (module->status == ModuleEnumerator::CONFIRMED_BAD)
        ++confirmed_bad_modules_detected_;
      if (module->status == ModuleEnumerator::SUSPECTED_BAD)
        ++suspected_bad_modules_detected_;
  }

  scanning_ = false;
  lock->Release();

  NotificationService::current()->Notify(
      NotificationType::MODULE_LIST_ENUMERATED,
      Source<EnumerateModulesModel>(this),
      NotificationService::NoDetails());

  if (suspected_bad_modules_detected_ || confirmed_bad_modules_detected_) {
    bool found_confirmed_bad_modules = confirmed_bad_modules_detected_  > 0;
    NotificationService::current()->Notify(
        NotificationType::MODULE_INCOMPATIBILITY_DETECTED,
        Source<EnumerateModulesModel>(this),
        Details<bool>(&found_confirmed_bad_modules));
  }
}

GURL EnumerateModulesModel::ConstructHelpCenterUrl(
    const ModuleEnumerator::Module& module) {
  if (!(module.recommended_action & ModuleEnumerator::SEE_LINK))
    return GURL();

  // Construct the needed hashes.
  std::string filename, location, description, signer;
  GenerateHash(WideToUTF8(module.name), &filename);
  GenerateHash(WideToUTF8(module.location), &location);
  GenerateHash(WideToUTF8(module.description), &description);
  GenerateHash(WideToUTF8(module.digital_signer), &signer);

  string16 url = l10n_util::GetStringF(IDS_HELP_CENTER_VIEW_CONFLICTS,
      ASCIIToWide(filename), ASCIIToWide(location),
      ASCIIToWide(description), ASCIIToWide(signer));
  return GURL(WideToUTF8(url));
}
