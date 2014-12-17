// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"

namespace ios {

namespace {
ChromeBrowserProvider* g_chrome_browser_provider;
}  // namespace

void SetChromeBrowserProvider(ChromeBrowserProvider* provider) {
  g_chrome_browser_provider = provider;
}

ChromeBrowserProvider* GetChromeBrowserProvider() {
  return g_chrome_browser_provider;
}

ChromeBrowserProvider::~ChromeBrowserProvider() {
}

// A dummy implementation of ChromeBrowserProvider.

ChromeBrowserProvider::ChromeBrowserProvider() {
}

net::URLRequestContextGetter*
ChromeBrowserProvider::GetSystemURLRequestContext() {
  return nullptr;
}

PrefService* ChromeBrowserProvider::GetLocalState() {
  return nullptr;
}

InfoBarViewPlaceholder* ChromeBrowserProvider::CreateInfoBarView() {
  return nullptr;
}

infobars::InfoBarManager* ChromeBrowserProvider::GetInfoBarManager(
    web::WebState* web_state) {
  return nullptr;
}

StringProvider* ChromeBrowserProvider::GetStringProvider() {
  return nullptr;
}

void ChromeBrowserProvider::ShowTranslateSettings() {
}

const char* ChromeBrowserProvider::GetChromeUIScheme() {
  return nullptr;
}

void ChromeBrowserProvider::SetUIViewAlphaWithAnimation(UIView* view,
                                                        float alpha) {
}

}  // namespace ios
