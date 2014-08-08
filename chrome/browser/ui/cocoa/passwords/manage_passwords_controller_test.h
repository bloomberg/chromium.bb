// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PASSWORDS_MANAGE_PASSWORDS_CONTROLLER_TEST_H_
#define CHROME_BROWSER_UI_COCOA_PASSWORDS_MANAGE_PASSWORDS_CONTROLLER_TEST_H_

#include "chrome/browser/ui/cocoa/cocoa_profile_test.h"
#include "chrome/browser/ui/cocoa/cocoa_test_helper.h"

namespace content {
class WebContents;
}  // namespace content

class ManagePasswordsUIControllerMock;
class ManagePasswordsBubbleModel;

class ManagePasswordsControllerTest : public CocoaProfileTest {
 public:
  ManagePasswordsControllerTest();
  virtual ~ManagePasswordsControllerTest();
  virtual void SetUp() OVERRIDE;

  ManagePasswordsUIControllerMock* ui_controller() { return ui_controller_; }
  ManagePasswordsBubbleModel* model();

 private:
  ManagePasswordsUIControllerMock* ui_controller_;
  scoped_ptr<content::WebContents> test_web_contents_;
  scoped_ptr<ManagePasswordsBubbleModel> model_;
};

#endif // CHROME_BROWSER_UI_COCOA_PASSWORDS_MANAGE_PASSWORDS_CONTROLLER_TEST_H_
