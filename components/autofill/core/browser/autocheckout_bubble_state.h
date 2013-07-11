// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOCHECKOUT_BUBBLE_STATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOCHECKOUT_BUBBLE_STATE_H_

namespace autofill {

// Specifies the state of Autocheckout bubble.
enum AutocheckoutBubbleState {
  // User clicked on continue.
  AUTOCHECKOUT_BUBBLE_ACCEPTED = 0,
  // User clicked on "No Thanks".
  AUTOCHECKOUT_BUBBLE_CANCELED = 1,
  // Bubble is ignored.
  AUTOCHECKOUT_BUBBLE_IGNORED = 2,
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOCHECKOUT_BUBBLE_STATE_H_
