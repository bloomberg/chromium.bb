// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_registry_extension_loader_win.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/scoped_handle.h"
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

bool CanOpenFileForReading(const FilePath& path) {
  ScopedStdioHandle file_handle(file_util::OpenFile(path, "rb"));
  return file_handle.get() != NULL;
}

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
  for (; iterator.Valid(); ++iterator) {
    base::win::RegKey key;
    std::wstring key_path = ASCIIToWide(kRegistryExtensions);
    key_path.append(L"\\");
    key_path.append(iterator.Name());
    if (key.Open(kRegRoot, key_path.c_str(), KEY_READ) != ERROR_SUCCESS) {
      LOG(ERROR) << "Unable to read registry key at path: " << key_path << ".";
      continue;
    }

    std::wstring extension_path_str;
    if (key.ReadValue(kRegistryExtensionPath, &extension_path_str)
        != ERROR_SUCCESS) {
      // TODO(erikkay): find a way to get this into about:extensions
      LOG(ERROR) << "Missing value " << kRegistryExtensionPath
                 << " for key " << key_path << ".";
      continue;
    }

    FilePath extension_path(extension_path_str);
    if (!extension_path.IsAbsolute()) {
      LOG(ERROR) << "File path " << extension_path_str
                 << " needs to be absolute in key "
                 << key_path;
      continue;
    }

    if (!file_util::PathExists(extension_path)) {
      LOG(ERROR) << "File " << extension_path_str
                 << " for key " << key_path
                 << " does not exist or is not readable.";
      continue;
    }

    if (!CanOpenFileForReading(extension_path)) {
      LOG(ERROR) << "File " << extension_path_str
                 << " for key " << key_path << " can not be read. "
                 << "Check that users who should have the extension "
                 << "installed have permission to read it.";
      continue;
    }

    std::wstring extension_version;
    if (key.ReadValue(kRegistryExtensionVersion, &extension_version)
        != ERROR_SUCCESS) {
      // TODO(erikkay): find a way to get this into about:extensions
      LOG(ERROR) << "Missing value " << kRegistryExtensionVersion
                 << " for key " << key_path << ".";
      continue;
    }

    std::string id = WideToASCII(iterator.Name());
    StringToLowerASCII(&id);
    if (!Extension::IdIsValid(id)) {
      LOG(ERROR) << "Invalid id value " << id
                 << " for key " << key_path << ".";
      continue;
    }

    scoped_ptr<Version> version;
    version.reset(Version::GetVersionFromString(
                      WideToASCII(extension_version)));
    if (!version.get()) {
      LOG(ERROR) << "Invalid version value " << extension_version
                 << " for key " << key_path << ".";
      continue;
    }

    prefs->SetString(
        id + "." + ExternalExtensionProviderImpl::kExternalVersion,
        WideToASCII(extension_version));
    prefs->SetString(
        id + "." + ExternalExtensionProviderImpl::kExternalCrx,
        extension_path_str);
  }

  prefs_.reset(prefs.release());
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(
          this,
          &ExternalRegistryExtensionLoader::LoadFinished));
}
