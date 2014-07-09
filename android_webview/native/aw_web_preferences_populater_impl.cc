// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/aw_web_preferences_populater_impl.h"

#include "android_webview/native/aw_settings.h"

namespace android_webview {

AwWebPreferencesPopulaterImpl::AwWebPreferencesPopulaterImpl() {
}

AwWebPreferencesPopulaterImpl::~AwWebPreferencesPopulaterImpl() {
}

void AwWebPreferencesPopulaterImpl::PopulateFor(
    content::WebContents* web_contents,
    content::WebPreferences* web_prefs) {
  AwSettings* aw_settings = AwSettings::FromWebContents(web_contents);
  if (aw_settings) {
    aw_settings->PopulateWebPreferences(web_prefs);
  }
}

}  // namespace android_webview
