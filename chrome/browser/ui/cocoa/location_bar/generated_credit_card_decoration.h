// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_LOCATION_BAR_GENERATED_CREDIT_CARD_DECORATION_H_
#define CHROME_BROWSER_UI_COCOA_LOCATION_BAR_GENERATED_CREDIT_CARD_DECORATION_H_

#import <Cocoa/Cocoa.h>

#include "base/macros.h"
#include "chrome/browser/ui/cocoa/location_bar/image_decoration.h"

class LocationBarViewMac;

namespace autofill {
class GeneratedCreditCardBubbleController;
}

// An icon that shows up in the omnibox after successfully generating a credit
// card number. Used as an anchor and click target to show the associated
// bubble with more details about the credit cards saved or used.
class GeneratedCreditCardDecoration : public ImageDecoration {
 public:
  explicit GeneratedCreditCardDecoration(LocationBarViewMac* owner);
  ~GeneratedCreditCardDecoration() override;

  // Called when this decoration should update its visible status.
  void Update();

  // Implement |LocationBarDecoration|.
  bool AcceptsMousePress() override;
  bool OnMousePressed(NSRect frame, NSPoint location) override;
  NSPoint GetBubblePointInFrame(NSRect frame) override;

 private:
  // Helper to get the GeneratedCreditCardBubbleController associated with the
  // current web contents.
  autofill::GeneratedCreditCardBubbleController* GetController() const;

  // The control that owns this. Weak.
  LocationBarViewMac* owner_;

  DISALLOW_COPY_AND_ASSIGN(GeneratedCreditCardDecoration);
};

#endif  // CHROME_BROWSER_UI_COCOA_LOCATION_BAR_GENERATED_CREDIT_CARD_DECORATION_H_

