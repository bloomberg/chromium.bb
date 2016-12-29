// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/toolbar/test_toolbar_model_ios.h"

#include "components/toolbar/toolbar_model_impl.h"
#import "ios/chrome/browser/tabs/tab.h"

TestToolbarModelIOS::TestToolbarModelIOS()
    : ToolbarModelIOS(),
      is_loading_(FALSE),
      load_progress_fraction_(0.0),
      can_go_back_(FALSE),
      can_go_forward_(FALSE),
      is_current_tab_native_page_(FALSE),
      is_current_tab_bookmarked_(FALSE) {
  test_toolbar_model_.reset(new TestToolbarModel());
}

TestToolbarModelIOS::~TestToolbarModelIOS() {}

TestToolbarModel* TestToolbarModelIOS::GetToolbarModel() {
  return test_toolbar_model_.get();
}

bool TestToolbarModelIOS::IsLoading() {
  return is_loading_;
}

CGFloat TestToolbarModelIOS::GetLoadProgressFraction() {
  return load_progress_fraction_;
}

bool TestToolbarModelIOS::CanGoBack() {
  return can_go_back_;
}

bool TestToolbarModelIOS::CanGoForward() {
  return can_go_forward_;
}

bool TestToolbarModelIOS::IsCurrentTabNativePage() {
  return is_current_tab_native_page_;
}

bool TestToolbarModelIOS::IsCurrentTabBookmarked() {
  return is_current_tab_bookmarked_;
}

bool TestToolbarModelIOS::IsCurrentTabBookmarkedByUser() {
  return is_current_tab_bookmarked_;
}

bool TestToolbarModelIOS::ShouldDisplayHintText() {
  return false;
}

base::string16 TestToolbarModelIOS::GetFormattedURL(size_t* prefix_end) const {
  return test_toolbar_model_->GetFormattedURL(prefix_end);
}

GURL TestToolbarModelIOS::GetURL() const {
  return test_toolbar_model_->GetURL();
}

security_state::SecurityLevel TestToolbarModelIOS::GetSecurityLevel(
    bool ignore_editing) const {
  return test_toolbar_model_->GetSecurityLevel(ignore_editing);
}

gfx::VectorIconId TestToolbarModelIOS::GetVectorIcon() const {
  return test_toolbar_model_->GetVectorIcon();
}

base::string16 TestToolbarModelIOS::GetSecureVerboseText() const {
  return test_toolbar_model_->GetSecureVerboseText();
}

base::string16 TestToolbarModelIOS::GetEVCertName() const {
  return test_toolbar_model_->GetEVCertName();
}

bool TestToolbarModelIOS::ShouldDisplayURL() const {
  return test_toolbar_model_->ShouldDisplayURL();
}
