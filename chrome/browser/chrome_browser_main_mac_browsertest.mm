// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/test_utils.h"
#import "testing/gtest_mac.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_handle.h"
#include "ui/base/resource/scale_factor.h"
#include "ui/base/ui_base_switches.h"

namespace ui {

class ChromeBrowserMainMacBrowserTest : public InProcessBrowserTest {
 public:
  ChromeBrowserMainMacBrowserTest() {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Activate Material Design mode by placing the MD switch on the command
    // line.
    command_line->AppendSwitchASCII(switches::kTopChromeMD,
                                    switches::kTopChromeMDMaterial);
  }
};

// Confirm that the Material Design resource packs are loaded and are the
// first packs in the vector (so that they are searched before the regular non-
// MD packs). TODO(shrike) - remove this test once Material Design replaces
// the old design.
IN_PROC_BROWSER_TEST_F(ChromeBrowserMainMacBrowserTest, MDResourceAccess) {
  // Make sure we're running in Material Design mode.
  ASSERT_TRUE(MaterialDesignController::IsModeMaterial());

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  int counter = 0;
  for (const auto& pack : rb.data_packs_) {
    switch (counter) {
      case 0:
        // The first pack should be Material Design at 1x.
        EXPECT_TRUE(pack->HasOnlyMaterialDesignAssets());
        EXPECT_EQ(SCALE_FACTOR_100P, pack->GetScaleFactor());
        break;

      case 1:
        // The Second pack should be Material Design at 2x.
        EXPECT_TRUE(pack->HasOnlyMaterialDesignAssets());
        EXPECT_EQ(SCALE_FACTOR_200P, pack->GetScaleFactor());
        break;

      default:
        // All other packs should not be Material Design.
        EXPECT_FALSE(pack->HasOnlyMaterialDesignAssets());
        break;
    }
    counter++;
  }
}

}  // namespace ui
