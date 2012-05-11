// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PREFS_PREFS_TAB_HELPER_H_
#define CHROME_BROWSER_UI_PREFS_PREFS_TAB_HELPER_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class OverlayUserPrefStore;
class PrefService;
class Profile;

namespace content {
class WebContents;
}

// Per-tab class to handle user preferences.
class PrefsTabHelper : public content::NotificationObserver {
 public:
  explicit PrefsTabHelper(content::WebContents* contents);
  virtual ~PrefsTabHelper();

  static void InitIncognitoUserPrefStore(OverlayUserPrefStore* pref_store);
  static void RegisterUserPrefs(PrefService* prefs);

 protected:
  // Update the RenderView's WebPreferences. Exposed as protected for testing.
  virtual void UpdateWebPreferences();

 private:
  // content::NotificationObserver overrides:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Update the WebContents's RendererPreferences.
  void UpdateRendererPreferences();

  Profile* GetProfile();

  content::WebContents* web_contents_;
  content::NotificationRegistrar registrar_;
  PrefChangeRegistrar pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(PrefsTabHelper);
};

#endif  // CHROME_BROWSER_UI_PREFS_PREFS_TAB_HELPER_H_
