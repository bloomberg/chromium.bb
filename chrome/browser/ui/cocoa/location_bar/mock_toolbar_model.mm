// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/location_bar/mock_toolbar_model.h"

namespace chrome {
namespace testing {

MockToolbarModel::MockToolbarModel()
    : ToolbarModel(),
      input_in_progress_(false),
      would_replace_search_url_with_search_terms_(false) {
}

MockToolbarModel::~MockToolbarModel() {
}

string16 MockToolbarModel::GetText(
    bool display_search_urls_as_search_terms) const {
    return string16();
}

GURL MockToolbarModel::GetURL() const {
  return GURL();
}

bool MockToolbarModel::WouldReplaceSearchURLWithSearchTerms() const {
  return would_replace_search_url_with_search_terms_;
}

MockToolbarModel::SecurityLevel MockToolbarModel::GetSecurityLevel() const {
  return NONE;
}

int MockToolbarModel::GetIcon() const {
  return 0;
}

string16 MockToolbarModel::GetEVCertName() const {
  return string16();
}

bool MockToolbarModel::ShouldDisplayURL() const {
  return false;
}

void MockToolbarModel::SetInputInProgress(bool value) {
  input_in_progress_ = value;
}

bool MockToolbarModel::GetInputInProgress() const {
  return input_in_progress_;
}

}  // namespace testing
}  // namespace chrome
