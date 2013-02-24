// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/pnacl/pnacl_component_installer.h"

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/values.h"
#include "base/version.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "chrome/browser/component_updater/component_updater_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {

// If PNaCl isn't installed yet, but a user is running chrome with
// --enable-pnacl, this is the amount of time to wait before starting
// a background install.
const int kInitialDelaySeconds = 10;

// One of the Pnacl component files, for checking that expected files exist.
// TODO(jvoung): perhaps replace this with a list of the expected files in the
// manifest.json. Use that to check that everything is unpacked.
// However, that would make startup detection even slower (need to check for
// more than one file!).
const char kPnaclCompilerFileName[] = "llc_nexe";

// Name of the Pnacl component specified in the manifest.
const char kPnaclManifestNamePrefix[] = "PNaCl";

// Returns the name of the Pnacl architecture supported by an install.
// NOTE: this is independent of the Omaha "arch" query parameter.
const char* PnaclArch() {
#if defined(ARCH_CPU_X86_FAMILY)
#if defined(ARCH_CPU_X86_64)
  return "x86-64";
#elif defined(OS_WIN)
  bool x86_64 = (base::win::OSInfo::GetInstance()->wow64_status() ==
                 base::win::OSInfo::WOW64_ENABLED);
  return x86_64 ? "x86-64" : "x86-32";
#else
  return "x86-32";
#endif
#elif defined(ARCH_CPU_ARMEL)
  return "arm";
#elif defined(ARCH_CPU_MIPSEL)
  return "mips32";
#else
#error "Add support for your architecture to Pnacl Component Installer."
#endif
}

// Sanitize characters given by PnaclArch so that they can be used
// in path names.  This should only be characters in the set: [a-z0-9_].
// Keep in sync with chrome/browser/nacl_host/pnacl_file_host.
std::string SanitizeForPath(const std::string& input) {
  std::string result;
  ReplaceChars(input, "-", "_", &result);
  return result;
}

// Set the component's hash to the arch-specific PNaCl package.
void SetPnaclHash(CrxComponent* component) {
#if defined(ARCH_CPU_X86_FAMILY)
  // Define both x86_32 and x86_64, and choose below.
  static const uint8 x86_sha256_hash[][32] = {
    { // This corresponds to AppID (x86-32): aealhdcgieaiikaifafholmmeooeeioj
      0x04, 0x0b, 0x73, 0x26, 0x84, 0x08, 0x8a, 0x08, 0x50, 0x57,
      0xeb, 0xcc, 0x4e, 0xe4, 0x48, 0xe9, 0x44, 0x2c, 0xc8, 0xa6, 0xd6,
      0x96, 0x11, 0xd4, 0x2a, 0xc5, 0x26, 0x64, 0x34, 0x76, 0x3d, 0x14},
    { // This corresponds to AppID (x86-64): knlfebnofcjjnkpkapbgfphaagefndik
      0xad, 0xb5, 0x41, 0xde, 0x52, 0x99, 0xda, 0xfa, 0x0f, 0x16,
      0x5f, 0x70, 0x06, 0x45, 0xd3, 0x8a, 0x32, 0x20, 0x84, 0x57, 0x5c,
      0x1f, 0xef, 0xb4, 0x42, 0x32, 0xce, 0x4a, 0x3c, 0x2d, 0x7e, 0x3a}
  };

#if defined(ARCH_CPU_X86_64)
  component->pk_hash.assign(
      x86_sha256_hash[1],
      &x86_sha256_hash[1][sizeof(x86_sha256_hash[1])]);
#elif defined(OS_WIN)
  bool x86_64 = (base::win::OSInfo::GetInstance()->wow64_status() ==
                 base::win::OSInfo::WOW64_ENABLED);
  if (x86_64) {
    component->pk_hash.assign(
        x86_sha256_hash[1],
        &x86_sha256_hash[1][sizeof(x86_sha256_hash[1])]);
  } else {
    component->pk_hash.assign(
        x86_sha256_hash[0],
        &x86_sha256_hash[0][sizeof(x86_sha256_hash[0])]);
  }
#else
  component->pk_hash.assign(
      x86_sha256_hash[0],
      &x86_sha256_hash[0][sizeof(x86_sha256_hash[0])]);
#endif
#elif defined(ARCH_CPU_ARMEL)
  // This corresponds to AppID: jgobdlakdbanalhiagkdgcnofkbebejj
  static const uint8 arm_sha256_hash[] = {
    0x96, 0xe1, 0x3b, 0x0a, 0x31, 0x0d, 0x0b, 0x78, 0x06, 0xa3,
    0x62, 0xde, 0x5a, 0x14, 0x14, 0x99, 0xd4, 0xd9, 0x01, 0x85, 0xc6,
    0x9a, 0xd2, 0x51, 0x90, 0xa4, 0xb4, 0x94, 0xbd, 0xb8, 0x8b, 0xe8};

  component->pk_hash.assign(arm_sha256_hash,
                            &arm_sha256_hash[sizeof(arm_sha256_hash)]);
#elif defined(ARCH_CPU_MIPSEL)
  // This is a dummy CRX hash for MIPS, so that it will at least compile.
  static const uint8 mips32_sha256_hash[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  component->pk_hash.assign(mips32_sha256_hash,
                            &mips32_sha256_hash[sizeof(mips32_sha256_hash)]);
#else
#error "Add support for your architecture to Pnacl Component Installer."
#endif
}


// If we don't have Pnacl installed, this is the version we claim.
const char kNullVersion[] = "0.0.0.0";

// Pnacl components have the version encoded in the path itself:
// <profile>\AppData\Local\Google\Chrome\User Data\Pnacl\0.1.2.3\.
// and the base directory will be:
// <profile>\AppData\Local\Google\Chrome\User Data\Pnacl\.
base::FilePath GetPnaclBaseDirectory() {
  base::FilePath result;
  CHECK(PathService::Get(chrome::DIR_PNACL_BASE, &result));
  return result;
}

bool GetLatestPnaclDirectory(base::FilePath* latest_dir,
                             Version* latest_version,
                             std::vector<base::FilePath>* older_dirs) {
  // Enumerate all versions starting from the base directory.
  base::FilePath base_dir = GetPnaclBaseDirectory();
  bool found = false;
  file_util::FileEnumerator
      file_enumerator(base_dir, false, file_util::FileEnumerator::DIRECTORIES);
  for (base::FilePath path = file_enumerator.Next(); !path.value().empty();
       path = file_enumerator.Next()) {
    Version version(path.BaseName().MaybeAsASCII());
    if (!version.IsValid())
      continue;
    if (found) {
      if (version.CompareTo(*latest_version) > 0) {
        older_dirs->push_back(*latest_dir);
        *latest_dir = path;
        *latest_version = version;
      } else {
        older_dirs->push_back(path);
      }
    } else {
      *latest_version = version;
      *latest_dir = path;
      found = true;
    }
  }
  return found;
}

// Read the PNaCl specific manifest.
base::DictionaryValue* ReadPnaclManifest(const base::FilePath& unpack_path) {
  base::FilePath manifest = unpack_path.Append(
      FILE_PATH_LITERAL("pnacl_public_pnacl_json"));
  if (!file_util::PathExists(manifest))
    return NULL;
  JSONFileValueSerializer serializer(manifest);
  std::string error;
  scoped_ptr<base::Value> root(serializer.Deserialize(NULL, &error));
  if (!root.get())
    return NULL;
  if (!root->IsType(base::Value::TYPE_DICTIONARY))
    return NULL;
  return static_cast<base::DictionaryValue*>(root.release());
}

}  // namespace

