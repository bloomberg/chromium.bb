// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_request_dialog_view.h"

#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"
#include "chrome/browser/ui/views/harmony/chrome_typography.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"

// static
void ShowAuthenticatorRequestDialog(
    content::WebContents* web_contents,
    std::unique_ptr<AuthenticatorRequestDialogModel> model) {
  // The authenticator request dialog will only be shown for common user-facing
  // WebContents, which have a |manager|. Most other sources without managers,
  // like service workers and extension background pages, do not allow WebAuthn
  // requests to be issued in the first place.
  // TODO(https://crbug.com/849323): There are some niche WebContents where the
  // WebAuthn API is available, but there is no |manager| available. Currently,
  // we will not be able to show a dialog, so the |model| will be immediately
  // destroyed. The request may be able to still run to completion if it does
  // not require any user input, otherise it will be blocked and time out. We
  // should audit this.
  auto* manager = web_modal::WebContentsModalDialogManager::FromWebContents(
      constrained_window::GetTopLevelWebContents(web_contents));
  if (!manager)
    return;

  auto dialog =
      std::make_unique<AuthenticatorRequestDialogView>(std::move(model));
  constrained_window::ShowWebModalDialogViews(dialog.release(), web_contents);
}

AuthenticatorRequestDialogView::AuthenticatorRequestDialogView(
    std::unique_ptr<AuthenticatorRequestDialogModel> model)
    : model_(std::move(model)) {
  model_->AddObserver(this);
  CreateContents();
}

AuthenticatorRequestDialogView::~AuthenticatorRequestDialogView() {
  model_->RemoveObserver(this);
}

void AuthenticatorRequestDialogView::CreateContents() {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetBorder(views::CreateEmptyBorder(
      views::LayoutProvider::Get()->GetDialogInsetsForContentType(
          views::TEXT, views::CONTROL)));
  auto description_label = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_WEBAUTHN_DIALOG_DESCRIPTION),
      CONTEXT_BODY_TEXT_SMALL, STYLE_SECONDARY);
  description_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  description_label->SetMultiLine(true);
  description_label->SetMaximumWidth(
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH) -
      margins().width());
  AddChildView(description_label.release());
}

int AuthenticatorRequestDialogView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

ui::ModalType AuthenticatorRequestDialogView::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

base::string16 AuthenticatorRequestDialogView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_DIALOG_TITLE);
}

void AuthenticatorRequestDialogView::OnModelDestroyed() {
  NOTREACHED();
}

void AuthenticatorRequestDialogView::OnRequestComplete() {
  if (!GetWidget())
    return;
  GetWidget()->Close();
}
