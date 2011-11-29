// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TAB_CONTENTS_PER_TAB_PREFS_TAB_HELPER_H_
#define CHROME_BROWSER_UI_TAB_CONTENTS_PER_TAB_PREFS_TAB_HELPER_H_
#pragma once

#include "chrome/browser/prefs/pref_change_registrar.h"
#include "content/browser/tab_contents/tab_contents_observer.h"

class PrefService;
class TabContentsWrapper;
struct WebPreferences;

// Per-tab class to override user preferences.
class PerTabPrefsTabHelper : public TabContentsObserver {
 public:
  explicit PerTabPrefsTabHelper(TabContentsWrapper* tab_contents);
  virtual ~PerTabPrefsTabHelper();

  PrefService* prefs() { return prefs_.get(); }

  void OverrideWebPreferences(WebPreferences* prefs);

 private:
  void RegisterPerTabUserPrefs(PrefService* prefs);

  // TabContentsObserver overrides:
  virtual void TabContentsDestroyed(TabContents* tab) OVERRIDE;

  // Our owning TabContentsWrapper.
  TabContentsWrapper* wrapper_;

  scoped_ptr<PrefService> prefs_;
  PrefChangeRegistrar pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(PerTabPrefsTabHelper);
};

#endif  // CHROME_BROWSER_UI_TAB_CONTENTS_PER_TAB_PREFS_TAB_HELPER_H_
