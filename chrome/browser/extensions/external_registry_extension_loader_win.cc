// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_registry_extension_loader_win.h"

#include "base/file_path.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "content/browser/browser_thread.h"
#include "chrome/browser/extensions/external_extension_provider_impl.h"

namespace {

// The Registry hive where to look for external extensions.
const HKEY kRegRoot = HKEY_LOCAL_MACHINE;

// The Registry subkey that contains information about external extensions.
const char kRegistryExtensions[] = "Software\\Google\\Chrome\\Extensions";

// Registry value of of that key that defines the path to the .crx file.
const wchar_t kRegistryExtensionPath[] = L"path";

// Registry value of that key that defines the current version of the .crx file.
const wchar_t kRegistryExtensionVersion[] = L"version";

}  // namespace

void ExternalRegistryExtensionLoader::StartLoading() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(
          this,
          &ExternalRegistryExtensionLoader::LoadOnFileThread));
}

void ExternalRegistryExtensionLoader::LoadOnFileThread() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  scoped_ptr<DictionaryValue> prefs(new DictionaryValue);

  base::win::RegistryKeyIterator iterator(
      kRegRoot, ASCIIToWide(kRegistryExtensions).c_str());
  while (iterator.Valid()) {
    base::win::RegKey key;
    std::wstring key_path = ASCIIToWide(kRegistryExtensions);
    key_path.append(L"\\");
    key_path.append(iterator.Name());
    if (key.Open(kRegRoot, key_path.c_str(), KEY_READ)  == ERROR_SUCCESS) {
      std::wstring extension_path_str;
      if (key.ReadValue(kRegistryExtensionPath, &extension_path_str)
          == ERROR_SUCCESS) {
        FilePath extension_path(extension_path_str);
        if (!extension_path.IsAbsolute()) {
          LOG(ERROR) << "Path " << extension_path_str
                     << " needs to be absolute in key "
                     << key_path;
          ++iterator;
          continue;
        }
        std::wstring extension_version;
        if (key.ReadValue(kRegistryExtensionVersion, &extension_version)
            == ERROR_SUCCESS) {
          std::string id = WideToASCII(iterator.Name());
          StringToLowerASCII(&id);

          if (!Extension::IdIsValid(id)) {
            LOG(ERROR) << "Invalid id value " << id
                       << " for key " << key_path << " .";
            ++iterator;
            continue;
          }

          scoped_ptr<Version> version;
          version.reset(Version::GetVersionFromString(
                            WideToASCII(extension_version)));
          if (!version.get()) {
            LOG(ERROR) << "Invalid version value " << extension_version
                       << " for key " << key_path << " .";
            ++iterator;
            continue;
          }

          prefs->SetString(
              id + "." + ExternalExtensionProviderImpl::kExternalVersion,
              WideToASCII(extension_version));
          prefs->SetString(
              id + "." + ExternalExtensionProviderImpl::kExternalCrx,
              extension_path_str);
        } else {
          // TODO(erikkay): find a way to get this into about:extensions
          LOG(ERROR) << "Missing value " << kRegistryExtensionVersion
                     << " for key " << key_path << " .";
        }
      } else {
        // TODO(erikkay): find a way to get this into about:extensions
        LOG(ERROR) << "Missing value " << kRegistryExtensionPath
                   << " for key " << key_path << " .";
      }
    }
    ++iterator;
  }

  prefs_.reset(prefs.release());
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(
          this,
          &ExternalRegistryExtensionLoader::LoadFinished));
}
