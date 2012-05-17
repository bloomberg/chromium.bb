// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/action_box_controller.h"

#include <algorithm>

namespace extensions {

// static
void ActionBoxController::AddMissingActions(
    const std::set<ExtensionAction*>& actions,
    std::vector<ExtensionAction*>* out) {
  for (std::set<ExtensionAction*>::const_iterator it = actions.begin();
       it != actions.end(); ++it) {
    ExtensionAction* action = (*it);
    if (!std::count(out->begin(), out->end(), action))
      out->push_back(action);
  }
}

}  // namespace extensions
