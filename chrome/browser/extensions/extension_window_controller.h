// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_WINDOW_CONTROLLER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"

class BaseWindow;
class Browser;  // TODO(stevenjb) eliminate this dependency.
class GURL;
class Profile;
class SessionID;

namespace base {
class DictionaryValue;
}

namespace gfx {
class Rect;
}

// This API needs to be implemented by any window that might be accessed
// through chrome.windows or chrome.tabs (e.g. browser windows and panels).
class ExtensionWindowController {
 public:
  enum Reason {
    REASON_NONE,
    REASON_NOT_EDITABLE,
  };

  enum ProfileMatchType {
    MATCH_NORMAL_ONLY,
    MATCH_INCOGNITO
  };

  ExtensionWindowController(BaseWindow* window, Profile* profile);
  virtual ~ExtensionWindowController();

  BaseWindow* window() const { return window_; }

  Profile* profile() const { return profile_; }

  // Returns true if the window matches the profile.
  bool MatchesProfile(Profile* profile, ProfileMatchType match_type) const;

  // Return an id uniquely identifying the window.
  virtual int GetWindowId() const = 0;

  // Return the type name for the window.
  virtual std::string GetWindowTypeText() const = 0;

  // Populates a dictionary for the Window object. Override this to set
  // implementation specific properties (call the base implementation first to
  // set common properties).
  virtual base::DictionaryValue* CreateWindowValue() const;

  // Populates a dictionary for the Window object, including a list of tabs.
  virtual base::DictionaryValue* CreateWindowValueWithTabs() const = 0;

  // Returns false if the window is in a state where closing the window is not
  // permitted and sets |reason| if not NULL.
  virtual bool CanClose(Reason* reason) const = 0;

  // Set the window's fullscreen state. |extension_url| provides the url
  // associated with the extension (used by FullscreenController).
  virtual void SetFullscreenMode(bool is_fullscreen,
                                 const GURL& extension_url) const = 0;

  // Returns a Browser if available. Defaults to returning NULL.
  // TODO(stevenjb): Temporary workaround. Eliminate this.
  virtual Browser* GetBrowser() const;

 private:
  BaseWindow* window_;
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionWindowController);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_WINDOW_CONTROLLER_H_