bool CheckPnaclComponentManifest(base::DictionaryValue* manifest,
                                 base::DictionaryValue* pnacl_manifest,
                                 Version* version_out) {
  // Make sure we have the right manifest file.
  std::string name;
  manifest->GetStringASCII("name", &name);
  // For the webstore, we've given different names to each of the
  // architecture specific packages, so only the prefix is the same.
  if (StartsWithASCII(kPnaclManifestNamePrefix, name, false)) {
    LOG(WARNING) << "'name' field in manifest is invalid ("
                 << name << ") -- missing prefix ("
                 << kPnaclManifestNamePrefix << ")";
    return false;
  }

  std::string proposed_version;
  manifest->GetStringASCII("version", &proposed_version);
  Version version(proposed_version.c_str());
  if (!version.IsValid()) {
    LOG(WARNING) << "'version' field in manifest is invalid "
                 << version.GetString();
    return false;
  }

  std::string arch;
  pnacl_manifest->GetStringASCII("pnacl-arch", &arch);
  if (arch.compare(PnaclArch()) != 0) {
    LOG(WARNING) << "'pnacl-arch' field in manifest is invalid ("
                 << arch << " vs " << PnaclArch() << ")";
    return false;
  }

  *version_out = version;
  return true;
}

class PnaclComponentInstaller : public ComponentInstaller {
 public:
  explicit PnaclComponentInstaller(const Version& version);

  virtual ~PnaclComponentInstaller() {}

  virtual void OnUpdateError(int error) OVERRIDE;

  virtual bool Install(base::DictionaryValue* manifest,
                       const base::FilePath& unpack_path) OVERRIDE;

 private:
  Version current_version_;
};

PnaclComponentInstaller::PnaclComponentInstaller(
    const Version& version) : current_version_(version) {
  DCHECK(version.IsValid());
}

void PnaclComponentInstaller::OnUpdateError(int error) {
  NOTREACHED() << "Pnacl update error: " << error;
}

