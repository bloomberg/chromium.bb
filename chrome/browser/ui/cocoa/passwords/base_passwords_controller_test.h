// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PASSWORDS_BASE_PASSWORDS_CONTROLLER_TEST_H_
#define CHROME_BROWSER_UI_COCOA_PASSWORDS_BASE_PASSWORDS_CONTROLLER_TEST_H_

#include <memory>

#include "base/mac/scoped_nsobject.h"
#include "base/strings/string_piece_forward.h"
#import "chrome/browser/ui/cocoa/passwords/base_passwords_content_view_controller.h"
#include "chrome/browser/ui/cocoa/test/cocoa_profile_test.h"
#include "chrome/browser/ui/cocoa/test/cocoa_test_helper.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate_mock.h"

namespace content {
class WebContents;
}  // namespace content
class PasswordsModelDelegateMock;

// Helper delegate for testing the views of the password management bubble.
@interface ContentViewDelegateMock : NSObject<BasePasswordsContentViewDelegate>
@property(nonatomic) ManagePasswordsBubbleModel* model;
@property(readonly, nonatomic) BOOL dismissed;
@end

class ManagePasswordsControllerTest : public CocoaProfileTest {
 public:
  using VectorConstFormPtr =
      std::vector<std::unique_ptr<autofill::PasswordForm>>;

  ManagePasswordsControllerTest();
  ~ManagePasswordsControllerTest() override;
  void SetUp() override;

  PasswordsModelDelegateMock* ui_controller() {
    return ui_controller_.get();
  }
  ContentViewDelegateMock* delegate() { return delegate_.get(); }
  ManagePasswordsBubbleModel* GetModelAndCreateIfNull();

  // Sets the appropriate state for ManagePasswordsBubbleModel.
  void SetUpSavePendingState();
  void SetUpSavePendingState(
      base::StringPiece username,
      base::StringPiece password,
      const std::vector<base::StringPiece>& other_passwords);
  void SetUpUpdatePendingState(bool multiple_forms);
  void SetUpConfirmationState();
  void SetUpManageState(const VectorConstFormPtr& forms);

  // An opportunity for tests to override the constructor parameter of
  // ManagePasswordsBubbleModel.
  virtual ManagePasswordsBubbleModel::DisplayReason GetDisplayReason() const;

 private:
  std::unique_ptr<PasswordsModelDelegateMock> ui_controller_;
  std::unique_ptr<content::WebContents> test_web_contents_;
  std::unique_ptr<ManagePasswordsBubbleModel> model_;
  base::scoped_nsobject<ContentViewDelegateMock> delegate_;

  DISALLOW_COPY_AND_ASSIGN(ManagePasswordsControllerTest);
};

#endif // CHROME_BROWSER_UI_COCOA_PASSWORDS_BASE_PASSWORDS_CONTROLLER_TEST_H_
