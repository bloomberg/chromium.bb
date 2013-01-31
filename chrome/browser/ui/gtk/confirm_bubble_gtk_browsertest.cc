// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/confirm_bubble_gtk.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/test/test_confirm_bubble_model.h"
#include "chrome/test/base/in_process_browser_test.h"

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
