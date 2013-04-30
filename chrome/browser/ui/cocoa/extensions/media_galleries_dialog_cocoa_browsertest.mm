// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/extensions/media_galleries_dialog_cocoa.h"

#include "chrome/browser/media_galleries/media_galleries_dialog_controller_mock.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/cocoa/constrained_window/constrained_window_alert.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_contents_modal_dialog_manager.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;

namespace chrome {

class MediaGalleriesDialogBrowserTest : public InProcessBrowserTest {
};

// Verify that programatically closing the constrained window correctly closes
// the sheet.
IN_PROC_BROWSER_TEST_F(MediaGalleriesDialogBrowserTest, Close) {
  NiceMock<MediaGalleriesDialogControllerMock> controller;

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_CALL(controller, web_contents()).
      WillRepeatedly(Return(web_contents));

  MediaGalleriesDialogController::GalleryPermissionsVector attached_permissions;
  EXPECT_CALL(controller, AttachedPermissions()).
      WillRepeatedly(Return(attached_permissions));
  MediaGalleriesDialogController::GalleryPermissionsVector
      unattached_permissions;
  EXPECT_CALL(controller, UnattachedPermissions()).
      WillRepeatedly(Return(unattached_permissions));

  scoped_ptr<MediaGalleriesDialogCocoa> dialog(
      static_cast<MediaGalleriesDialogCocoa*>(
          MediaGalleriesDialog::Create(&controller)));
  scoped_nsobject<NSWindow> window([[dialog->alert_ window] retain]);
  EXPECT_TRUE([window isVisible]);

  WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      WebContentsModalDialogManager::FromWebContents(web_contents);
  WebContentsModalDialogManager::TestApi test_api(
      web_contents_modal_dialog_manager);
  test_api.CloseAllDialogs();
  EXPECT_FALSE([window isVisible]);
}

}  // namespace chrome
