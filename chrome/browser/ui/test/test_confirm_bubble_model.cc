// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/test/test_confirm_bubble_model.h"

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"

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
  if (model_deleted_)
    *model_deleted_ = true;
}

base::string16 TestConfirmBubbleModel::GetTitle() const {
  return base::ASCIIToUTF16("Test");
}

base::string16 TestConfirmBubbleModel::GetMessageText() const {
  return base::ASCIIToUTF16("Test Message");
}

gfx::Image* TestConfirmBubbleModel::GetIcon() const {
  // Return an arbitrary non-empty image.
  return &ui::ResourceBundle::GetSharedInstance().GetImageNamed(
      IDR_PRODUCT_LOGO_16);
}

int TestConfirmBubbleModel::GetButtons() const {
  return BUTTON_OK | BUTTON_CANCEL;
}

base::string16 TestConfirmBubbleModel::GetButtonLabel(
    BubbleButton button) const {
  return button == BUTTON_OK ? base::ASCIIToUTF16("OK")
                             : base::ASCIIToUTF16("Cancel");
}

void TestConfirmBubbleModel::Accept() {
  if (accept_clicked_)
    *accept_clicked_ = true;
}

void TestConfirmBubbleModel::Cancel() {
  if (cancel_clicked_)
    *cancel_clicked_ = true;
}

base::string16 TestConfirmBubbleModel::GetLinkText() const {
  return base::ASCIIToUTF16("Link");
}

void TestConfirmBubbleModel::LinkClicked() {
  if (link_clicked_)
    *link_clicked_ = true;
}
