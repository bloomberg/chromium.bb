// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROTECTOR_SETTINGS_CHANGE_GLOBAL_ERROR_H_
#define CHROME_BROWSER_PROTECTOR_SETTINGS_CHANGE_GLOBAL_ERROR_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/string16.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/global_error.h"

class Profile;

// Global error about unwanted settings changes.
class SettingsChangeGlobalError : public GlobalError {
 public:
  enum ChangeType {
    kSearchEngineChanged = 0,  // Default search engine has been changed.
    kHomePageChanged,          // Home page has been changed.
  };

  struct Change {
    ChangeType type;       // Which setting has been changed.
    string16 old_setting;  // Old setting value or "" if unknown.
    string16 new_setting;  // New setting value.
  };

  typedef std::vector<Change> ChangesVector;

  // Creates new global error about settings changes |changes|.
  // If user decides to apply changes, |make_changes_cb| is called.
  // If user decides to keep previous settings, |restore_changes_cb| is called.
  SettingsChangeGlobalError(const ChangesVector& changes,
                            const base::Closure& make_changes_cb,
                            const base::Closure& restore_changes_cb);
  virtual ~SettingsChangeGlobalError();

  // Displays a global error bubble for the default browser profile.
  // Can be called from any thread.
  void ShowForDefaultProfile();

  // GlobalError implementation.
  virtual bool HasBadge() OVERRIDE;
  virtual bool HasMenuItem() OVERRIDE;
  virtual int MenuItemCommandID() OVERRIDE;
  virtual string16 MenuItemLabel() OVERRIDE;
  virtual void ExecuteMenuItem(Browser* browser) OVERRIDE;
  virtual bool HasBubbleView() OVERRIDE;
  virtual string16 GetBubbleViewTitle() OVERRIDE;
  virtual string16 GetBubbleViewMessage() OVERRIDE;
  virtual string16 GetBubbleViewAcceptButtonLabel() OVERRIDE;
  virtual string16 GetBubbleViewCancelButtonLabel() OVERRIDE;
  virtual void BubbleViewDidClose() OVERRIDE;
  virtual void BubbleViewAcceptButtonPressed() OVERRIDE;
  virtual void BubbleViewCancelButtonPressed() OVERRIDE;

 private:
  ChangesVector changes_;

  base::Closure make_changes_cb_;
  base::Closure restore_changes_cb_;

  // Profile that we have been added to.
  Profile* profile_;

  // True if user has dismissed the bubble by clicking on one of the buttons.
  bool closed_by_button_;

  base::WeakPtrFactory<SettingsChangeGlobalError> weak_factory_;

  // Helper called on the UI thread to add this global error to the default
  // profile (stored in |profile_|).
  void AddToDefaultProfile();

  // Displays global error bubble. Must be called on the UI thread.
  void Show();

  // Removes global error from its profile and deletes |this| later.
  void RemoveFromProfile();

  DISALLOW_COPY_AND_ASSIGN(SettingsChangeGlobalError);
};

#endif  // CHROME_BROWSER_PROTECTOR_SETTINGS_CHANGE_GLOBAL_ERROR_H_
