// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bubble/bubble_controller.h"

#include "components/bubble/bubble_delegate.h"
#include "components/bubble/bubble_manager.h"
#include "components/bubble/bubble_ui.h"

BubbleController::BubbleController(BubbleManager* manager,
                                   scoped_ptr<BubbleDelegate> delegate)
    : manager_(manager), delegate_(delegate.Pass()) {
  DCHECK(manager_);
  DCHECK(delegate_);
}

BubbleController::~BubbleController() {
  if (bubble_ui_)
    ShouldClose(BUBBLE_CLOSE_FORCED);
}

bool BubbleController::CloseBubble(BubbleCloseReason reason) {
  return manager_->CloseBubble(this->AsWeakPtr(), reason);
}

void BubbleController::Show() {
  DCHECK(!bubble_ui_);
  bubble_ui_ = delegate_->BuildBubbleUI();
  DCHECK(bubble_ui_);
  bubble_ui_->Show();
  // TODO(hcarmona): log that bubble was shown.
}

void BubbleController::UpdateAnchorPosition() {
  DCHECK(bubble_ui_);
  bubble_ui_->UpdateAnchorPosition();
}

bool BubbleController::ShouldClose(BubbleCloseReason reason) {
  DCHECK(bubble_ui_);
  if (delegate_->ShouldClose(reason) || reason == BUBBLE_CLOSE_FORCED) {
    bubble_ui_->Close();
    bubble_ui_.reset();
    // TODO(hcarmona): log that bubble was hidden.
    return true;
  }
  return false;
}
