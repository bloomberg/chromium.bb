// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_WEB_PREFERENCES_POPULATER_H_
#define ANDROID_WEBVIEW_BROWSER_AW_WEB_PREFERENCES_POPULATER_H_

namespace content {
class WebContents;
struct WebPreferences;
}

namespace android_webview {

// Empty base class so this can be destroyed by AwContentBrowserClient.
class AwWebPreferencesPopulater {
 public:
  virtual ~AwWebPreferencesPopulater();

  virtual void PopulateFor(content::WebContents* web_contents,
                           content::WebPreferences* web_prefs) = 0;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_WEB_PREFERENCES_POPULATER_H_
