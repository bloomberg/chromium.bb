// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_EXTENSIONS_WINDOW_CONTROLLER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/common/extensions/api/tabs.h"
#include "chrome/common/extensions/api/windows.h"

class Browser;  // TODO(stevenjb) eliminate this dependency.
class GURL;
class Profile;

namespace base {
class DictionaryValue;
}

namespace ui {
class BaseWindow;
}

namespace extensions {
class Extension;

// This API needs to be implemented by any window that might be accessed
// through chrome.windows or chrome.tabs (e.g. browser windows and panels).
// Subclasses must add/remove themselves from the WindowControllerList
// upon construction/destruction.
class WindowController {
 public:
  enum Reason {
    REASON_NONE,
    REASON_NOT_EDITABLE,
  };

  // A bitmaks used as filter on window types.
  using TypeFilter = uint32_t;

  // Represents the lack of any window filter, implying
  // IsVisibleToExtension will be used as non-filtered behavior.
  static const TypeFilter kNoWindowFilter = 0;

  // Returns a filter allowing all window types to be manipulated
  // through the chrome.windows APIs.
  static TypeFilter GetAllWindowFilter();

  // Builds a filter out of a vector of window types.
  static TypeFilter GetFilterFromWindowTypes(
      const std::vector<api::windows::WindowType>& types);

  static TypeFilter GetFilterFromWindowTypesValues(
      const base::ListValue* types);

  WindowController(ui::BaseWindow* window, Profile* profile);
  virtual ~WindowController();

  ui::BaseWindow* window() const { return window_; }

  Profile* profile() const { return profile_; }

  // Return an id uniquely identifying the window.
  virtual int GetWindowId() const = 0;

  // Return the type name for the window.
  virtual std::string GetWindowTypeText() const = 0;

  // Populates a dictionary for the Window object. Override this to set
  // implementation specific properties (call the base implementation first to
  // set common properties).
  std::unique_ptr<base::DictionaryValue> CreateWindowValue() const;

  // Populates a dictionary for the Window object, including a list of tabs.
  virtual std::unique_ptr<base::DictionaryValue> CreateWindowValueWithTabs(
      const extensions::Extension* extension) const = 0;

  virtual std::unique_ptr<api::tabs::Tab> CreateTabObject(
      const extensions::Extension* extension,
      int tab_index) const = 0;

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

  // Extension/window visibility and ownership is window-specific, subclasses
  // need to define this behavior.
  virtual bool IsVisibleToExtension(const Extension* extension) const = 0;

  // Returns true if the window type of the controller matches the |filter|.
  bool MatchesFilter(TypeFilter filter) const;

 private:
  ui::BaseWindow* window_;
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(WindowController);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_WINDOW_CONTROLLER_H_
