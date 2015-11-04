// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_SAVE_CARD_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_SAVE_CARD_BUBBLE_CONTROLLER_H_

#include "base/macros.h"

namespace autofill {

class SaveCardBubbleView;

// Interface that exposes controller functionality to SaveCardBubbleView.
class SaveCardBubbleController {
 public:
  virtual void OnSaveButton() = 0;
  virtual void OnCancelButton() = 0;
  virtual void OnLearnMoreClicked() = 0;
  virtual void OnBubbleClosed() = 0;

 protected:
  SaveCardBubbleController() {}
  virtual ~SaveCardBubbleController() {}

  DISALLOW_COPY_AND_ASSIGN(SaveCardBubbleController);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_SAVE_CARD_BUBBLE_CONTROLLER_H_
