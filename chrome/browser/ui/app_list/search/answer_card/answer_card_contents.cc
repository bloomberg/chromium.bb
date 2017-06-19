// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/answer_card/answer_card_contents.h"

#include "chrome/browser/ui/app_list/search/answer_card/answer_card_result.h"

namespace app_list {

AnswerCardContents::AnswerCardContents() {}

AnswerCardContents::~AnswerCardContents() {
  for (auto& result : results_)
    result.OnContentsDestroying();
}

void AnswerCardContents::SetDelegate(Delegate* delegate) {
  DCHECK(delegate);
  DCHECK(!delegate_);
  delegate_ = delegate;
}

void AnswerCardContents::RegisterResult(AnswerCardResult* result) {
  results_.AddObserver(result);
  if (mouse_is_in_view_ != result->is_mouse_in_view())
    result->SetIsMouseInView(mouse_is_in_view_);
}

void AnswerCardContents::UnregisterResult(AnswerCardResult* result) {
  results_.RemoveObserver(result);
}

void AnswerCardContents::SetIsMouseInView(bool mouse_is_inside) {
  if (mouse_is_inside == mouse_is_in_view_)
    return;

  mouse_is_in_view_ = mouse_is_inside;

  for (auto& result : results_)
    result.SetIsMouseInView(mouse_is_inside);
}

}  // namespace app_list
