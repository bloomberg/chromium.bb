// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_ACTION_BOX_CONTROLLER_H_
#define CHROME_BROWSER_EXTENSIONS_ACTION_BOX_CONTROLLER_H_
#pragma once

#include <set>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"

class ExtensionAction;

namespace extensions {

// Controller of the "badges" (aka "page actions") in the UI.
class ActionBoxController {
 public:
  // The reaction that the UI should take after executing |OnClicked|.
  enum Action {
    ACTION_NONE,
    ACTION_SHOW_POPUP,
    ACTION_SHOW_CONTEXT_MENU,
  };

  virtual ~ActionBoxController() {}

  // Utility to add any actions to |out| which aren't present in |actions|.
  static void AddMissingActions(
      const std::set<ExtensionAction*>& actions,
      std::vector<ExtensionAction*>* out);

  // Gets the action data for all extensions.
  virtual scoped_ptr<std::vector<ExtensionAction*> > GetCurrentActions() = 0;

  // Notifies this that the badge for an extension has been clicked with some
  // mouse button (1 for left, 2 for middle, and 3 for right click), and
  // returns the action that should be taken in response (if any).
  virtual Action OnClicked(const std::string& extension_id,
                           int mouse_button) = 0;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_ACTION_BOX_CONTROLLER_H_
