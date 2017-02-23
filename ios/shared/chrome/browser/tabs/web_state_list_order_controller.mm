// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/shared/chrome/browser/tabs/web_state_list_order_controller.h"

#include <cstdint>

#include "base/logging.h"
#import "ios/shared/chrome/browser/tabs/web_state_list.h"

WebStateListOrderController::WebStateListOrderController(
    WebStateList* web_state_list)
    : web_state_list_(web_state_list) {
  DCHECK(web_state_list_);
}

WebStateListOrderController::~WebStateListOrderController() = default;

int WebStateListOrderController::DetermineInsertionIndex(
    ui::PageTransition transition,
    web::WebState* opener) const {
  if (!opener)
    return web_state_list_->count();

  if (!PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_LINK))
    return web_state_list_->count();

  int opener_index = web_state_list_->GetIndexOfWebState(opener);
  DCHECK_NE(WebStateList::kInvalidIndex, opener_index);

  int list_child_index = web_state_list_->GetIndexOfLastWebStateOpenedBy(
      opener, opener_index, true);

  int reference_index = list_child_index != WebStateList::kInvalidIndex
                            ? list_child_index
                            : opener_index;

  // Check for overflows (just a DCHECK as INT_MAX open WebState is unlikely).
  DCHECK_LT(reference_index, INT_MAX);
  return reference_index + 1;
}
