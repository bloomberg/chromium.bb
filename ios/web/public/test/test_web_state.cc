// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/test/test_web_state.h"

namespace web {

BrowserState* TestWebState::GetBrowserState() const {
  return nullptr;
}

NavigationManager* TestWebState::GetNavigationManager() {
  return nullptr;
}

CRWJSInjectionReceiver* TestWebState::GetJSInjectionReceiver() const {
  return nullptr;
}

const std::string& TestWebState::GetContentsMimeType() const {
  return mime_type_;
}

const std::string& TestWebState::GetContentLanguageHeader() const {
  return content_language_;
}

bool TestWebState::ContentIsHTML() const {
  return true;
}

const GURL& TestWebState::GetVisibleURL() const {
  return url_;
}

const GURL& TestWebState::GetLastCommittedURL() const {
  return url_;
}

}  // namespace web
