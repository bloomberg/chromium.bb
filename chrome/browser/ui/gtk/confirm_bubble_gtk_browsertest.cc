// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/confirm_bubble_gtk.h"

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/confirm_bubble_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

// The model object used in this test. This model implements all methods and
// updates its status when ConfirmBubbleGtk calls its methods.
class TestConfirmBubbleModel : public ConfirmBubbleModel {
 public:
  TestConfirmBubbleModel(bool* model_deleted,
                         bool* accept_clicked,
                         bool* cancel_clicked,
                         bool* link_clicked);
  virtual ~TestConfirmBubbleModel() OVERRIDE;
  virtual string16 GetTitle() const OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;
  virtual gfx::Image* GetIcon() const OVERRIDE;
  virtual int GetButtons() const OVERRIDE;
  virtual string16 GetButtonLabel(BubbleButton button) const OVERRIDE;
  virtual void Accept() OVERRIDE;
  virtual void Cancel() OVERRIDE;
  virtual string16 GetLinkText() const OVERRIDE;
  virtual void LinkClicked() OVERRIDE;

 private:
  bool* model_deleted_;
  bool* accept_clicked_;
  bool* cancel_clicked_;
  bool* link_clicked_;
};

TestConfirmBubbleModel::TestConfirmBubbleModel(bool* model_deleted,
                                               bool* accept_clicked,
                                               bool* cancel_clicked,
                                               bool* link_clicked)
    : model_deleted_(model_deleted),
      accept_clicked_(accept_clicked),
      cancel_clicked_(cancel_clicked),
      link_clicked_(link_clicked) {
}

TestConfirmBubbleModel::~TestConfirmBubbleModel() {
  *model_deleted_ = true;
}

string16 TestConfirmBubbleModel::GetTitle() const {
  return ASCIIToUTF16("Test");
}

string16 TestConfirmBubbleModel::GetMessageText() const {
  return ASCIIToUTF16("Test Message");
}

gfx::Image* TestConfirmBubbleModel::GetIcon() const {
  return &ResourceBundle::GetSharedInstance().GetImageNamed(
      IDR_PRODUCT_LOGO_16);
}

int TestConfirmBubbleModel::GetButtons() const {
  return BUTTON_OK | BUTTON_CANCEL;
}

string16 TestConfirmBubbleModel::GetButtonLabel(BubbleButton button) const {
  return button == BUTTON_OK ? ASCIIToUTF16("OK") : ASCIIToUTF16("Cancel");
}

void TestConfirmBubbleModel::Accept() {
  *accept_clicked_ = true;
}

void TestConfirmBubbleModel::Cancel() {
  *cancel_clicked_ = true;
}

string16 TestConfirmBubbleModel::GetLinkText() const {
  return ASCIIToUTF16("Link");
}

void TestConfirmBubbleModel::LinkClicked() {
  *link_clicked_ = true;
}

}  // namespace

class ConfirmBubbleGtkTest : public InProcessBrowserTest {
 public:
  ConfirmBubbleGtkTest()
      : bubble_(NULL),
        model_deleted_(false),
        accept_clicked_(false),
        cancel_clicked_(false),
        link_clicked_(false) {
  }

  void ShowBubble() {
    model_deleted_ = false;
    accept_clicked_ = false;
    cancel_clicked_ = false;
    link_clicked_ = false;

    gfx::Point point(0, 0);
    bubble_ = new ConfirmBubbleGtk(
        GTK_WIDGET(browser()->window()->GetNativeWindow()),
        point,
        new TestConfirmBubbleModel(&model_deleted_,
                                   &accept_clicked_,
                                   &cancel_clicked_,
                                   &link_clicked_));
    bubble_->Show();
  }

  ConfirmBubbleGtk* bubble() const { return bubble_; }

  bool model_deleted() const {
    return model_deleted_;
  }

  bool accept_clicked() const {
    return accept_clicked_;
  }

  bool cancel_clicked() const {
    return cancel_clicked_;
  }

  bool link_clicked() const {
    return link_clicked_;
  }

 private:
  ConfirmBubbleGtk* bubble_;
  bool model_deleted_;
  bool accept_clicked_;
  bool cancel_clicked_;
  bool link_clicked_;
};

// Verifies clicking a button or a link calls an appropriate model method and
// deletes the model.
IN_PROC_BROWSER_TEST_F(ConfirmBubbleGtkTest, ClickCancel) {
  ShowBubble();
  bubble()->OnCancelButton(NULL);

  EXPECT_TRUE(model_deleted());
  EXPECT_FALSE(accept_clicked());
  EXPECT_TRUE(cancel_clicked());
  EXPECT_FALSE(link_clicked());
}

IN_PROC_BROWSER_TEST_F(ConfirmBubbleGtkTest, ClickLink) {
  ShowBubble();
  bubble()->OnLinkButton(NULL);

  EXPECT_TRUE(model_deleted());
  EXPECT_FALSE(accept_clicked());
  EXPECT_FALSE(cancel_clicked());
  EXPECT_TRUE(link_clicked());
}

IN_PROC_BROWSER_TEST_F(ConfirmBubbleGtkTest, ClickOk) {
  ShowBubble();
  bubble()->OnOkButton(NULL);

  EXPECT_TRUE(model_deleted());
  EXPECT_TRUE(accept_clicked());
  EXPECT_FALSE(cancel_clicked());
  EXPECT_FALSE(link_clicked());
}
