// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_SIGN_IN_PROMO_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_SIGN_IN_PROMO_VIEW_H_

#include "chrome/browser/ui/passwords/bubble_controllers/sign_in_promo_bubble_controller.h"
#include "chrome/browser/ui/sync/bubble_sync_promo_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace content {
class WebContents;
}
class SignInPromoBubbleController;

// A view that can show up after saving a password without being signed in to
// offer signing users in so they can access their credentials across devices.
class PasswordSignInPromoView : public views::View {
 public:
  explicit PasswordSignInPromoView(content::WebContents* web_contents);
  ~PasswordSignInPromoView() override;

 private:
  // Delegate for the personalized sync promo view used when desktop identity
  // consistency is enabled.
  class DiceSyncPromoDelegate : public BubbleSyncPromoDelegate {
   public:
    explicit DiceSyncPromoDelegate(SignInPromoBubbleController* controller);
    ~DiceSyncPromoDelegate() override;

    // BubbleSyncPromoDelegate:
    void OnEnableSync(const AccountInfo& account,
                      bool is_default_promo_account) override;

   private:
    SignInPromoBubbleController* controller_;

    DISALLOW_COPY_AND_ASSIGN(DiceSyncPromoDelegate);
  };

  SignInPromoBubbleController controller_;
  std::unique_ptr<DiceSyncPromoDelegate> dice_sync_promo_delegate_;

  DISALLOW_COPY_AND_ASSIGN(PasswordSignInPromoView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_SIGN_IN_PROMO_VIEW_H_
