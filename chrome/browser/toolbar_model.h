// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOOLBAR_MODEL_H__
#define CHROME_BROWSER_TOOLBAR_MODEL_H__

#include <string>

#include "base/basictypes.h"

class Browser;
class NavigationController;
class NavigationEntry;

// This class is the model used by the toolbar, location bar and autocomplete
// edit.  It populates its states from the current navigation entry retrieved
// from the navigation controller returned by GetNavigationController().
class ToolbarModel {
 public:
  enum SecurityLevel {
    NONE = 0,          // HTTP/no URL/user is editing
    EV_SECURE,         // HTTPS with valid EV cert
    SECURE,            // HTTPS (non-EV)
    SECURITY_WARNING,  // HTTPS, but with mixed content on the page
    SECURITY_ERROR,    // Attempted HTTPS and failed, page not authenticated
    NUM_SECURITY_LEVELS,
  };

  explicit ToolbarModel(Browser* browser);
  ~ToolbarModel();

  // Returns the text that should be displayed in the location bar.
  std::wstring GetText() const;

  // Returns the security level that the toolbar should display.
  SecurityLevel GetSecurityLevel() const;

  // Returns the resource_id of the icon to show to the left of the address.
  int GetSecurityIcon() const;

  // Sets the text displayed in the info bubble that appears when the user
  // hovers the mouse over the icon.
  void GetIconHoverText(std::wstring* text) const;

  // Returns the text, if any, that should be displayed on the right of the
  // location bar.
  std::wstring GetSecurityInfoText() const;

  // Getter/setter of whether the text in location bar is currently being
  // edited.
  void set_input_in_progress(bool value) { input_in_progress_ = value; }
  bool input_in_progress() const { return input_in_progress_; }

 private:
  // Returns the navigation controller used to retrieve the navigation entry
  // from which the states are retrieved.
  // If this returns NULL, default values are used.
  NavigationController* GetNavigationController() const;

  // Builds a short error message from the SSL status code found in |entry|.
  // The message is set in |text|.
  void CreateErrorText(NavigationEntry* entry, std::wstring* text) const;

  Browser* browser_;

  // Whether the text in the location bar is currently being edited.
  bool input_in_progress_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ToolbarModel);
};

#endif  // CHROME_BROWSER_TOOLBAR_MODEL_H__
