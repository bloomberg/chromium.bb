// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/mock_save_card_bubble_controller.h"

namespace autofill {

MockSaveCardBubbleController::MockSaveCardBubbleController() {}

MockSaveCardBubbleController::~MockSaveCardBubbleController() {}

base::string16 MockSaveCardBubbleController::GetCvcEnteredByUser() const {
  return cvc_entered_by_user_;
}

void MockSaveCardBubbleController::OnSaveButton(const base::string16& cvc) {
  if (!cvc.empty())
    cvc_entered_by_user_ = cvc;
}

}  // namespace autofill
