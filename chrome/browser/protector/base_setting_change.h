// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROTECTOR_BASE_SETTING_CHANGE_H_
#define CHROME_BROWSER_PROTECTOR_BASE_SETTING_CHANGE_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/string16.h"
#include "chrome/browser/tabs/pinned_tab_codec.h"

class Browser;
class Profile;
class TemplateURL;
struct SessionStartupPref;

namespace protector {

// Base class for setting change tracked by Protector.
class BaseSettingChange {
 public:
  BaseSettingChange();
  virtual ~BaseSettingChange();

  // Applies initial actions to the setting if needed. Must be called before
  // any other calls are made, including text getters.
  // Returns true if initialization was successful.
  // Associates this change with |profile| instance so overrides must call the
  // base method.
  virtual bool Init(Profile* profile);

  // Persists new setting if needed. |browser| is the Browser instance from
  // which the user action originates.
  virtual void Apply(Browser* browser);

  // Restores old setting if needed. |browser| is the Browser instance from
  // which the user action originates.
  virtual void Discard(Browser* browser);

  // Indicates that user has ignored this change and timeout has passed.
  virtual void Timeout();

  // Returns the resource ID of the badge icon.
  virtual int GetBadgeIconID() const = 0;

  // Returns the resource ID for the menu item icon.
  virtual int GetMenuItemIconID() const = 0;

  // Returns the resource ID for bubble view icon.
  virtual int GetBubbleIconID() const = 0;

  // Returns the wrench menu item and bubble title.
  virtual string16 GetBubbleTitle() const = 0;

  // Returns the bubble message text.
  virtual string16 GetBubbleMessage() const = 0;

  // Returns text for the button to apply the change with |Apply|.
  // Returns empty string if no apply button should be shown.
  virtual string16 GetApplyButtonText() const = 0;

  // Returns text for the button to discard the change with |Discard|.
  virtual string16 GetDiscardButtonText() const = 0;

 protected:
  // Profile instance we've been associated with by an |Init| call.
  Profile* profile() { return profile_; }

 private:
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(BaseSettingChange);
};

// TODO(ivankr): CompositeSettingChange that incapsulates multiple
// BaseSettingChange instances.

// Allocates and initializes BaseSettingChange implementation for default search
// provider setting. Reports corresponding histograms. Both |actual| and
// |backup| may be NULL if corresponding values are unknown or invalid.
// |backup| will be owned by the returned |BaseSettingChange| instance. |actual|
// is not owned and is safe to destroy after Protector::ShowChange has been
// called for the returned instance.
BaseSettingChange* CreateDefaultSearchProviderChange(const TemplateURL* actual,
                                                     TemplateURL* backup);

// Allocates and initializes BaseSettingChange implementation for session
// startup setting, including the pinned tabs. Reports corresponding histograms.
BaseSettingChange* CreateSessionStartupChange(
    const SessionStartupPref& actual_startup_pref,
    const PinnedTabCodec::Tabs& actual_pinned_tabs,
    const SessionStartupPref& backup_startup_pref,
    const PinnedTabCodec::Tabs& backup_pinned_tabs);

// Allocates and initializes BaseSettingChange implementation for an unknown
// preferences change with invalid backup.
BaseSettingChange* CreatePrefsBackupInvalidChange();

}  // namespace protector

#endif  // CHROME_BROWSER_PROTECTOR_BASE_SETTING_CHANGE_H_
