// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOCHECKOUT_BUBBLE_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOCHECKOUT_BUBBLE_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"

namespace autofill {

class AutocheckoutBubbleController;

class AutocheckoutBubble : public base::SupportsWeakPtr<AutocheckoutBubble> {
 public:
  virtual ~AutocheckoutBubble();

  // Show the Autocheckout bubble.
  virtual void ShowBubble() = 0;

  // Hide the Autocheckout bubble.
  virtual void HideBubble() = 0;

  // Creates the Autocheckout bubble. The returned bubble owns itself.
  static base::WeakPtr<AutocheckoutBubble> Create(
      scoped_ptr<AutocheckoutBubbleController> controller);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOCHECKOUT_BUBBLE_H_
