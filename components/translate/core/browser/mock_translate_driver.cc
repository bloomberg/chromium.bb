// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/mock_translate_driver.h"

namespace translate {

namespace testing {

const std::string kHtmlMimeType = "text/html";

MockTranslateDriver::MockTranslateDriver()
    : on_is_page_translated_changed_called_(false),
      on_translate_enabled_changed_called_(false),
      language_state_(this) {
}

void MockTranslateDriver::Reset() {
  on_is_page_translated_changed_called_ = false;
  on_translate_enabled_changed_called_ = false;
}

void MockTranslateDriver::OnIsPageTranslatedChanged() {
  on_is_page_translated_changed_called_ = true;
}

void  MockTranslateDriver::OnTranslateEnabledChanged() {
  on_translate_enabled_changed_called_ = true;
}

bool MockTranslateDriver::IsLinkNavigation() {
  return false;
}


bool MockTranslateDriver::IsOffTheRecord() {
  return false;
}

const std::string& MockTranslateDriver::GetContentsMimeType() {
  return kHtmlMimeType;
}

const GURL&  MockTranslateDriver::GetLastCommittedURL() {
  return GURL::EmptyGURL();
}

const GURL& MockTranslateDriver::GetVisibleURL() {
  return GURL::EmptyGURL();
}

bool MockTranslateDriver::HasCurrentPage() {
  return true;
}

}  // namespace testing

}  // namespace translate

