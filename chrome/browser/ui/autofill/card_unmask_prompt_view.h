// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_CARD_UNMASK_PROMPT_VIEW_H_
#define CHROME_BROWSER_UI_AUTOFILL_CARD_UNMASK_PROMPT_VIEW_H_

#include "base/callback.h"
#include "base/strings/string16.h"

namespace content {
class WebContents;
}

namespace autofill {

// The cross-platform UI interface which prompts the user to unlock a masked
// Wallet instrument (credit card). This object is responsible for its own
// lifetime.
class CardUnmaskPromptView {
 public:
  typedef base::Callback<void(const base::string16& /* CVC */)> UnmaskCallback;

  // |finished| is guaranteed to be called.
  static CardUnmaskPromptView* CreateAndShow(content::WebContents* web_contents,
                                             const UnmaskCallback& finished);

 protected:
  CardUnmaskPromptView();
  virtual ~CardUnmaskPromptView();

 private:
  DISALLOW_COPY_AND_ASSIGN(CardUnmaskPromptView);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_CARD_UNMASK_PROMPT_VIEW_H_
