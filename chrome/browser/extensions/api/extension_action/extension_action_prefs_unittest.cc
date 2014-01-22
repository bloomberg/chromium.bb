// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/./extension_prefs_unittest.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "extensions/common/extension.h"

namespace extensions {

// Tests force hiding browser actions.
class ExtensionPrefsHidingBrowserActions : public ExtensionPrefsTest {
 public:
  virtual void Initialize() OVERRIDE {
    // Install 5 extensions.
    for (int i = 0; i < 5; i++) {
      std::string name = "test" + base::IntToString(i);
      extensions_.push_back(prefs_.AddExtension(name));
    }

    ExtensionList::const_iterator iter;
    for (iter = extensions_.begin(); iter != extensions_.end(); ++iter) {
      EXPECT_TRUE(ExtensionActionAPI::GetBrowserActionVisibility(
          prefs(), (*iter)->id()));
    }

    ExtensionActionAPI::SetBrowserActionVisibility(
        prefs(), extensions_[0]->id(), false);
    ExtensionActionAPI::SetBrowserActionVisibility(
        prefs(), extensions_[1]->id(), true);
  }

  virtual void Verify() OVERRIDE {
    // Make sure the one we hid is hidden.
    EXPECT_FALSE(ExtensionActionAPI::GetBrowserActionVisibility(
        prefs(), extensions_[0]->id()));

    // Make sure the other id's are not hidden.
    ExtensionList::const_iterator iter = extensions_.begin() + 1;
    for (; iter != extensions_.end(); ++iter) {
      SCOPED_TRACE(base::StringPrintf("Loop %d ",
                       static_cast<int>(iter - extensions_.begin())));
      EXPECT_TRUE(ExtensionActionAPI::GetBrowserActionVisibility(
          prefs(), (*iter)->id()));
    }
  }

 private:
  ExtensionList extensions_;
};

TEST_F(ExtensionPrefsHidingBrowserActions, ForceHide) {}

}  // namespace extensions
