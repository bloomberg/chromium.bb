// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROTECTOR_BASE_PREFS_CHANGE_H_
#define CHROME_BROWSER_PROTECTOR_BASE_PREFS_CHANGE_H_
#pragma once

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/protector/base_setting_change.h"
#include "content/public/browser/notification_observer.h"

class PrefSetObserver;

namespace protector {

// BaseSettingChange subclass for PrefService-managed settings changes.
class BasePrefsChange : public BaseSettingChange,
                        public content::NotificationObserver {
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
  // content::NotificationObserver overrides:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  scoped_ptr<PrefSetObserver> pref_observer_;

  DISALLOW_COPY_AND_ASSIGN(BasePrefsChange);
};

}  // namespace protector

#endif  // CHROME_BROWSER_PROTECTOR_BASE_PREFS_CHANGE_H_
