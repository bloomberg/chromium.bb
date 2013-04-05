// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/managed_mode/scoped_extension_elevation.h"

#include "chrome/browser/managed_mode/managed_user_service.h"

ScopedExtensionElevation::ScopedExtensionElevation(ManagedUserService* service)
    : service_(service) {
}

void ScopedExtensionElevation::AddExtension(const std::string& extension_id) {
  service_->AddElevationForExtension(extension_id);
  elevated_extensions_.push_back(extension_id);
}

ScopedExtensionElevation::~ScopedExtensionElevation() {
  for (size_t i = 0; i < elevated_extensions_.size(); ++i)
    service_->RemoveElevationForExtension(elevated_extensions_[i]);
}