namespace {

bool PathContainsPnacl(const base::FilePath& base_path) {
  // Check that at least one of the compiler files exists, for the current ISA.
  std::string expected_filename("pnacl_public_");
  std::string arch = PnaclArch();
  expected_filename = expected_filename + SanitizeForPath(arch) +
      "_" + kPnaclCompilerFileName;
  return file_util::PathExists(base_path.AppendASCII(expected_filename));
}

}  // namespace

bool PnaclComponentInstaller::Install(base::DictionaryValue* manifest,
                                      const base::FilePath& unpack_path) {
  scoped_ptr<base::DictionaryValue> pnacl_manifest(
      ReadPnaclManifest(unpack_path));
  if (pnacl_manifest == NULL) {
    LOG(WARNING) << "Failed to read pnacl manifest.";
    return false;
  }

  Version version;
  if (!CheckPnaclComponentManifest(manifest,
                                   pnacl_manifest.get(),
                                   &version)) {
    LOG(WARNING) << "CheckPnaclComponentManifest failed, not installing.";
    return false;
  }

  // Don't install if the current version is actually newer.
  if (current_version_.CompareTo(version) > 0)
    return false;

  if (!PathContainsPnacl(unpack_path)) {
    LOG(WARNING) << "PathContainsPnacl check failed, not installing.";
    return false;
  }

  // Passed the basic tests. Time to install it.
  base::FilePath path =
      GetPnaclBaseDirectory().AppendASCII(version.GetString());
  if (file_util::PathExists(path)) {
    LOG(WARNING) << "Target path already exists, not installing.";
    return false;
  }
  if (!file_util::Move(unpack_path, path)) {
    LOG(WARNING) << "Move failed, not installing.";
    return false;
  }

  // Installation is done. Now tell the rest of chrome (just the path service
  // for now). TODO(jvoung): we need notifications if someone surfed to a
  // Pnacl webpage and Pnacl was just installed at this time. They should
  // then be able to reload the page and retry (or something).
  // See: http://code.google.com/p/chromium/issues/detail?id=107438
  current_version_ = version;

  PathService::Override(chrome::DIR_PNACL_COMPONENT, path);
  return true;
}

namespace {

void DoCheckForUpdate(ComponentUpdateService* cus,
                   const CrxComponent& pnacl) {
  if (cus->CheckForUpdateSoon(pnacl) != ComponentUpdateService::kOk) {
    LOG(WARNING) << "Pnacl check for update failed.";
  }
}

// Finally, do the registration with the right version number.
void FinishPnaclUpdateRegistration(ComponentUpdateService* cus,
                                   const Version& current_version) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Note: the source is the default of BANDAID, even though the
  // crxes are hosted from CWS.
  CrxComponent pnacl;
  pnacl.name = "pnacl";
  pnacl.installer = new PnaclComponentInstaller(current_version);
  pnacl.version = current_version;
  SetPnaclHash(&pnacl);
  if (cus->RegisterComponent(pnacl) != ComponentUpdateService::kOk) {
    NOTREACHED() << "Pnacl component registration failed.";
  }

  // If PNaCl is not yet installed but it is requested by --enable-pnacl,
  // we want it to be available "soon", so kick off an update check
  // earlier than usual.
  Version null_version(kNullVersion);
  if (current_version.Equals(null_version)) {
    BrowserThread::PostDelayedTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(DoCheckForUpdate, cus, pnacl),
        base::TimeDelta::FromSeconds(kInitialDelaySeconds));
  }
}

// Check if there is an existing version on disk first to know when
// a hosted version is actually newer.
void StartPnaclUpdateRegistration(ComponentUpdateService* cus) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  base::FilePath path = GetPnaclBaseDirectory();
  if (!file_util::PathExists(path)) {
    if (!file_util::CreateDirectory(path)) {
      NOTREACHED() << "Could not create base Pnacl directory.";
      return;
    }
  }

  Version version(kNullVersion);
  std::vector<base::FilePath> older_dirs;
  if (GetLatestPnaclDirectory(&path, &version, &older_dirs)) {
    if (!PathContainsPnacl(path)) {
      version = Version(kNullVersion);
    } else {
      PathService::Override(chrome::DIR_PNACL_COMPONENT, path);
    }
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&FinishPnaclUpdateRegistration, cus, version));

  // Remove older versions of PNaCl.
  for (std::vector<base::FilePath>::iterator iter = older_dirs.begin();
       iter != older_dirs.end(); ++iter) {
    file_util::Delete(*iter, true);
  }
}

}  // namespace

void RegisterPnaclComponent(ComponentUpdateService* cus,
                            const CommandLine& command_line) {
  // Only register when given the right flag.  This is important since
  // we do an early component updater check above (in DoCheckForUpdate).
  if (command_line.HasSwitch(switches::kEnablePnacl)) {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&StartPnaclUpdateRegistration, cus));
  }
}
