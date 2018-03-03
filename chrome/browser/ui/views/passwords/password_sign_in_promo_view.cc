// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_sign_in_promo_view.h"

#include "base/metrics/user_metrics.h"
#include "build/buildflag.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"
#include "chrome/browser/ui/views/harmony/chrome_typography.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/core/browser/account_info.h"
#include "components/signin/core/browser/signin_buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/layout/fill_layout.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/ui/views/sync/dice_bubble_sync_promo_view.h"
#endif

PasswordSignInPromoView::DiceSyncPromoDelegate::DiceSyncPromoDelegate(
    ManagePasswordsBubbleModel* model)
    : model_(model) {
  DCHECK(model_);
}

PasswordSignInPromoView::DiceSyncPromoDelegate::~DiceSyncPromoDelegate() =
    default;

void PasswordSignInPromoView::DiceSyncPromoDelegate::OnEnableSync(
    const AccountInfo& account) {
  model_->OnSignInToChromeClicked(account);
}

PasswordSignInPromoView::PasswordSignInPromoView(
    ManagePasswordsBubbleModel* model)
    : model_(model) {
  DCHECK(model_);
  base::RecordAction(
      base::UserMetricsAction("Signin_Impression_FromPasswordBubble"));

  SetLayoutManager(std::make_unique<views::FillLayout>());
  Profile* profile = model_->GetProfile();
  if (AccountConsistencyModeManager::IsDiceEnabledForProfile(profile)) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    dice_sync_promo_delegate_ =
        std::make_unique<PasswordSignInPromoView::DiceSyncPromoDelegate>(
            model_);
    AddChildView(new DiceBubbleSyncPromoView(
        profile, dice_sync_promo_delegate_.get(),
        IDS_PASSWORD_MANAGER_DICE_PROMO_SIGNIN_MESSAGE,
        IDS_PASSWORD_MANAGER_DICE_PROMO_SYNC_MESSAGE));
#else
    NOTREACHED();
#endif
  } else {
    auto label = std::make_unique<views::Label>(
        l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_SIGNIN_PROMO_LABEL),
        CONTEXT_BODY_TEXT_LARGE, STYLE_SECONDARY);
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    label->SetMultiLine(true);
    AddChildView(label.release());
  }
}

PasswordSignInPromoView::~PasswordSignInPromoView() = default;

bool PasswordSignInPromoView::Accept() {
  DCHECK(!dice_sync_promo_delegate_);
  model_->OnSignInToChromeClicked(AccountInfo());
  return true;
}

bool PasswordSignInPromoView::Cancel() {
  NOTREACHED();
  return true;
}

int PasswordSignInPromoView::GetDialogButtons() const {
  if (dice_sync_promo_delegate_) {
    // The desktop identity consistency sync promo has its own promo message
    // and button (it does not reuse the ManagePasswordPendingView's dialog
    // buttons).
    return ui::DIALOG_BUTTON_NONE;
  }
  return ui::DIALOG_BUTTON_OK;
}

base::string16 PasswordSignInPromoView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  DCHECK_EQ(ui::DIALOG_BUTTON_OK, button);
  DCHECK(!dice_sync_promo_delegate_);
  return l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_SIGNIN_PROMO_SIGN_IN);
}
