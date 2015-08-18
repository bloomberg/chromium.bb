// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTION_TEST_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTION_TEST_UTIL_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "extensions/common/manifest.h"

class Profile;
class ToolbarActionsModel;

namespace content {
class WebContents;
}

namespace extensions {
class Extension;

namespace extension_action_test_util {

// The different possible types of actions for an extension to have (we use
// this instead of ActionInfo::Type because we want a "none" option).
enum ActionType {
  NO_ACTION,
  PAGE_ACTION,
  BROWSER_ACTION
};

// TODO(devlin): Should we also pull out methods to test browser actions?

// Returns the number of page actions that are visible in the given
// |web_contents|.
size_t GetVisiblePageActionCount(content::WebContents* web_contents);

// Returns the total number of page actions (visible or not) for the given
// |web_contents|.
size_t GetTotalPageActionCount(content::WebContents* web_contents);

// Creates and returns an extension with the given |name| with the given
// |action_type|.
// Does not add the extension to the extension service or registry.
scoped_refptr<const Extension> CreateActionExtension(const std::string& name,
                                                     ActionType action_type);
scoped_refptr<const Extension> CreateActionExtension(
    const std::string& name,
    ActionType action_type,
    Manifest::Location location);

// Creates a new ToolbarActionsModel for the given |profile|, and associates
// it with the profile as a keyed service.
// This should only be used in unit tests (since it assumes the existence of
// a TestExtensionSystem), but if running a browser test, the model should
// already be created.
ToolbarActionsModel* CreateToolbarModelForProfile(Profile* profile);
// Like above, but doesn't run the ExtensionSystem::ready() task for the new
// model.
ToolbarActionsModel* CreateToolbarModelForProfileWithoutWaitingForReady(
    Profile* profile);

}  // namespace extension_action_test_util
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTION_TEST_UTIL_H_
