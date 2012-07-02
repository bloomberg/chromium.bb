// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/pnacl/pnacl_component_installer.h"

#include <string.h>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/values.h"
#include "base/version.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "chrome/browser/component_updater/component_updater_service.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {

// CRX hash. This corresponds to AppId: emkhcgigkicgidendmffimilfehocheg
const uint8 sha256_hash[] = {
  0x4c, 0xa7, 0x26, 0x86, 0xa8, 0x26, 0x83, 0x4d, 0x3c, 0x55,
  0x8c, 0x8b, 0x54, 0x7e, 0x27, 0x46, 0xa0, 0xf8, 0xd5, 0x0e, 0xea,
  0x33, 0x46, 0x6e, 0xab, 0x6c, 0xde, 0xba, 0xc0, 0x91, 0xd4, 0x5e };

// One of the Pnacl component files, for checking that expected files exist.
// TODO(jvoung): perhaps replace this with a list of the expected files in the
// manifest.json. Use that to check that everything is unpacked.
// However, that would make startup detection even slower (need to check for
// more than one file!).
const FilePath::CharType kPnaclCompilerFileName[] =
    FILE_PATH_LITERAL("llc");

// Name of the Pnacl component specified in the manifest.
const char kPnaclManifestName[] = "PNaCl";

// Name of the Pnacl architecture in the component manifest.
// NOTE: this is independent of the Omaha query parameter.
// TODO(jvoung): Will the Omaha query do the right thing for windows
// on x86-64?  If it doesn't, will we need two separate Omaha queries (ouch)?
const char* PnaclArch() {
#if defined(ARCH_CPU_X86_FAMILY)
#if defined(ARCH_CPU_X86_64)
  bool x86_64 = true;
#elif defined(OS_WIN)
  bool x86_64 = (base::win::OSInfo::GetInstance()->wow64_status() ==
                 base::win::OSInfo::WOW64_ENABLED);
#else
  bool x86_64 = false;
#endif
  return x86_64 ? "x86-64" : "x86-32";
#elif defined(ARCH_CPU_ARMEL)
  // Eventually we'll need to distinguish arm32 vs thumb2.
  // That may need to be based on the actual nexe rather than a static
  // choice, which would require substantial refactoring.
  return "arm";
#else
#error "Add support for your architecture to Pnacl Component Installer."
#endif
}

// If we don't have Pnacl installed, this is the version we claim.
// TODO(jvoung): Is there a way to trick the configurator to ping the server
// earlier if there are components that are not yet installed (version 0.0.0.0),
// So that they will be available ASAP? Be careful not to hurt startup speed.
// Make kNullVersion part of ComponentUpdater in that case, to avoid skew?
const char kNullVersion[] = "0.0.0.0";

// Pnacl components have the version encoded in the path itself:
// <profile>\AppData\Local\Google\Chrome\User Data\Pnacl\0.1.2.3\.
// and the base directory will be:
// <profile>\AppData\Local\Google\Chrome\User Data\Pnacl\.
FilePath GetPnaclBaseDirectory() {
  FilePath result;
  CHECK(PathService::Get(chrome::DIR_PNACL_BASE, &result));
  return result;
}

bool GetLatestPnaclDirectory(FilePath* latest_dir, Version* latest_version,
                             std::vector<FilePath>* older_dirs) {
  // Enumerate all versions starting from the base directory.
  FilePath base_dir = GetPnaclBaseDirectory();
  bool found = false;
  file_util::FileEnumerator
      file_enumerator(base_dir, false, file_util::FileEnumerator::DIRECTORIES);
  for (FilePath path = file_enumerator.Next(); !path.value().empty();
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
base::DictionaryValue* ReadPnaclManifest(const FilePath& unpack_path) {
  FilePath manifest = unpack_path.Append(FILE_PATH_LITERAL("pnacl.json"));
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
  if (name != kPnaclManifestName) {
    LOG(WARNING) << "'name' field in manifest is invalid ("
                 << name << " vs " << kPnaclManifestName << ")";
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
                       const FilePath& unpack_path) OVERRIDE;

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

bool PathContainsPnacl(const FilePath& base_path) {
  // Check that at least one of the compiler files exists, for the current ISA.
  return file_util::PathExists(
      base_path.AppendASCII(PnaclArch()).Append(kPnaclCompilerFileName));
}

}  // namespace

bool PnaclComponentInstaller::Install(base::DictionaryValue* manifest,
                                      const FilePath& unpack_path) {
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
  FilePath path =
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

// Finally, do the registration with the right version number.
void FinishPnaclUpdateRegistration(ComponentUpdateService* cus,
                                   const Version& version) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CrxComponent pnacl;
  pnacl.name = "pnacl";
  pnacl.installer = new PnaclComponentInstaller(version);
  pnacl.version = version;
  pnacl.pk_hash.assign(sha256_hash, &sha256_hash[sizeof(sha256_hash)]);
  if (cus->RegisterComponent(pnacl) != ComponentUpdateService::kOk) {
    NOTREACHED() << "Pnacl component registration failed.";
  }
}

// Check if there is an existing version on disk first to know when
// a hosted version is actually newer.
void StartPnaclUpdateRegistration(ComponentUpdateService* cus) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  FilePath path = GetPnaclBaseDirectory();
  if (!file_util::PathExists(path)) {
    if (!file_util::CreateDirectory(path)) {
      NOTREACHED() << "Could not create base Pnacl directory.";
      return;
    }
  }

  Version version(kNullVersion);
  std::vector<FilePath> older_dirs;
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
  for (std::vector<FilePath>::iterator iter = older_dirs.begin();
       iter != older_dirs.end(); ++iter) {
    file_util::Delete(*iter, true);
  }
}

}  // namespace

void RegisterPnaclComponent(ComponentUpdateService* cus) {
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&StartPnaclUpdateRegistration, cus));
}
