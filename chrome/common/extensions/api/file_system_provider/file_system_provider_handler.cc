// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/file_system_provider/file_system_provider_handler.h"

#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/extensions/api/manifest_types.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_constants.h"
#include "url/gurl.h"

namespace extensions {

FileSystemProviderCapabilities::FileSystemProviderCapabilities()
    : configurable_(false), multiple_mounts_(false), source_(SOURCE_FILE) {
}

FileSystemProviderCapabilities::FileSystemProviderCapabilities(
    bool configurable,
    bool multiple_mounts,
    FileSystemProviderSource source)
    : configurable_(configurable),
      multiple_mounts_(multiple_mounts),
      source_(source) {
}

FileSystemProviderCapabilities::~FileSystemProviderCapabilities() {
}

FileSystemProviderHandler::FileSystemProviderHandler() {
}

FileSystemProviderHandler::~FileSystemProviderHandler() {
}

// static
const FileSystemProviderCapabilities* FileSystemProviderCapabilities::Get(
    const Extension* extension) {
  return static_cast<FileSystemProviderCapabilities*>(
      extension->GetManifestData(manifest_keys::kFileSystemProvider));
}

bool FileSystemProviderHandler::Parse(Extension* extension,
                                      base::string16* error) {
  const base::DictionaryValue* section = NULL;
  extension->manifest()->GetDictionary(manifest_keys::kFileSystemProvider,
                                       &section);
  DCHECK(section);

  api::manifest_types::FileSystemProviderCapabilities idl_capabilities;
  if (!api::manifest_types::FileSystemProviderCapabilities::Populate(
          *section, &idl_capabilities, error)) {
    return false;
  }

  FileSystemProviderSource source = SOURCE_FILE;
  switch (idl_capabilities.source) {
    case api::manifest_types::FILE_SYSTEM_PROVIDER_SOURCE_FILE:
      source = SOURCE_FILE;
      break;
    case api::manifest_types::FILE_SYSTEM_PROVIDER_SOURCE_DEVICE:
      source = SOURCE_DEVICE;
      break;
    case api::manifest_types::FILE_SYSTEM_PROVIDER_SOURCE_NETWORK:
      source = SOURCE_NETWORK;
      break;
    case api::manifest_types::FILE_SYSTEM_PROVIDER_SOURCE_NONE:
      NOTREACHED();
  }

  scoped_ptr<FileSystemProviderCapabilities> capabilities(
      new FileSystemProviderCapabilities(idl_capabilities.configurable,
                                         idl_capabilities.multiple_mounts,
                                         source));

  extension->SetManifestData(manifest_keys::kFileSystemProvider,
                             capabilities.release());
  return true;
}

const std::vector<std::string> FileSystemProviderHandler::Keys() const {
  return SingleKey(manifest_keys::kFileSystemProvider);
}

}  // namespace extensions
