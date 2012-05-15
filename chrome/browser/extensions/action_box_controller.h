// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_ACTION_BOX_CONTROLLER_H_
#define CHROME_BROWSER_EXTENSIONS_ACTION_BOX_CONTROLLER_H_
#pragma once

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"

class ExtensionAction;

namespace extensions {

// Controller of the "badges" (aka "page actions") in the UI.
class ActionBoxController {
 public:
  // UI decoration on a page box item.
  enum Decoration {
    DECORATION_NONE,
  };

  // Data about a UI badge.
  struct Data {
    // The type of decoration that should be applied to the badge.
    Decoration decoration;

    // The ExtensionAction that corresponds to the badge.
    ExtensionAction* action;
  };

  typedef std::vector<Data> DataList;

  // The reaction that the UI should take after executing |OnClicked|.
  enum Action {
    ACTION_NONE,
    ACTION_SHOW_POPUP,
    ACTION_SHOW_CONTEXT_MENU,
  };

  virtual ~ActionBoxController() {}

  // Gets the badge data for all extensions.
  virtual scoped_ptr<DataList> GetAllBadgeData() = 0;

  // Notifies this that the badge for an extension has been clicked with some
  // mouse button (1 for left, 2 for middle, and 3 for right click), and
  // returns the action that should be taken in response (if any).
  virtual Action OnClicked(const std::string& extension_id,
                           int mouse_button) = 0;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_ACTION_BOX_CONTROLLER_H_
