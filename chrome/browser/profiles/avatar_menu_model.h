// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_AVATAR_MENU_MODEL_H_
#define CHROME_BROWSER_PROFILES_AVATAR_MENU_MODEL_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/string16.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/gfx/image/image.h"

class AvatarMenuModelObserver;
class Browser;
class ProfileInfoInterface;

// This class is the model for the menu-like interface that appears when the
// avatar icon is clicked in the browser window frame. This class will notify
// its observer when the backend data changes, and the controller/view for this
// model should forward actions back to it in response to user events.
class AvatarMenuModel : public content::NotificationObserver {
 public:
  // Represents an item in the menu.
  struct Item {
    Item(size_t model_index, const gfx::Image& icon);
    ~Item();

    // The icon to be displayed next to the item.
    gfx::Image icon;

    // Whether or not the current browser is using this profile.
    bool active;

    // The name of this profile.
    string16 name;

    // A string representing the sync state of the profile.
    string16 sync_state;

    // Whether or not the current profile is signed in. If true, |sync_state| is
    // expected to be the email of the signed in user.
    bool signed_in;

    // The index in the |profile_cache| that this Item represents.
    size_t model_index;
  };

  // Constructor. |observer| can be NULL. |browser| can be NULL and a new one
  // will be created if an action requires it.
  AvatarMenuModel(ProfileInfoInterface* profile_cache,
                  AvatarMenuModelObserver* observer,
                  Browser* browser);
  virtual ~AvatarMenuModel();

  // Actions performed by the view that the controller forwards back to the
  // model:
  // Opens a Browser with the specified profile in response to the user
  // selecting an item. If |always_create| is true then a new window is created
  // even if a window for that profile already exists.
  void SwitchToProfile(size_t index, bool always_create);
  // Opens the profile settings in response to clicking the edit button next to
  // an item.
  void EditProfile(size_t index);
  // Creates a new profile.
  void AddNewProfile(ProfileMetrics::ProfileAdd type);

  // Gets the number of profiles.
  size_t GetNumberOfItems();

  // Returns the index of the active profile.
  size_t GetActiveProfileIndex();

  // Gets the an Item at a specified index.
  const Item& GetItemAt(size_t index);

  // Returns true if the add profile link should be shown.
  bool ShouldShowAddNewProfileLink() const;

  // This model is also used for the always-present Mac system menubar. As the
  // last active browser changes, the model needs to update accordingly.
  void set_browser(Browser* browser) { browser_ = browser; }

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // True if avatar menu should be displayed.
  static bool ShouldShowAvatarMenu();

 private:
  // Rebuilds the menu from the cache and notifies the |observer_|.
  void RebuildMenu();

  // Clears the list of items, deleting each.
  void ClearMenu();

  // The cache that provides the profile information. Weak.
  ProfileInfoInterface* profile_info_;

  // The observer of this model, which is notified of changes. Weak.
  AvatarMenuModelObserver* observer_;

  // Browser in which this avatar menu resides. Weak.
  Browser* browser_;

  // List of built "menu items."
  std::vector<Item*> items_;

  // Listens for notifications from the ProfileInfoCache.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(AvatarMenuModel);
};

#endif  // CHROME_BROWSER_PROFILES_AVATAR_MENU_MODEL_H_
