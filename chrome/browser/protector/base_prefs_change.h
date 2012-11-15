// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROTECTOR_BASE_PREFS_CHANGE_H_
#define CHROME_BROWSER_PROTECTOR_BASE_PREFS_CHANGE_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/prefs/public/pref_change_registrar.h"
#include "chrome/browser/protector/base_setting_change.h"

namespace protector {

// BaseSettingChange subclass for PrefService-managed settings changes.
class BasePrefsChange : public BaseSettingChange {
 public:
  BasePrefsChange();
  virtual ~BasePrefsChange();

  // BaseSettingChange overrides:
  virtual bool Init(Profile* profile) OVERRIDE;
  virtual void InitWhenDisabled(Profile* profile) OVERRIDE;

 protected:
  // Marks |this| to be dismissed when |pref_name| is changed. Should only be
  // called after Init.
  void DismissOnPrefChange(const std::string& pref_name);

  // Ignores any further changes to prefs. No further DismissOnPrefChange calls
  // can be made.
  void IgnorePrefChanges();

 private:
  void OnPreferenceChanged(const std::string& pref_name);

  PrefChangeRegistrar pref_observer_;

  DISALLOW_COPY_AND_ASSIGN(BasePrefsChange);
};

}  // namespace protector

#endif  // CHROME_BROWSER_PROTECTOR_BASE_PREFS_CHANGE_H_
