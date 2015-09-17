// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/translate/translate_bubble_test_utils.h"

#include "base/logging.h"
#include "chrome/browser/ui/translate/translate_bubble_model.h"
#include "chrome/browser/ui/views/translate/translate_bubble_view.h"

namespace translate {

namespace test_utils {

const TranslateBubbleModel* GetCurrentModel(Browser* browser) {
  DCHECK(browser);
  TranslateBubbleView* view = TranslateBubbleView::GetCurrentBubble();
  return view ? view->model() : nullptr;
}

void PressTranslate(Browser* browser) {
  DCHECK(browser);
  TranslateBubbleView* bubble = TranslateBubbleView::GetCurrentBubble();
  DCHECK(bubble);
  bubble->HandleButtonPressed(TranslateBubbleView::BUTTON_ID_TRANSLATE);
}

}  // namespace test_utils

}  // namespace translate
