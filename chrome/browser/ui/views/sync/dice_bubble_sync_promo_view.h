// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SYNC_DICE_BUBBLE_SYNC_PROMO_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SYNC_DICE_BUBBLE_SYNC_PROMO_VIEW_H_

#include "base/macros.h"
#include "chrome/browser/ui/sync/bubble_sync_promo_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

class Profile;
class DiceSigninButton;

// A personalized sync promo used when Desktop Identity Consistency is enabled.
// Its display a message informing the user the benefits of enabling sync and
// a button that allows the user to enable sync.
// The button has 2 different displays:
// * If Chrome has no accounts, then the promo button is a MD button allowing
//   the user to sign in to Chrome.
// * If Chrome has at least one account, then the promo button is personalized
//   with the user full name and avatar icon and allows the user to enable sync.
class DiceBubbleSyncPromoView : public views::View,
                                public views::ButtonListener {
 public:
  // Creates a personalized sync promo view.
  // |delegate| is not owned by DiceBubbleSyncPromoView.
  // The promo message is set to |no_accounts_promo_message_resource_id| when
  // Chrome has no accounts.
  // The promo message is set to |accounts_promo_message_resource_id| when
  // Chrome has at least one account.
  DiceBubbleSyncPromoView(Profile* profile,
                          BubbleSyncPromoDelegate* delegate,
                          int no_accounts_promo_message_resource_id,
                          int accounts_promo_message_resource_id);
  ~DiceBubbleSyncPromoView() override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  // views::View:
  const char* GetClassName() const override;

  // Delegate, to handle clicks on the sign-in buttons.
  BubbleSyncPromoDelegate* delegate_;
  DiceSigninButton* signin_button_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(DiceBubbleSyncPromoView);
};
#endif  // CHROME_BROWSER_UI_VIEWS_SYNC_DICE_BUBBLE_SYNC_PROMO_VIEW_H_
