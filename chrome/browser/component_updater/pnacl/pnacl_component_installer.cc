// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/pnacl/pnacl_component_installer.h"

#include <string.h>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/values.h"
#include "base/version.h"
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
// TODO(jvoung): will this pre-processor define work with 64-bit Windows?
// NaCl's sel_ldr is 64-bit, but the browser process is 32-bit.
// Will the Omaha query do the right thing as well? If it doesn't, will
// we need two separate Omaha queries (ouch)?
const char kPnaclArch[] =
#if defined(ARCH_CPU_X86)
    "x86-32";
#elif defined(ARCH_CPU_X86_64)
    "x86-64";
#elif defined(ARCH_CPU_ARMEL)
    "arm";
#else
#error "Unknown Architecture in Pnacl Component Installer."
#endif

// The Pnacl components are in a directory with this name.
const FilePath::CharType kPnaclBaseDirectory[] =
    FILE_PATH_LITERAL("Pnacl");

// If we don't have Pnacl installed, this is the version we claim.
// TODO(jvoung): Is there a way to trick the configurator to ping the server
// earlier if there are components that are not yet installed (version 0.0.0.0),
// So that they will be available ASAP? Be careful not to hurt startup speed.
// Make kNullVersion part of ComponentUpdater in that case, to avoid skew?
const char kNullVersion[] = "0.0.0.0";

// The base directory on Windows looks like:
// <profile>\AppData\Local\Google\Chrome\User Data\Pnacl\.
FilePath GetPnaclBaseDirectory() {
  FilePath result;
  PathService::Get(chrome::DIR_USER_DATA, &result);
  return result.Append(kPnaclBaseDirectory);
}

// Pnacl components have the version encoded in the path itself
// so we need to enumerate the directories to find the full path.
// On success it returns something like:
// <profile>\AppData\Local\Google\Chrome\User Data\Pnacl\0.1.2.3\.
//
// TODO(jvoung): Does it garbage collect old versions when a new version is
// installed? Do we need the architecture in the path too? That is for handling
// cases when you share a profile but switch between machine-types (or ignore
// that odd case?).
bool GetLatestPnaclDirectory(FilePath* result, Version* latest) {
  *result = GetPnaclBaseDirectory();
  bool found = false;
  file_util::FileEnumerator
      file_enumerator(*result, false, file_util::FileEnumerator::DIRECTORIES);
  for (FilePath path = file_enumerator.Next(); !path.value().empty();
       path = file_enumerator.Next()) {
    Version version(path.BaseName().MaybeAsASCII());
    if (!version.IsValid())
      continue;
    if (version.CompareTo(*latest) > 0) {
      *latest = version;
      *result = path;
      found = true;
    }
  }
  return found;
}

}  // namespace

bool CheckPnaclComponentManifest(base::DictionaryValue* manifest,
                                 Version* version_out) {
  // Make sure we have the right manifest file.
  std::string name;
  manifest->GetStringASCII("name", &name);
  if (name != kPnaclManifestName)
    return false;

  std::string proposed_version;
  manifest->GetStringASCII("version", &proposed_version);
  Version version(proposed_version.c_str());
  if (!version.IsValid())
    return false;

  std::string arch;
  manifest->GetStringASCII("x-pnacl-arch", &arch);
  if (arch != kPnaclArch)
    return false;

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

bool PnaclComponentInstaller::Install(base::DictionaryValue* manifest,
                                      const FilePath& unpack_path) {
  Version version;
  if (!CheckPnaclComponentManifest(manifest, &version))
    return false;
  if (current_version_.CompareTo(version) > 0)
    return false;

  // Make sure that at least one of the compiler files exists.
  if (!file_util::PathExists(unpack_path.Append(kPnaclCompilerFileName)))
    return false;

  // Passed the basic tests. Time to install it.
  FilePath path =
      GetPnaclBaseDirectory().AppendASCII(version.GetString());
  if (file_util::PathExists(path))
    return false;
  if (!file_util::Move(unpack_path, path))
    return false;

  // Installation is done. Now tell the rest of chrome (just the path service
  // for now). TODO(jvoung): we need notifications if someone surfed to a
  // Pnacl webpage and Pnacl was just installed at this time. They should
  // then be able to reload the page and retry (or something).
  // See: http://code.google.com/p/chromium/issues/detail?id=107438
  current_version_ = version;

  PathService::Override(chrome::FILE_PNACL_COMPONENT, path);
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
      NOTREACHED() << "Could not create Pnacl directory.";
      return;
    }
  }

  Version version(kNullVersion);
  if (GetLatestPnaclDirectory(&path, &version)) {
    // Check if one of the Pnacl files is really there.
    FilePath compiler_path = path.Append(kPnaclCompilerFileName);
    if (!file_util::PathExists(compiler_path)) {
      version = Version(kNullVersion);
    } else {
      // Register the existing path for now, before checking for updates.
      // TODO(jvoung): Will this always happen "early" or will it
      // race with the NaCl plugin in browser tests?
      PathService::Override(chrome::FILE_PNACL_COMPONENT, path);
    }
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&FinishPnaclUpdateRegistration, cus, version));
}

}  // namespace

void RegisterPnaclComponent(ComponentUpdateService* cus) {
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&StartPnaclUpdateRegistration, cus));
}
