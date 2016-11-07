// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/account_chooser_dialog_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/browser/ui/passwords/password_dialog_controller.h"
#include "chrome/browser/ui/views/passwords/credentials_item_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/common/password_form.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/border.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

namespace {

// Maximum height of the credential list. The unit is one row's height.
constexpr double kMaxHeightAccounts = 3.5;

constexpr int kVerticalAvatarMargin = 8;

// An identifier for views::ColumnSet.
enum ColumnSetType {
  SINGLE_VIEW_COLUMN_SET,
  SINGLE_VIEW_COLUMN_SET_NO_PADDING,
};

// Construct a |type| ColumnSet and add it to |layout|.
void BuildColumnSet(ColumnSetType type, views::GridLayout* layout) {
  views::ColumnSet* column_set = layout->AddColumnSet(type);
  bool padding = (type == SINGLE_VIEW_COLUMN_SET);
  if (padding)
    column_set->AddPaddingColumn(0, views::kButtonHEdgeMarginNew);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::FILL,
                        1,
                        views::GridLayout::USE_PREF,
                        0,
                        0);
  if (padding)
    column_set->AddPaddingColumn(0, views::kButtonHEdgeMarginNew);
}

views::StyledLabel::RangeStyleInfo GetLinkStyle() {
  auto result = views::StyledLabel::RangeStyleInfo::CreateForLink();
  result.disable_line_wrapping = false;
  return result;
}

Profile* GetProfileFromWebContents(content::WebContents* web_contents) {
  if (!web_contents)
    return nullptr;
  return Profile::FromBrowserContext(web_contents->GetBrowserContext());
}

// Creates a list view of credentials in |forms|.
views::ScrollView* CreateCredentialsView(
    const PasswordDialogController::FormsVector& forms,
    views::ButtonListener* button_listener,
    net::URLRequestContextGetter* request_context) {
  views::View* list_view = new views::View;
  list_view->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));
  int item_height = 0;
  for (const auto& form : forms) {
    std::pair<base::string16, base::string16> titles =
        GetCredentialLabelsForAccountChooser(*form);
    CredentialsItemView* credential_view = new CredentialsItemView(
        button_listener, titles.first, titles.second, kButtonHoverColor,
        form.get(), request_context);
    credential_view->SetLowerLabelColor(kAutoSigninTextColor);
    credential_view->SetBorder(views::CreateEmptyBorder(
        kVerticalAvatarMargin, views::kButtonHEdgeMarginNew,
        kVerticalAvatarMargin, views::kButtonHEdgeMarginNew));
    item_height = std::max(item_height, credential_view->GetPreferredHeight());
    list_view->AddChildView(credential_view);
  }
  views::ScrollView* scroll_view = new views::ScrollView;
  scroll_view->ClipHeightTo(0, kMaxHeightAccounts * item_height);
  scroll_view->SetContents(list_view);
  return scroll_view;
}

}  // namespace

AccountChooserDialogView::AccountChooserDialogView(
    PasswordDialogController* controller,
    content::WebContents* web_contents)
    : controller_(controller),
      web_contents_(web_contents),
      show_signin_button_(false) {
  DCHECK(controller);
  DCHECK(web_contents);
}

AccountChooserDialogView::~AccountChooserDialogView() = default;

void AccountChooserDialogView::ShowAccountChooser() {
  show_signin_button_ = controller_->ShouldShowSignInButton();
  InitWindow();
  constrained_window::ShowWebModalDialogViews(this, web_contents_);
}

void AccountChooserDialogView::ControllerGone() {
  controller_ = nullptr;
  GetWidget()->Close();
}

ui::ModalType AccountChooserDialogView::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

base::string16 AccountChooserDialogView::GetWindowTitle() const {
  return controller_->GetAccoutChooserTitle().first;
}

bool AccountChooserDialogView::ShouldShowWindowTitle() const {
  // The title may contain a hyperlink.
  return false;
}

bool AccountChooserDialogView::ShouldShowCloseButton() const {
  return false;
}

void AccountChooserDialogView::WindowClosing() {
  if (controller_)
    controller_->OnCloseDialog();
}

bool AccountChooserDialogView::Accept() {
  DCHECK(show_signin_button_);
  DCHECK(controller_);
  controller_->OnSignInClicked();
  // The dialog is closed by the controller.
  return false;
}

int AccountChooserDialogView::GetDialogButtons() const {
  if (show_signin_button_)
    return ui::DIALOG_BUTTON_CANCEL | ui::DIALOG_BUTTON_OK;
  return ui::DIALOG_BUTTON_CANCEL;
}

bool AccountChooserDialogView::ShouldDefaultButtonBeBlue() const {
  return show_signin_button_;
}

base::string16 AccountChooserDialogView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  int message_id = 0;
  if (button == ui::DIALOG_BUTTON_OK)
    message_id = IDS_PASSWORD_MANAGER_ACCOUNT_CHOOSER_SIGN_IN;
  else if (button == ui::DIALOG_BUTTON_CANCEL)
    message_id = IDS_APP_CANCEL;
  else
    NOTREACHED();
  return l10n_util::GetStringUTF16(message_id);
}

gfx::Size AccountChooserDialogView::GetPreferredSize() const {
  return gfx::Size(kDesiredWidth, GetHeightForWidth(kDesiredWidth));
}

void AccountChooserDialogView::StyledLabelLinkClicked(views::StyledLabel* label,
                                                      const gfx::Range& range,
                                                      int event_flags) {
  controller_->OnSmartLockLinkClicked();
}

void AccountChooserDialogView::ButtonPressed(views::Button* sender,
                                             const ui::Event& event) {
  CredentialsItemView* view = static_cast<CredentialsItemView*>(sender);
  controller_->OnChooseCredentials(
      *view->form(),
      password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD);
}

void AccountChooserDialogView::InitWindow() {
  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);
  BuildColumnSet(SINGLE_VIEW_COLUMN_SET, layout);

  // Create the title.
  std::pair<base::string16, gfx::Range> title_content =
      controller_->GetAccoutChooserTitle();
  views::StyledLabel* title_label =
      new views::StyledLabel(title_content.first, this);
  title_label->SetBaseFontList(
      ui::ResourceBundle::GetSharedInstance().GetFontList(
          ui::ResourceBundle::MediumFont));
  if (!title_content.second.is_empty()) {
    title_label->AddStyleRange(title_content.second, GetLinkStyle());
  }
  layout->StartRowWithPadding(0, SINGLE_VIEW_COLUMN_SET, 0, kTitleTopInset);
  layout->AddView(title_label);
  layout->AddPaddingRow(0, 2*views::kRelatedControlVerticalSpacing);

  // Show credentials.
  BuildColumnSet(SINGLE_VIEW_COLUMN_SET_NO_PADDING, layout);
  layout->StartRow(0, SINGLE_VIEW_COLUMN_SET_NO_PADDING);
  layout->AddView(CreateCredentialsView(
      controller_->GetLocalForms(),
      this,
      GetProfileFromWebContents(web_contents_)->GetRequestContext()));
  // DialogClientView adds kRelatedControlVerticalSpacing padding once more for
  // the buttons.
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
}

AccountChooserPrompt* CreateAccountChooserPromptView(
    PasswordDialogController* controller, content::WebContents* web_contents) {
  return new AccountChooserDialogView(controller, web_contents);
}
