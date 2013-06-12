// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sync/profile_signin_confirmation_dialog_views.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/range/range.h"
#include "ui/gfx/font.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

namespace {

// Wrap a view in a fixed-width container.
views::View* MakeFixedWidth(views::View* view, int width) {
  views::View* container = new views::View;
  views::GridLayout* layout = views::GridLayout::CreatePanel(container);
  container->SetLayoutManager(layout);
  layout->AddColumnSet(0)->AddColumn(
      views::GridLayout::LEADING, views::GridLayout::CENTER, 0,
      views::GridLayout::FIXED, width, false);
  layout->StartRow(0, 0);
  layout->AddView(view, 1, 1, views::GridLayout::FILL, views::GridLayout::FILL);
  return container;
}

}  // namespace

namespace chrome {
// Declared in browser_dialogs.h
void ShowProfileSigninConfirmationDialog(
    Browser* browser,
    content::WebContents* web_contents,
    Profile* profile,
    const std::string& username,
    ui::ProfileSigninConfirmationDelegate* delegate) {
  ProfileSigninConfirmationDialogViews::ShowDialog(browser,
                                                   profile,
                                                   username,
                                                   delegate);
}
}  // namespace chrome

ProfileSigninConfirmationDialogViews::ProfileSigninConfirmationDialogViews(
    Browser* browser,
    Profile* profile,
    const std::string& username,
    ui::ProfileSigninConfirmationDelegate* delegate)
  : browser_(browser),
    profile_(profile),
    username_(username),
    delegate_(delegate),
    prompt_for_new_profile_(true),
    link_(NULL) {
}

ProfileSigninConfirmationDialogViews::~ProfileSigninConfirmationDialogViews() {}

// static
void ProfileSigninConfirmationDialogViews::ShowDialog(
    Browser* browser,
    Profile* profile,
    const std::string& username,
    ui::ProfileSigninConfirmationDelegate* delegate) {
  ProfileSigninConfirmationDialogViews* dialog =
      new ProfileSigninConfirmationDialogViews(
          browser, profile, username, delegate);
  ui::CheckShouldPromptForNewProfile(
      profile,
      // This callback is guaranteed to be invoked, and once it is, the dialog
      // owns itself.
      base::Bind(&ProfileSigninConfirmationDialogViews::Show,
                 base::Unretained(dialog)));
}

void ProfileSigninConfirmationDialogViews::Show(bool prompt_for_new_profile) {
  prompt_for_new_profile_ = prompt_for_new_profile;
  CreateBrowserModalDialogViews(
      this, browser_->window()->GetNativeWindow())->Show();
}

string16 ProfileSigninConfirmationDialogViews::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_ENTERPRISE_SIGNIN_TITLE_NEW_STYLE);
}

string16 ProfileSigninConfirmationDialogViews::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return l10n_util::GetStringUTF16((button == ui::DIALOG_BUTTON_OK) ?
      IDS_ENTERPRISE_SIGNIN_CONTINUE_NEW_STYLE :
      IDS_ENTERPRISE_SIGNIN_CANCEL);
}

int ProfileSigninConfirmationDialogViews::GetDefaultDialogButton() const {
  return ui::DIALOG_BUTTON_NONE;
}

views::View* ProfileSigninConfirmationDialogViews::CreateExtraView() {
  if (prompt_for_new_profile_) {
    const string16 create_profile_text =
        l10n_util::GetStringUTF16(
            IDS_ENTERPRISE_SIGNIN_CREATE_NEW_PROFILE_NEW_STYLE);
    link_ = new views::Link(create_profile_text);
    link_->SetUnderline(false);
    link_->set_listener(this);
    link_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  }
  return link_;
}

bool ProfileSigninConfirmationDialogViews::Accept() {
  if (delegate_) {
    delegate_->OnContinueSignin();
    delegate_ = NULL;
  }
  return true;
}

bool ProfileSigninConfirmationDialogViews::Cancel() {
  if (delegate_) {
    delegate_->OnCancelSignin();
    delegate_ = NULL;
  }
  return true;
}

void ProfileSigninConfirmationDialogViews::OnClose() {
  Cancel();
}

ui::ModalType ProfileSigninConfirmationDialogViews::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

void ProfileSigninConfirmationDialogViews::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (!details.is_add || details.child != this)
    return;

  // Layout the labels in a single fixed-width column.
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));

  // Create the prompt label.
  std::vector<size_t> offsets;
  const string16 domain = ASCIIToUTF16(gaia::ExtractDomainName(username_));
  const string16 username = ASCIIToUTF16(username_);
  const string16 prompt_text =
      l10n_util::GetStringFUTF16(
          IDS_ENTERPRISE_SIGNIN_ALERT_NEW_STYLE,
          username, domain, &offsets);
  views::StyledLabel* prompt_label = new views::StyledLabel(prompt_text, this);
  views::StyledLabel::RangeStyleInfo bold_style;
  bold_style.font_style = gfx::Font::BOLD;
  prompt_label->AddStyleRange(
      ui::Range(offsets[1], offsets[1] + domain.size()), bold_style);

  // Add the prompt label with a darker background and border.
  const int kDialogWidth = 440;
  views::View* prompt_container = MakeFixedWidth(prompt_label, kDialogWidth);
  prompt_container->set_border(
      views::Border::CreateSolidSidedBorder(
          1, 0, 1, 0,
          ui::GetSigninConfirmationPromptBarColor(
              ui::kSigninConfirmationPromptBarBorderAlpha)));
  prompt_container->set_background(
      views::Background::CreateSolidBackground(
          ui::GetSigninConfirmationPromptBarColor(
              ui::kSigninConfirmationPromptBarBackgroundAlpha)));
  AddChildView(prompt_container);

  // Create and add the explanation label.
  offsets.clear();
  const string16 learn_more_text =
      l10n_util::GetStringUTF16(
          IDS_ENTERPRISE_SIGNIN_PROFILE_LINK_LEARN_MORE);
  const string16 signin_explanation_text =
      l10n_util::GetStringFUTF16(prompt_for_new_profile_ ?
          IDS_ENTERPRISE_SIGNIN_EXPLANATION_WITH_PROFILE_CREATION_NEW_STYLE :
          IDS_ENTERPRISE_SIGNIN_EXPLANATION_WITHOUT_PROFILE_CREATION_NEW_STYLE,
          username, learn_more_text, &offsets);
  explanation_label_ = new views::StyledLabel(signin_explanation_text, this);
  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink();
  link_style.font_style = gfx::Font::NORMAL;
  explanation_label_->AddStyleRange(
      ui::Range(offsets[1], offsets[1] + learn_more_text.size()),
      link_style);
  // TODO(dconnelly): set the background color on the label (crbug.com/244630)
  AddChildView(MakeFixedWidth(explanation_label_, kDialogWidth));
}

void ProfileSigninConfirmationDialogViews::LinkClicked(views::Link* source,
                                                       int event_flags) {
  if (delegate_) {
    delegate_->OnSigninWithNewProfile();
    delegate_ = NULL;
  }
  GetWidget()->Close();
}

void ProfileSigninConfirmationDialogViews::StyledLabelLinkClicked(
    const ui::Range& range,
    int event_flags) {
  chrome::NavigateParams params(
      browser_,
      GURL("http://support.google.com/chromeos/bin/answer.py?answer=1331549"),
      content::PAGE_TRANSITION_LINK);
  params.disposition = NEW_POPUP;
  params.window_action = chrome::NavigateParams::SHOW_WINDOW;
  chrome::Navigate(&params);
}
