// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/extension_provider.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system.h"
#include "chrome/browser/chromeos/file_system_provider/service.h"
#include "chrome/browser/chromeos/file_system_provider/throttled_file_system.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/permissions/permissions_data.h"

namespace chromeos {
namespace file_system_provider {
namespace {

// Returns boolean indicating success. result->capabilities contains the
// capabilites of the extension.
bool GetProvidingExtensionInfo(const std::string& extension_id,
                               ProvidingExtensionInfo* result,
                               Profile* profile) {
  DCHECK(result);
  extensions::ExtensionRegistry* const registry =
      extensions::ExtensionRegistry::Get(profile);
  DCHECK(registry);

  const extensions::Extension* const extension = registry->GetExtensionById(
      extension_id, extensions::ExtensionRegistry::ENABLED);
  if (!extension || !extension->permissions_data()->HasAPIPermission(
                        extensions::APIPermission::kFileSystemProvider)) {
    return false;
  }

  result->extension_id = extension->id();
  result->name = extension->name();
  const extensions::FileSystemProviderCapabilities* const capabilities =
      extensions::FileSystemProviderCapabilities::Get(extension);
  DCHECK(capabilities);
  result->capabilities = *capabilities;

  return true;
}

}  // namespace

std::unique_ptr<ProvidedFileSystemInterface>
ExtensionProvider::CreateProvidedFileSystem(
    Profile* profile,
    const ProvidedFileSystemInfo& file_system_info) {
  DCHECK(profile);
  return std::make_unique<ThrottledFileSystem>(
      std::make_unique<ProvidedFileSystem>(profile, file_system_info));
}

bool ExtensionProvider::GetCapabilities(Profile* profile,
                                        const ProviderId& provider_id,
                                        Capabilities& result) {
  ProvidingExtensionInfo providing_extension_info;

  // TODO(baileyberro): Change this so error is not swallowed once
  // bug is resolved (crrev.com/c/767629).
  bool success = GetProvidingExtensionInfo(provider_id.GetExtensionId(),
                                           &providing_extension_info, profile);

  result = Capabilities(providing_extension_info.capabilities.configurable(),
                        providing_extension_info.capabilities.watchable(),
                        providing_extension_info.capabilities.multiple_mounts(),
                        providing_extension_info.capabilities.source());
  return success;
}

ExtensionProvider::ExtensionProvider() {}

}  // namespace file_system_provider
}  // namespace chromeos
