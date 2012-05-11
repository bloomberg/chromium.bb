// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROTECTOR_BASE_SETTING_CHANGE_H_
#define CHROME_BROWSER_PROTECTOR_BASE_SETTING_CHANGE_H_
#pragma once

#include <utility>

#include "base/basictypes.h"
#include "base/string16.h"
#include "chrome/browser/tabs/pinned_tab_codec.h"
#include "googleurl/src/gurl.h"

class Browser;
class Profile;
class TemplateURL;
struct SessionStartupPref;

namespace protector {

class CompositeSettingsChange;

// Base class for setting change tracked by Protector.
class BaseSettingChange {
 public:
  // Pair consisting of a display name representing either new or backup setting
  // and priority for it. Priority is used for composite changes, consisting
  // of multiple BaseSettingChange instances - the display name with the highest
  // priority is used.
  typedef std::pair<size_t, string16> DisplayName;

  // Default priority value.
  static const size_t kDefaultNamePriority;

  BaseSettingChange();
  virtual ~BaseSettingChange();

  // Merges |this| with |other| and returns a CompositeSettingsChange instance.
  // Returned instance takes ownership of |other|.
  // Init should not be called for the returned instance.
  // |this| may be another CompositeSettingsChange, in which case |other| is
  // simply added to it and |this| is returned. |other| cannot be
  // CompositeSettingsChange.
  virtual CompositeSettingsChange* MergeWith(BaseSettingChange* other);

  // Returns true if |this| is a result of merging some changes with |other|.
  virtual bool Contains(const BaseSettingChange* other) const;

  // Applies initial actions to the setting if needed. Must be called before
  // any other calls are made, including text getters.
  // Returns true if initialization was successful.
  // Associates this change with |profile| instance so overrides must call the
  // base method.
  virtual bool Init(Profile* profile);

  // Called instead of Init when ProtectorService is disabled. No other members
  // are called in that case.
  virtual void InitWhenDisabled(Profile* profile);

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

  // Returns the display name and priority for the new setting. If multiple
  // BaseSettingChange instances are merged into CompositeSettingsChange
  // instance, the display name with the highest priority will be used for the
  // Apply button (Discard button will have a generic caption in that case).
  // Returns an empty string in |second| if there is no specific representation
  // for new setting value and a generic string should be used.
  virtual DisplayName GetApplyDisplayName() const;

  // Returns a URL uniquely identifying new (to be applied) settings.
  // ProtectorService uses this URLs to decide whether to merge a change
  // with already existing active changes. The URL may be empty.
  virtual GURL GetNewSettingURL() const;

  // Returns true if this change can be merged with other changes.
  virtual bool CanBeMerged() const;

  // Returns |false| if this change is not user-visible. It won't be presented
  // to user on it's own then, but may be merged with other changes and applied
  // or discarded.
  virtual bool IsUserVisible() const;

 protected:
  // Profile instance we've been associated with by an |Init| call.
  Profile* profile() { return profile_; }

 private:
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(BaseSettingChange);
};

// Display name priorities of various change types:
extern const size_t kDefaultSearchProviderChangeNamePriority;
extern const size_t kSessionStartupChangeNamePriority;
extern const size_t kHomepageChangeNamePriority;

// TODO(ivankr): CompositeSettingChange that incapsulates multiple
// BaseSettingChange instances.

// Allocates and initializes BaseSettingChange implementation for default search
// provider setting. Reports corresponding histograms. Both |actual| and
// |backup| may be NULL if corresponding values are unknown or invalid.
// |backup| will be owned by the returned |BaseSettingChange| instance. |actual|
// is not owned and is safe to destroy after Protector::ShowChange has been
// called for the returned instance.
BaseSettingChange* CreateDefaultSearchProviderChange(TemplateURL* actual,
                                                     TemplateURL* backup);

// Allocates and initializes BaseSettingChange implementation for session
// startup setting, including the pinned tabs. Reports corresponding histograms.
BaseSettingChange* CreateSessionStartupChange(
    const SessionStartupPref& actual_startup_pref,
    const PinnedTabCodec::Tabs& actual_pinned_tabs,
    const SessionStartupPref& backup_startup_pref,
    const PinnedTabCodec::Tabs& backup_pinned_tabs);

BaseSettingChange* CreateHomepageChange(
    const std::string& actual_homepage,
    bool actual_homepage_is_ntp,
    bool actual_show_homepage,
    const std::string& backup_homepage,
    bool backup_homepage_is_ntp,
    bool backup_show_homepage);

// Allocates and initializes BaseSettingChange implementation for an unknown
// preferences change with invalid backup.
BaseSettingChange* CreatePrefsBackupInvalidChange();

}  // namespace protector

#endif  // CHROME_BROWSER_PROTECTOR_BASE_SETTING_CHANGE_H_
