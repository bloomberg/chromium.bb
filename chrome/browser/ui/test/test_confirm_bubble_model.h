// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TEST_TEST_CONFIRM_BUBBLE_MODEL_H_
#define CHROME_BROWSER_UI_TEST_TEST_CONFIRM_BUBBLE_MODEL_H_

#include "base/compiler_specific.h"
#include "chrome/browser/ui/confirm_bubble_model.h"

// A test version of the model for confirmation bubbles.
class TestConfirmBubbleModel : public ConfirmBubbleModel {
 public:
  // Parameters may be NULL depending on the needs of the test.
  TestConfirmBubbleModel(bool* model_deleted,
                         bool* accept_clicked,
                         bool* cancel_clicked,
                         bool* link_clicked);
  virtual ~TestConfirmBubbleModel();

  // ConfirmBubbleModel overrides:
  virtual base::string16 GetTitle() const OVERRIDE;
  virtual base::string16 GetMessageText() const OVERRIDE;
  virtual gfx::Image* GetIcon() const OVERRIDE;
  virtual int GetButtons() const OVERRIDE;
  virtual base::string16 GetButtonLabel(BubbleButton button) const OVERRIDE;
  virtual void Accept() OVERRIDE;
  virtual void Cancel() OVERRIDE;
  virtual base::string16 GetLinkText() const OVERRIDE;
  virtual void LinkClicked() OVERRIDE;

 private:
  bool* model_deleted_;
  bool* accept_clicked_;
  bool* cancel_clicked_;
  bool* link_clicked_;

  DISALLOW_COPY_AND_ASSIGN(TestConfirmBubbleModel);
};

#endif  // CHROME_BROWSER_UI_TEST_TEST_CONFIRM_BUBBLE_MODEL_H_
