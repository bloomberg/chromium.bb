// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_WEBSITE_SETTINGS_PERMISSION_SELECTOR_OBSERVER_H_
#define CHROME_BROWSER_UI_GTK_WEBSITE_SETTINGS_PERMISSION_SELECTOR_OBSERVER_H_

class PermissionSelector;

class PermissionSelectorObserver {
 public:
  // This method is called when a permission setting is changed by a
  // |selector|.
  virtual void OnPermissionChanged(PermissionSelector* selector) = 0;

  // GtkComboBox grabs the keyboard and pointer when it displays its popup,
  // which steals the grabs that BubbleGtk had installed. When the combobox
  // popup is hidden, we notify BubbleGtk so it can try to reacquire the grabs
  // (otherwise, GTK won't activate our widgets when the user clicks in them).
  // OnComboboxShown is called when a combobox popup is closed.
  virtual void OnComboboxShown() = 0;

 protected:
  virtual ~PermissionSelectorObserver() {}
};

#endif  // CHROME_BROWSER_UI_GTK_WEBSITE_SETTINGS_PERMISSION_SELECTOR_OBSERVER_H_
