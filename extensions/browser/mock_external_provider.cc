// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/mock_external_provider.h"

#include "base/memory/ptr_util.h"
#include "base/version.h"
#include "extensions/browser/external_install_info.h"
#include "extensions/common/extension.h"

namespace extensions {

MockExternalProvider::MockExternalProvider(VisitorInterface* visitor,
                                           Manifest::Location location)
    : location_(location), visitor_(visitor), visit_count_(0) {}

MockExternalProvider::~MockExternalProvider() {}

void MockExternalProvider::UpdateOrAddExtension(const ExtensionId& id,
                                                const std::string& version,
                                                const base::FilePath& path) {
  extension_map_[id] = std::make_pair(version, path);
}

void MockExternalProvider::RemoveExtension(const ExtensionId& id) {
  extension_map_.erase(id);
}

void MockExternalProvider::VisitRegisteredExtension() {
  visit_count_++;
  for (const auto& extension_kv : extension_map_) {
    std::unique_ptr<base::Version> version =
        base::MakeUnique<base::Version>(extension_kv.second.first);
    std::unique_ptr<ExternalInstallInfoFile> info =
        base::MakeUnique<ExternalInstallInfoFile>(
            extension_kv.first, std::move(version), extension_kv.second.second,
            location_, Extension::NO_FLAGS, false, false);
    visitor_->OnExternalExtensionFileFound(*info);
  }
  visitor_->OnExternalProviderReady(this);
}

bool MockExternalProvider::HasExtension(const std::string& id) const {
  return extension_map_.find(id) != extension_map_.end();
}

bool MockExternalProvider::GetExtensionDetails(
    const std::string& id,
    Manifest::Location* location,
    std::unique_ptr<base::Version>* version) const {
  DataMap::const_iterator it = extension_map_.find(id);
  if (it == extension_map_.end())
    return false;

  if (version)
    version->reset(new base::Version(it->second.first));

  if (location)
    *location = location_;

  return true;
}

bool MockExternalProvider::IsReady() const {
  return true;
}

}  // namespace extensions
