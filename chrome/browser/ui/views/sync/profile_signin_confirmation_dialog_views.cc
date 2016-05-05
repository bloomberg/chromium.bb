// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sync/profile_signin_confirmation_dialog_views.h"

#include <stddef.h>

#include <algorithm>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/profiles/profile_chooser_view.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/font.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/range/range.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"

ProfileSigninConfirmationDialogViews::ProfileSigninConfirmationDialogViews(
    Browser* browser,
    const std::string& username,
    ui::ProfileSigninConfirmationDelegate* delegate)
    : browser_(browser),
      username_(username),
      delegate_(delegate),
      prompt_for_new_profile_(true),
      continue_signin_button_(NULL) {
}

ProfileSigninConfirmationDialogViews::~ProfileSigninConfirmationDialogViews() {}

// static
void ProfileSigninConfirmationDialogViews::ShowDialog(
    Browser* browser,
    Profile* profile,
    const std::string& username,
    ui::ProfileSigninConfirmationDelegate* delegate) {
  // Hides the new avatar bubble if it is currently shown. The new avatar bubble
  // should be automatically closed when it loses focus. However on windows the
  // profile signin confirmation dialog is not modal yet thus it does not take
  // away focus, thus as a temporary workaround we need to manually close the
  // bubble.
  // TODO(guohui): removes the workaround once the profile confirmation dialog
  // is fixed.
  ProfileChooserView::Hide();

  ProfileSigninConfirmationDialogViews* dialog =
      new ProfileSigninConfirmationDialogViews(
          browser, username, delegate);
  ui::CheckShouldPromptForNewProfile(
      profile,
      // This callback is guaranteed to be invoked, and once it is, the dialog
      // owns itself.
      base::Bind(&ProfileSigninConfirmationDialogViews::Show,
                 base::Unretained(dialog)));
}

void ProfileSigninConfirmationDialogViews::Show(bool prompt_for_new_profile) {
  prompt_for_new_profile_ = prompt_for_new_profile;
  constrained_window::CreateBrowserModalDialogViews(
      this, browser_->window()->GetNativeWindow())->Show();
}

base::string16 ProfileSigninConfirmationDialogViews::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_ENTERPRISE_SIGNIN_TITLE_NEW_STYLE);
}

base::string16 ProfileSigninConfirmationDialogViews::GetDialogButtonLabel(
    ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_OK) {
    // If we're giving the option to create a new profile, then OK is
    // "Create new profile".  Otherwise it is "Continue signin".
    return l10n_util::GetStringUTF16(
        prompt_for_new_profile_ ?
            IDS_ENTERPRISE_SIGNIN_CREATE_NEW_PROFILE_NEW_STYLE :
            IDS_ENTERPRISE_SIGNIN_CONTINUE_NEW_STYLE);
  }
  return l10n_util::GetStringUTF16(IDS_ENTERPRISE_SIGNIN_CANCEL);
}

int ProfileSigninConfirmationDialogViews::GetDefaultDialogButton() const {
  return ui::DIALOG_BUTTON_NONE;
}

views::View* ProfileSigninConfirmationDialogViews::CreateExtraView() {
  if (prompt_for_new_profile_) {
    const base::string16 continue_signin_text =
        l10n_util::GetStringUTF16(IDS_ENTERPRISE_SIGNIN_CONTINUE_NEW_STYLE);
    continue_signin_button_ =
        new views::LabelButton(this, continue_signin_text);
    continue_signin_button_->SetStyle(views::Button::STYLE_BUTTON);
    views::Button::ConfigureDefaultFocus(continue_signin_button_);
  }
  return continue_signin_button_;
}

