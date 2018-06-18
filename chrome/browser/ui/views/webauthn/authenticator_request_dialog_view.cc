// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_request_dialog_view.h"

#include "base/logging.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"
#include "chrome/browser/ui/views/harmony/chrome_typography.h"
#include "chrome/browser/ui/webauthn/authenticator_request_sheet_model.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/layout/fill_layout.h"

namespace {

// Defines a static sheet with no additional step-specific content and no button
// enabled other than `Cancel`.
class SimpleStaticSheetModel : public AuthenticatorRequestSheetModel {
 public:
  SimpleStaticSheetModel() = default;

 private:
  // AuthenticatorRequestSheetModel:
  bool IsBackButtonVisible() const override { return false; }

  bool IsCancelButtonVisible() const override { return true; }
  base::string16 GetCancelButtonLabel() const override {
    return l10n_util::GetStringUTF16(IDS_CANCEL);
  }

  bool IsAcceptButtonVisible() const override { return false; }
  bool IsAcceptButtonEnabled() const override { return false; }
  base::string16 GetAcceptButtonLabel() const override {
    return base::string16();
  }

  base::string16 GetStepTitle() const override {
    return l10n_util::GetStringUTF16(IDS_WEBAUTHN_DIALOG_TITLE);
  }

  base::string16 GetStepDescription() const override {
    return l10n_util::GetStringUTF16(IDS_WEBAUTHN_DIALOG_DESCRIPTION);
  }

  void OnBack() override { NOTREACHED(); }
  void OnAccept() override { NOTREACHED(); }
  void OnCancel() override {}

  DISALLOW_COPY_AND_ASSIGN(SimpleStaticSheetModel);
};

}  // namespace

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

  // Keep this logic in sync with AuthenticatorRequestDialogViewTestApi::Show.
  auto dialog = std::make_unique<AuthenticatorRequestDialogView>(
      web_contents, std::move(model));
  constrained_window::ShowWebModalDialogViews(dialog.release(), web_contents);
}

// TODO(engedy): Create the sheet based on the model here.
AuthenticatorRequestDialogView::AuthenticatorRequestDialogView(
    content::WebContents* web_contents,
    std::unique_ptr<AuthenticatorRequestDialogModel> model)
    : content::WebContentsObserver(web_contents), model_(std::move(model)) {
  model_->AddObserver(this);

  // Currently, all sheets have a label on top and controls at the bottom.
  // Consider moving this to AuthenticatorRequestSheetView if this changes.
  SetBorder(views::CreateEmptyBorder(
      views::LayoutProvider::Get()->GetDialogInsetsForContentType(
          views::TEXT, views::CONTROL)));
  SetLayoutManager(std::make_unique<views::FillLayout>());

  auto sheet_view = std::make_unique<AuthenticatorRequestSheetView>(
      std::make_unique<SimpleStaticSheetModel>());
  sheet_view->InitChildViews();
  sheet_ = sheet_view.get();
  AddChildView(sheet_view.release());
}

AuthenticatorRequestDialogView::~AuthenticatorRequestDialogView() {
  model_->RemoveObserver(this);

  // AuthenticatorRequestDialogView is a WidgetDelegate, owned by views::Widget.
  // It's only destroyed by Widget::OnNativeWidgetDestroyed() invoking
  // DeleteDelegate(), and because WIDGET_OWNS_NATIVE_WIDGET, ~Widget() is
  // invoked straight after, which destroys child views. views::View subclasses
  // shouldn't be doing anything interesting in their destructors, so it should
  // be okay to destroy the |sheet_| immediately after this line.
}

gfx::Size AuthenticatorRequestDialogView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH);
  return gfx::Size(width, GetHeightForWidth(width));
}

bool AuthenticatorRequestDialogView::Accept() {
  sheet()->model()->OnAccept();
  return false;
}

bool AuthenticatorRequestDialogView::Cancel() {
  sheet()->model()->OnCancel();
  return false;
}

bool AuthenticatorRequestDialogView::Close() {
  return true;
}

int AuthenticatorRequestDialogView::GetDialogButtons() const {
  int button_mask = 0;
  if (sheet()->model()->IsAcceptButtonVisible())
    button_mask |= ui::DIALOG_BUTTON_OK;
  if (sheet()->model()->IsCancelButtonVisible())
    button_mask |= ui::DIALOG_BUTTON_CANCEL;
  return button_mask;
}

int AuthenticatorRequestDialogView::GetDefaultDialogButton() const {
  // The default button is either the `Ok` button or nothing.
  if (sheet()->model()->IsAcceptButtonVisible())
    return ui::DIALOG_BUTTON_OK;
  return ui::DIALOG_BUTTON_NONE;
}

base::string16 AuthenticatorRequestDialogView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  switch (button) {
    case ui::DIALOG_BUTTON_NONE:
      break;
    case ui::DIALOG_BUTTON_OK:
      return sheet()->model()->GetAcceptButtonLabel();
    case ui::DIALOG_BUTTON_CANCEL:
      return sheet()->model()->GetCancelButtonLabel();
  }
  NOTREACHED();
  return base::string16();
}

bool AuthenticatorRequestDialogView::IsDialogButtonEnabled(
    ui::DialogButton button) const {
  switch (button) {
    case ui::DIALOG_BUTTON_NONE:
      break;
    case ui::DIALOG_BUTTON_OK:
      return sheet()->model()->IsAcceptButtonEnabled();
    case ui::DIALOG_BUTTON_CANCEL:
      return true;  // Cancel is always enabled if visible.
  }
  NOTREACHED();
  return false;
}

ui::ModalType AuthenticatorRequestDialogView::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

base::string16 AuthenticatorRequestDialogView::GetWindowTitle() const {
  return sheet()->model()->GetStepTitle();
}

bool AuthenticatorRequestDialogView::ShouldShowWindowTitle() const {
  return false;
}

bool AuthenticatorRequestDialogView::ShouldShowCloseButton() const {
  return false;
}

void AuthenticatorRequestDialogView::OnModelDestroyed() {
  NOTREACHED();
}

void AuthenticatorRequestDialogView::OnStepTransition() {
  if (model_->current_step() ==
      AuthenticatorRequestDialogModel::Step::kCompleted) {
    if (!GetWidget())
      return;
    GetWidget()->Close();
  }
}

void AuthenticatorRequestDialogView::ReplaceCurrentSheetWith(
    std::unique_ptr<AuthenticatorRequestSheetView> new_sheet) {
  delete sheet_;
  DCHECK(!has_children());

  sheet_ = new_sheet.get();
  AddChildView(new_sheet.release());

  // The dialog button configuration is delegated to the |sheet_|, and the new
  // sheet likely wants to provide a new configuration.
  DialogModelChanged();

  // The accessibility title is also sourced from the |sheet_|'s step title, so
  // update it unless the widget is not yet shown or already being torn down.
  if (!GetWidget())
    return;

  GetWidget()->UpdateWindowTitle();

  // TODO(https://crbug.com/849323): Investigate how a web-modal dialog's
  // lifetime compares to that of the parent WebContents. Take a conservative
  // approach for now.
  if (!web_contents())
    return;

  // The |dialog_manager| might temporarily be unavailable while te tab is being
  // dragged from one browser window to the other.
  auto* dialog_manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents());
  if (!dialog_manager)
    return;

  // Update the dialog size and position, as the preferred size of the sheet
  // might have changed.
  constrained_window::UpdateWebContentsModalDialogPosition(
      GetWidget(),
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents())
          ->delegate()
          ->GetWebContentsModalDialogHost());
}
