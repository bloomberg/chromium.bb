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

 protected:
  virtual ~PermissionSelectorObserver() {}
};

#endif  // CHROME_BROWSER_UI_GTK_WEBSITE_SETTINGS_PERMISSION_SELECTOR_OBSERVER_H_