bool ProfileSigninConfirmationDialogViews::Accept() {
  if (delegate_) {
    if (prompt_for_new_profile_)
      delegate_->OnSigninWithNewProfile();
    else
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

ui::ModalType ProfileSigninConfirmationDialogViews::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

void ProfileSigninConfirmationDialogViews::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  views::DialogDelegateView::ViewHierarchyChanged(details);
  if (!details.is_add || details.child != this)
    return;

  const SkColor kPromptBarBackgroundColor =
      ui::GetSigninConfirmationPromptBarColor(
          GetNativeTheme(), ui::kSigninConfirmationPromptBarBackgroundAlpha);

  // Create the prompt label.
  size_t offset;
  const base::string16 domain =
      base::ASCIIToUTF16(gaia::ExtractDomainName(username_));
  const base::string16 username = base::ASCIIToUTF16(username_);
  const base::string16 prompt_text =
      l10n_util::GetStringFUTF16(
          IDS_ENTERPRISE_SIGNIN_ALERT_NEW_STYLE,
          domain, &offset);
  views::StyledLabel* prompt_label = new views::StyledLabel(prompt_text, this);
  prompt_label->SetDisplayedOnBackgroundColor(kPromptBarBackgroundColor);

  views::StyledLabel::RangeStyleInfo bold_style;
  bold_style.font_style = gfx::Font::BOLD;
  prompt_label->AddStyleRange(
      gfx::Range(offset, offset + domain.size()), bold_style);

  // Create the prompt bar.
  views::View* prompt_bar = new views::View;
  prompt_bar->SetBorder(views::Border::CreateSolidSidedBorder(
      1, 0, 1, 0,
      ui::GetSigninConfirmationPromptBarColor(
          GetNativeTheme(), ui::kSigninConfirmationPromptBarBorderAlpha)));
  prompt_bar->set_background(views::Background::CreateSolidBackground(
      kPromptBarBackgroundColor));

  // Create the explanation label.
  std::vector<size_t> offsets;
  const base::string16 learn_more_text =
      l10n_util::GetStringUTF16(
          IDS_ENTERPRISE_SIGNIN_PROFILE_LINK_LEARN_MORE);
  const base::string16 signin_explanation_text =
      l10n_util::GetStringFUTF16(prompt_for_new_profile_ ?
          IDS_ENTERPRISE_SIGNIN_EXPLANATION_WITH_PROFILE_CREATION_NEW_STYLE :
          IDS_ENTERPRISE_SIGNIN_EXPLANATION_WITHOUT_PROFILE_CREATION_NEW_STYLE,
          username, learn_more_text, &offsets);
  explanation_label_ = new views::StyledLabel(signin_explanation_text, this);
  explanation_label_->AddStyleRange(
      gfx::Range(offsets[1], offsets[1] + learn_more_text.size()),
      views::StyledLabel::RangeStyleInfo::CreateForLink());

  // Layout the components.
  views::GridLayout* dialog_layout = new views::GridLayout(this);
  SetLayoutManager(dialog_layout);

  // Use GridLayout inside the prompt bar because StyledLabel requires it.
  views::GridLayout* prompt_layout = views::GridLayout::CreatePanel(prompt_bar);
  prompt_bar->SetLayoutManager(prompt_layout);
  prompt_layout->AddColumnSet(0)->AddColumn(
      views::GridLayout::FILL, views::GridLayout::CENTER, 100,
      views::GridLayout::USE_PREF, 0, 0);
  prompt_layout->StartRow(0, 0);
  prompt_layout->AddView(prompt_label);
  // Use a column set with no padding.
  dialog_layout->AddColumnSet(0)->AddColumn(
      views::GridLayout::FILL, views::GridLayout::FILL, 100,
      views::GridLayout::USE_PREF, 0, 0);
  dialog_layout->StartRow(0, 0);
  dialog_layout->AddView(
      prompt_bar, 1, 1,
      views::GridLayout::FILL, views::GridLayout::FILL, 0, 0);

  // Use a new column set for the explanation label so we can add padding.
  dialog_layout->AddPaddingRow(0.0, views::kPanelVertMargin);
  views::ColumnSet* explanation_columns = dialog_layout->AddColumnSet(1);
  explanation_columns->AddPaddingColumn(0.0, views::kButtonHEdgeMarginNew);
  explanation_columns->AddColumn(
      views::GridLayout::FILL, views::GridLayout::FILL, 100,
      views::GridLayout::USE_PREF, 0, 0);
  explanation_columns->AddPaddingColumn(0.0, views::kButtonHEdgeMarginNew);
  dialog_layout->StartRow(0, 1);
  const int kPreferredWidth = 440;
  dialog_layout->AddView(
      explanation_label_, 1, 1,
      views::GridLayout::FILL, views::GridLayout::FILL,
      kPreferredWidth, explanation_label_->GetHeightForWidth(kPreferredWidth));
}

void ProfileSigninConfirmationDialogViews::WindowClosing() {
  Cancel();
}

void ProfileSigninConfirmationDialogViews::StyledLabelLinkClicked(
    views::StyledLabel* label,
    const gfx::Range& range,
    int event_flags) {
  chrome::NavigateParams params(
      browser_,
      GURL("https://support.google.com/chromebook/answer/1331549"),
      ui::PAGE_TRANSITION_LINK);
  params.disposition = NEW_POPUP;
  params.window_action = chrome::NavigateParams::SHOW_WINDOW;
  chrome::Navigate(&params);
}

void ProfileSigninConfirmationDialogViews::ButtonPressed(
    views::Button* sender,
    const ui::Event& event) {
  DCHECK(prompt_for_new_profile_);
  DCHECK_EQ(continue_signin_button_, sender);
  if (delegate_) {
    delegate_->OnContinueSignin();
    delegate_ = NULL;
  }
  GetWidget()->Close();
}
