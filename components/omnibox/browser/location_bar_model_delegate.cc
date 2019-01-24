// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/location_bar_model_delegate.h"

bool LocationBarModelDelegate::ShouldPreventElision() const {
  return false;
}

bool LocationBarModelDelegate::ShouldDisplayURL() const {
  return true;
}

void LocationBarModelDelegate::GetSecurityInfo(
    security_state::SecurityInfo* result) const {
  return;
}

scoped_refptr<net::X509Certificate> LocationBarModelDelegate::GetCertificate()
    const {
  return nullptr;
}

const gfx::VectorIcon* LocationBarModelDelegate::GetVectorIconOverride() const {
  return nullptr;
}

bool LocationBarModelDelegate::IsOfflinePage() const {
  return false;
}

AutocompleteClassifier* LocationBarModelDelegate::GetAutocompleteClassifier() {
  return nullptr;
}

TemplateURLService* LocationBarModelDelegate::GetTemplateURLService() {
  return nullptr;
}
