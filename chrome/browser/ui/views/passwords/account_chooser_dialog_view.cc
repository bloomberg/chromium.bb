// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/account_chooser_dialog_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
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
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

namespace {

// An identifier for views::ColumnSet.
enum ColumnSetType {
  SINGLE_VIEW_COLUMN_SET,
};

// Construct a SINGLE_VIEW_COLUMN_SET ColumnSet and add it to |layout|.
void BuildOneColumnSet(views::GridLayout* layout) {
  views::ColumnSet* column_set = layout->AddColumnSet(SINGLE_VIEW_COLUMN_SET);
  column_set->AddPaddingColumn(0, views::kButtonHEdgeMarginNew);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::FILL,
                        1,
                        views::GridLayout::USE_PREF,
                        0,
                        0);
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

}  // namespace

AccountChooserDialogView::AccountChooserDialogView(
    PasswordDialogController* controller,
    content::WebContents* web_contents)
    : controller_(controller),
      web_contents_(web_contents) {
  DCHECK(controller);
  DCHECK(web_contents);
}

AccountChooserDialogView::~AccountChooserDialogView() = default;

void AccountChooserDialogView::ShowAccountChooser() {
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

int AccountChooserDialogView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_CANCEL;
}

base::string16 AccountChooserDialogView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return l10n_util::GetStringUTF16(IDS_APP_CANCEL);
}

void AccountChooserDialogView::OnClosed() {
  if (controller_)
    controller_->OnCloseDialog();
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
  controller_->OnChooseCredentials(*view->form(),
                                   view->credential_type());
}

void AccountChooserDialogView::InitWindow() {
  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);
  BuildOneColumnSet(layout);

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
  net::URLRequestContextGetter* request_context =
      GetProfileFromWebContents(web_contents_)->GetRequestContext();
  for (const auto& form : controller_->GetLocalForms()) {
    const base::string16& upper_string =
        form->display_name.empty() ? form->username_value : form->display_name;
    base::string16 lower_string;
    if (form->federation_url.is_empty()) {
      if (!form->display_name.empty())
        lower_string = form->username_value;
    } else {
      lower_string = l10n_util::GetStringFUTF16(
          IDS_PASSWORDS_VIA_FEDERATION,
          base::UTF8ToUTF16(form->federation_url.host()));
    }
    layout->StartRow(0, SINGLE_VIEW_COLUMN_SET);
    layout->AddView(new CredentialsItemView(
        this, form.get(),
        password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD,
        upper_string, lower_string, request_context));
  }
  // DialogClientView adds kRelatedControlVerticalSpacing padding once more for
  // the buttons.
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
}

AccountChooserPrompt* CreateAccountChooserPromptView(
    PasswordDialogController* controller, content::WebContents* web_contents) {
  return new AccountChooserDialogView(controller, web_contents);
}
