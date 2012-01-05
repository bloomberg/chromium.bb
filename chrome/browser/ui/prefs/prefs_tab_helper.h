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
#include "content/public/browser/web_contents_observer.h"

class OverlayUserPrefStore;
class PrefService;
class Profile;
struct WebPreferences;

// Per-tab class to handle user preferences.
class PrefsTabHelper : public content::WebContentsObserver,
                       public content::NotificationObserver {
 public:
  explicit PrefsTabHelper(content::WebContents* contents);
  virtual ~PrefsTabHelper();

  static void InitIncognitoUserPrefStore(OverlayUserPrefStore* pref_store);
  static void InitPerTabUserPrefStore(OverlayUserPrefStore* pref_store);
  static void RegisterUserPrefs(PrefService* prefs);

  PrefService* per_tab_prefs() { return per_tab_prefs_.get(); }

 protected:
  // Update the RenderView's WebPreferences. Exposed as protected for testing.
  virtual void UpdateWebPreferences();

  // content::WebContentsObserver overrides, exposed as protected for testing.
  virtual void RenderViewCreated(RenderViewHost* render_view_host) OVERRIDE;

 private:
  // content::WebContentsObserver overrides:
  virtual void WebContentsDestroyed(content::WebContents* tab) OVERRIDE;

  // content::NotificationObserver overrides:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Update the TabContents's RendererPreferences.
  void UpdateRendererPreferences();

  Profile* GetProfile();

  content::NotificationRegistrar registrar_;

  scoped_ptr<PrefService> per_tab_prefs_;
  PrefChangeRegistrar pref_change_registrar_;
  PrefChangeRegistrar per_tab_pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(PrefsTabHelper);
};

#endif  // CHROME_BROWSER_UI_PREFS_PREFS_TAB_HELPER_H_
