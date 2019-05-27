// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_CREDIT_CARD_ACCESSORY_CONTROLLER_H_
#define CHROME_BROWSER_AUTOFILL_CREDIT_CARD_ACCESSORY_CONTROLLER_H_

#include "chrome/browser/autofill/accessory_controller.h"

namespace content {
class WebContents;
}

namespace autofill {

// Interface for credit card-specific keyboard accessory controller between the
// ManualFillingController and Autofill backend logic.
class CreditCardAccessoryController : public AccessoryController {
 public:
  CreditCardAccessoryController() = default;
  ~CreditCardAccessoryController() override = default;

  // Returns true if the accessory controller may exist for |web_contents|.
  // Otherwise it returns false.
  static bool AllowedForWebContents(content::WebContents* web_contents);

  // Returns a reference to the unique controller associated with
  // |web_contents|. A new instance is created if the first time this function
  // is called. Should only be called if AllowedForWebContents returns true for
  // |web_contents|.
  static CreditCardAccessoryController* GetOrCreate(
      content::WebContents* web_contents);

  // Returns a reference to the unique controller associated with
  // |web_contents|. Returns null if no such instance exists.
  static CreditCardAccessoryController* GetIfExisting(
      content::WebContents* web_contents);

  // Fetches suggestions and propagates to the frontend.
  virtual void RefreshSuggestions() = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_CREDIT_CARD_ACCESSORY_CONTROLLER_H_
