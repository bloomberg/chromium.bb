// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_MODEL_H_
#define CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_MODEL_H_
#pragma once

#include <string>

#include "base/basictypes.h"

class Browser;
class NavigationController;

// This class is the model used by the toolbar, location bar and autocomplete
// edit.  It populates its states from the current navigation entry retrieved
// from the navigation controller returned by GetNavigationController().
class ToolbarModel {
 public:
  // TODO(wtc): unify ToolbarModel::SecurityLevel with SecurityStyle.  We
  // don't need two sets of security UI levels.  SECURITY_STYLE_AUTHENTICATED
  // needs to be refined into three levels: warning, standard, and EV.
  enum SecurityLevel {
    NONE = 0,          // HTTP/no URL/user is editing
    EV_SECURE,         // HTTPS with valid EV cert
    SECURE,            // HTTPS (non-EV)
    SECURITY_WARNING,  // HTTPS, but unable to check certificate revocation
                       // status or with insecure content on the page
    SECURITY_ERROR,    // Attempted HTTPS and failed, page not authenticated
    NUM_SECURITY_LEVELS,
  };

  explicit ToolbarModel(Browser* browser);
  ~ToolbarModel();

  // Returns the text that should be displayed in the location bar.
  std::wstring GetText() const;

  // Returns the security level that the toolbar should display.
  SecurityLevel GetSecurityLevel() const;

  // Returns the resource_id of the icon to show to the left of the address,
  // based on the current URL.  This doesn't cover specialized icons while the
  // user is editing; see AutocompleteEditView::GetIcon().
  int GetIcon() const;

  // Returns the name of the EV cert holder.  Only call this when the security
  // level is EV_SECURE.
  std::wstring GetEVCertName() const;

  // Getter/setter of whether the text in location bar is currently being
  // edited.
  void set_input_in_progress(bool value) { input_in_progress_ = value; }
  bool input_in_progress() const { return input_in_progress_; }

 private:
  // Returns the navigation controller used to retrieve the navigation entry
  // from which the states are retrieved.
  // If this returns NULL, default values are used.
  NavigationController* GetNavigationController() const;

  Browser* browser_;

  // Whether the text in the location bar is currently being edited.
  bool input_in_progress_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ToolbarModel);
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_MODEL_H_
