// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NATIVE_AW_WEB_PREFERENCES_POPULATER_IMPL_H_
#define ANDROID_WEBVIEW_NATIVE_AW_WEB_PREFERENCES_POPULATER_IMPL_H_

#include "android_webview/browser/aw_web_preferences_populater.h"
#include "base/compiler_specific.h"

namespace android_webview {

class AwWebPreferencesPopulaterImpl : public AwWebPreferencesPopulater {
 public:
  AwWebPreferencesPopulaterImpl();
  ~AwWebPreferencesPopulaterImpl() override;

  // AwWebPreferencesPopulater
  void PopulateFor(content::WebContents* web_contents,
                   content::WebPreferences* web_prefs) override;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_NATIVE_AW_WEB_PREFERENCES_POPULATER_IMPL_H_
