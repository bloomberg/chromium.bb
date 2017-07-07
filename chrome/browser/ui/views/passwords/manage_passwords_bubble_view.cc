// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/manage_passwords_bubble_view.h"

#include "base/macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/timer.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/passwords/password_dialog_prompts.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"
#include "chrome/browser/ui/views/harmony/chrome_typography.h"
#include "chrome/browser/ui/views/passwords/credentials_item_view.h"
#include "chrome/browser/ui/views/passwords/credentials_selection_view.h"
#include "chrome/browser/ui/views/passwords/manage_password_items_view.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_icon_views.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_features.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/blue_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"

#if !defined(OS_MACOSX) || BUILDFLAG(MAC_VIEWS_BROWSER)
#include "chrome/browser/ui/views/frame/browser_view.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/ui/views/desktop_ios_promotion/desktop_ios_promotion_bubble_view.h"
#endif

int ManagePasswordsBubbleView::auto_signin_toast_timeout_ = 3;

// Helpers --------------------------------------------------------------------

namespace {

enum ColumnSetType {
  // | | (FILL, FILL) | |
  // Used for the bubble's header, the credentials list, and for simple
  // messages like "No passwords".
  SINGLE_VIEW_COLUMN_SET,

  // | | (FILL, FILL) | | (FILL, FILL) | |
  // Used for the credentials line of the bubble, for the pending view.
  DOUBLE_VIEW_COLUMN_SET,

  // | | (TRAILING, CENTER) | | (TRAILING, CENTER) | |
  // Used for buttons at the bottom of the bubble which should nest at the
  // bottom-right corner.
  DOUBLE_BUTTON_COLUMN_SET,

  // | | (LEADING, CENTER) | | (TRAILING, CENTER) | |
  // Used for buttons at the bottom of the bubble which should occupy
  // the corners.
  LINK_BUTTON_COLUMN_SET,

  // | | (TRAILING, CENTER) | |
  // Used when there is only one button which should next at the bottom-right
  // corner.
  SINGLE_BUTTON_COLUMN_SET,

  // | | (LEADING, CENTER) | | (TRAILING, CENTER) | | (TRAILING, CENTER) | |
  // Used when there are three buttons.
  TRIPLE_BUTTON_COLUMN_SET,
};

enum TextRowType { ROW_SINGLE, ROW_MULTILINE };

// Construct an appropriate ColumnSet for the given |type|, and add it
// to |layout|.
void BuildColumnSet(views::GridLayout* layout, ColumnSetType type) {
  views::ColumnSet* column_set = layout->AddColumnSet(type);
  int full_width = ManagePasswordsBubbleView::kDesiredBubbleWidth;
  const int button_divider = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_BUTTON_HORIZONTAL);
  const int column_divider = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_HORIZONTAL);
  switch (type) {
    case SINGLE_VIEW_COLUMN_SET:
      column_set->AddColumn(views::GridLayout::FILL,
                            views::GridLayout::FILL,
                            0,
                            views::GridLayout::FIXED,
                            full_width,
                            0);
      break;
    case DOUBLE_VIEW_COLUMN_SET:
      column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                            views::GridLayout::USE_PREF, 0, 0);
      column_set->AddPaddingColumn(0, column_divider);
      column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                            views::GridLayout::USE_PREF, 0, 0);
      break;
    case DOUBLE_BUTTON_COLUMN_SET:
      column_set->AddColumn(views::GridLayout::TRAILING,
                            views::GridLayout::CENTER,
                            1,
                            views::GridLayout::USE_PREF,
                            0,
                            0);
      column_set->AddPaddingColumn(0, button_divider);
      column_set->AddColumn(views::GridLayout::TRAILING,
                            views::GridLayout::CENTER,
                            0,
                            views::GridLayout::USE_PREF,
                            0,
                            0);
      break;
    case LINK_BUTTON_COLUMN_SET:
      column_set->AddColumn(views::GridLayout::LEADING,
                            views::GridLayout::CENTER,
                            1,
                            views::GridLayout::USE_PREF,
                            0,
                            0);
      column_set->AddPaddingColumn(0, button_divider);
      column_set->AddColumn(views::GridLayout::TRAILING,
                            views::GridLayout::CENTER,
                            0,
                            views::GridLayout::USE_PREF,
                            0,
                            0);
      break;
    case SINGLE_BUTTON_COLUMN_SET:
      column_set->AddColumn(views::GridLayout::TRAILING,
                            views::GridLayout::CENTER,
                            1,
                            views::GridLayout::USE_PREF,
                            0,
                            0);
      break;
    case TRIPLE_BUTTON_COLUMN_SET:
      column_set->AddColumn(views::GridLayout::LEADING,
                            views::GridLayout::CENTER,
                            1,
                            views::GridLayout::USE_PREF,
                            0,
                            0);
      column_set->AddPaddingColumn(0, button_divider);
      column_set->AddColumn(views::GridLayout::TRAILING,
                            views::GridLayout::CENTER,
                            0,
                            views::GridLayout::USE_PREF,
                            0,
                            0);
      column_set->AddPaddingColumn(0, button_divider);
      column_set->AddColumn(views::GridLayout::TRAILING,
                            views::GridLayout::CENTER,
                            0,
                            views::GridLayout::USE_PREF,
                            0,
                            0);
      break;
  }
}

views::StyledLabel::RangeStyleInfo GetLinkStyle() {
  auto result = views::StyledLabel::RangeStyleInfo::CreateForLink();
  result.disable_line_wrapping = false;
  return result;
}

}  // namespace

// ManagePasswordsBubbleView::AutoSigninView ----------------------------------

// A view containing just one credential that was used for for automatic signing
// in.
class ManagePasswordsBubbleView::AutoSigninView
    : public views::View,
      public views::ButtonListener,
      public views::WidgetObserver {
 public:
  explicit AutoSigninView(ManagePasswordsBubbleView* parent);

 private:
  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::WidgetObserver:
  // Tracks the state of the browser window.
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  void OnWidgetClosing(views::Widget* widget) override;

  void OnTimer();
  static base::TimeDelta GetTimeout() {
    return base::TimeDelta::FromSeconds(
        ManagePasswordsBubbleView::auto_signin_toast_timeout_);
  }

  base::OneShotTimer timer_;
  ManagePasswordsBubbleView* parent_;
  ScopedObserver<views::Widget, views::WidgetObserver> observed_browser_;

  DISALLOW_COPY_AND_ASSIGN(AutoSigninView);
};

ManagePasswordsBubbleView::AutoSigninView::AutoSigninView(
    ManagePasswordsBubbleView* parent)
    : parent_(parent),
      observed_browser_(this) {
  SetLayoutManager(new views::FillLayout);
  const autofill::PasswordForm& form = parent_->model()->pending_password();
  CredentialsItemView* credential;
  base::string16 upper_text, lower_text = form.username_value;
  if (ChromeLayoutProvider::Get()->IsHarmonyMode()) {
    upper_text =
        l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_AUTO_SIGNIN_TITLE_MD);
  } else {
    lower_text = l10n_util::GetStringFUTF16(
        IDS_MANAGE_PASSWORDS_AUTO_SIGNIN_TITLE, lower_text);
  }
  credential = new CredentialsItemView(
      this, upper_text, lower_text, kButtonHoverColor, &form,
      parent_->model()->GetProfile()->GetRequestContext());
  credential->SetEnabled(false);
  AddChildView(credential);

  // Setup the observer and maybe start the timer.
  Browser* browser =
      chrome::FindBrowserWithWebContents(parent_->web_contents());
  DCHECK(browser);

// Sign-in dialogs opened for inactive browser windows do not auto-close on
// MacOS. This matches existing Cocoa bubble behavior.
// TODO(varkha): Remove the limitation as part of http://crbug/671916 .
#if !defined(OS_MACOSX) || BUILDFLAG(MAC_VIEWS_BROWSER)
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  observed_browser_.Add(browser_view->GetWidget());
#endif
  if (browser->window()->IsActive())
    timer_.Start(FROM_HERE, GetTimeout(), this, &AutoSigninView::OnTimer);
}

void ManagePasswordsBubbleView::AutoSigninView::ButtonPressed(
    views::Button* sender, const ui::Event& event) {
  NOTREACHED();
}

void ManagePasswordsBubbleView::AutoSigninView::OnWidgetActivationChanged(
    views::Widget* widget, bool active) {
  if (active && !timer_.IsRunning())
    timer_.Start(FROM_HERE, GetTimeout(), this, &AutoSigninView::OnTimer);
}

void ManagePasswordsBubbleView::AutoSigninView::OnWidgetClosing(
    views::Widget* widget) {
  observed_browser_.RemoveAll();
}

void ManagePasswordsBubbleView::AutoSigninView::OnTimer() {
  parent_->model()->OnAutoSignInToastTimeout();
  parent_->CloseBubble();
}

// ManagePasswordsBubbleView::PendingView -------------------------------------

// A view offering the user the ability to save credentials. Contains a
// single ManagePasswordItemsView, along with a "Save Passwords" button,
// a "Never" button and an "Edit" button to edit username field.
class ManagePasswordsBubbleView::PendingView
    : public views::View,
      public views::ButtonListener,
      public views::FocusChangeListener {
 public:
  explicit PendingView(ManagePasswordsBubbleView* parent);
  ~PendingView() override;

 private:
  void CreateAndSetLayout();
  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;
  // views::FocusChangeListener:
  void OnWillChangeFocus(View* focused_before, View* focused_now) override;
  void OnDidChangeFocus(View* focused_before, View* focused_now) override;
  // views::View:
  bool OnKeyPressed(const ui::KeyEvent& event) override;

  void ToggleEditingState(bool accept_changes);

  ManagePasswordsBubbleView* parent_;

  views::Button* edit_button_;
  views::Button* save_button_;
  views::Button* never_button_;
  views::View* username_field_;
  views::View* password_field_;

  bool editing_;

  DISALLOW_COPY_AND_ASSIGN(PendingView);
};

ManagePasswordsBubbleView::PendingView::PendingView(
    ManagePasswordsBubbleView* parent)
    : parent_(parent),
      edit_button_(nullptr),
      save_button_(nullptr),
      never_button_(nullptr),
      username_field_(nullptr),
      password_field_(nullptr),
      editing_(false) {
  CreateAndSetLayout();
  parent_->set_initially_focused_view(save_button_);
}

void ManagePasswordsBubbleView::PendingView::CreateAndSetLayout() {
  views::GridLayout* layout = new views::GridLayout(this);
  layout->set_minimum_size(gfx::Size(kDesiredBubbleWidth, 0));
  SetLayoutManager(layout);

  // Create the edit, save and never buttons.
  if (!edit_button_ &&
      base::FeatureList::IsEnabled(
          password_manager::features::kEnableUsernameCorrection)) {
    edit_button_ = views::MdTextButton::CreateSecondaryUiButton(
        this, l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EDIT_BUTTON));
  }
  if (!save_button_) {
    save_button_ = views::MdTextButton::CreateSecondaryUiBlueButton(
        this, l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_SAVE_BUTTON));
  }
  if (!never_button_) {
    never_button_ = views::MdTextButton::CreateSecondaryUiButton(
        this, l10n_util::GetStringUTF16(
                  IDS_PASSWORD_MANAGER_BUBBLE_BLACKLIST_BUTTON));
  }

  // Credentials row.
  BuildColumnSet(layout, DOUBLE_VIEW_COLUMN_SET);
  if (!parent_->model()->pending_password().username_value.empty() ||
      edit_button_) {
    layout->StartRow(0, DOUBLE_VIEW_COLUMN_SET);
    const autofill::PasswordForm* password_form =
        &parent_->model()->pending_password();
    DCHECK(!username_field_);
    if (editing_) {
      username_field_ = GenerateUsernameEditable(*password_form).release();
    } else {
      username_field_ = GenerateUsernameLabel(*password_form).release();
    }
    if (!password_field_) {
      password_field_ = GeneratePasswordLabel(*password_form).release();
    }
    layout->AddView(username_field_);
    layout->AddView(password_field_);
    layout->AddPaddingRow(0,
                          ChromeLayoutProvider::Get()
                              ->GetInsetsMetric(views::INSETS_DIALOG_CONTENTS)
                              .bottom());
  }
  // Button row.
  ColumnSetType column_set_type =
      edit_button_ ? TRIPLE_BUTTON_COLUMN_SET : DOUBLE_BUTTON_COLUMN_SET;
  BuildColumnSet(layout, column_set_type);
  layout->StartRow(0, column_set_type);
  if (column_set_type == TRIPLE_BUTTON_COLUMN_SET) {
    layout->AddView(edit_button_);
  }
  layout->AddView(save_button_);
  layout->AddView(never_button_);
}

ManagePasswordsBubbleView::PendingView::~PendingView() {
}

void ManagePasswordsBubbleView::PendingView::ButtonPressed(
    views::Button* sender,
    const ui::Event& event) {
  // TODO(https://crbug.com/734965): Implement edit button logic.
  if (sender == edit_button_) {
    ToggleEditingState(false);
    return;
  }
  if (sender == save_button_) {
    parent_->model()->OnSaveClicked();
    if (parent_->model()->ReplaceToShowPromotionIfNeeded()) {
      parent_->Refresh();
      return;
    }
  } else if (sender == never_button_) {
    parent_->model()->OnNeverForThisSiteClicked();
  } else {
    NOTREACHED();
  }

  parent_->CloseBubble();
}

void ManagePasswordsBubbleView::PendingView::OnWillChangeFocus(
    View* focused_before,
    View* focused_now) {
  // Nothing to do here.
}

void ManagePasswordsBubbleView::PendingView::OnDidChangeFocus(
    View* focused_before,
    View* focused_now) {
  if (editing_ && focused_before == username_field_) {
    ToggleEditingState(true);
  }
}

bool ManagePasswordsBubbleView::PendingView::OnKeyPressed(
    const ui::KeyEvent& event) {
  if (editing_ && (event.key_code() == ui::KeyboardCode::VKEY_RETURN ||
                   event.key_code() == ui::KeyboardCode::VKEY_ESCAPE)) {
    ToggleEditingState(event.key_code() == ui::KeyboardCode::VKEY_RETURN);
    return true;
  }
  return false;
}

void ManagePasswordsBubbleView::PendingView::ToggleEditingState(
    bool accept_changes) {
  if (editing_ && accept_changes) {
    parent_->model()->OnUsernameEdited(
        static_cast<views::Textfield*>(username_field_)->text());
  }
  editing_ = !editing_;
  edit_button_->SetEnabled(!editing_);
  RemoveChildView(username_field_);
  username_field_ = nullptr;
  CreateAndSetLayout();
  Layout();
  if (editing_) {
    GetFocusManager()->SetFocusedView(username_field_);
    GetFocusManager()->AddFocusChangeListener(this);
  } else {
    GetFocusManager()->RemoveFocusChangeListener(this);
    GetFocusManager()->SetFocusedView(save_button_);
  }
  parent_->SizeToContents();
}

// ManagePasswordsBubbleView::ManageView --------------------------------------

// A view offering the user a list of their currently saved credentials
// for the current page, along with a "Manage passwords" link and a
// "Done" button.
class ManagePasswordsBubbleView::ManageView : public views::View,
                                              public views::ButtonListener,
                                              public views::LinkListener {
 public:
  explicit ManageView(ManagePasswordsBubbleView* parent);
  ~ManageView() override;

 private:
  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::LinkListener:
  void LinkClicked(views::Link* source, int event_flags) override;

  ManagePasswordsBubbleView* parent_;

  views::Link* manage_link_;
  views::Button* done_button_;

  DISALLOW_COPY_AND_ASSIGN(ManageView);
};

ManagePasswordsBubbleView::ManageView::ManageView(
    ManagePasswordsBubbleView* parent)
    : parent_(parent) {
  views::GridLayout* layout = new views::GridLayout(this);
  layout->set_minimum_size(gfx::Size(kDesiredBubbleWidth, 0));
  SetLayoutManager(layout);
  BuildColumnSet(layout, SINGLE_VIEW_COLUMN_SET);
  layout->StartRow(0, SINGLE_VIEW_COLUMN_SET);

  // If we have a list of passwords to store for the current site, display
  // them to the user for management. Otherwise, render a "No passwords for
  // this site" message.
  if (!parent_->model()->local_credentials().empty()) {
    ManagePasswordItemsView* item = new ManagePasswordItemsView(
        parent_->model(), &parent_->model()->local_credentials());
    layout->AddView(item);
  } else {
    views::Label* empty_label = new views::Label(
        l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_NO_PASSWORDS),
        CONTEXT_DEPRECATED_SMALL);
    empty_label->SetMultiLine(true);
    empty_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    layout->AddView(empty_label);
  }

  // Then add the "manage passwords" link and "Done" button.
  manage_link_ = new views::Link(parent_->model()->manage_link());
  manage_link_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  manage_link_->SetUnderline(false);
  manage_link_->set_listener(this);

  done_button_ = views::MdTextButton::CreateSecondaryUiButton(
      this, l10n_util::GetStringUTF16(IDS_DONE));

  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  layout->AddPaddingRow(
      0,
      layout_provider->GetInsetsMetric(views::INSETS_DIALOG_CONTENTS).bottom());
  BuildColumnSet(layout, LINK_BUTTON_COLUMN_SET);
  layout->StartRowWithPadding(
      0, LINK_BUTTON_COLUMN_SET, 0,
      layout_provider->GetInsetsMetric(views::INSETS_DIALOG_BUTTON_ROW).top());
  layout->AddView(manage_link_);
  layout->AddView(done_button_);

  parent_->set_initially_focused_view(done_button_);
}

ManagePasswordsBubbleView::ManageView::~ManageView() {
}

void ManagePasswordsBubbleView::ManageView::ButtonPressed(
    views::Button* sender,
    const ui::Event& event) {
  DCHECK(sender == done_button_);
  parent_->model()->OnDoneClicked();
  parent_->CloseBubble();
}

void ManagePasswordsBubbleView::ManageView::LinkClicked(views::Link* source,
                                                        int event_flags) {
  DCHECK_EQ(source, manage_link_);
  parent_->model()->OnManageLinkClicked();
  parent_->CloseBubble();
}

// ManagePasswordsBubbleView::SaveConfirmationView ----------------------------

// A view confirming to the user that a password was saved and offering a link
// to the Google account manager.
class ManagePasswordsBubbleView::SaveConfirmationView
    : public views::View,
      public views::ButtonListener,
      public views::StyledLabelListener {
 public:
  explicit SaveConfirmationView(ManagePasswordsBubbleView* parent);
  ~SaveConfirmationView() override;

 private:
  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::StyledLabelListener implementation
  void StyledLabelLinkClicked(views::StyledLabel* label,
                              const gfx::Range& range,
                              int event_flags) override;

  ManagePasswordsBubbleView* parent_;
  views::Button* ok_button_;

  DISALLOW_COPY_AND_ASSIGN(SaveConfirmationView);
};

ManagePasswordsBubbleView::SaveConfirmationView::SaveConfirmationView(
    ManagePasswordsBubbleView* parent)
    : parent_(parent) {
  views::GridLayout* layout = new views::GridLayout(this);
  layout->set_minimum_size(gfx::Size(kDesiredBubbleWidth, 0));
  SetLayoutManager(layout);

  views::StyledLabel* confirmation =
      new views::StyledLabel(parent_->model()->save_confirmation_text(), this);
  confirmation->SetBaseFontList(views::style::GetFont(
      CONTEXT_DEPRECATED_SMALL, views::style::STYLE_PRIMARY));
  confirmation->AddStyleRange(parent_->model()->save_confirmation_link_range(),
                              GetLinkStyle());

  BuildColumnSet(layout, SINGLE_VIEW_COLUMN_SET);
  layout->StartRow(0, SINGLE_VIEW_COLUMN_SET);
  layout->AddView(confirmation);

  ok_button_ = views::MdTextButton::CreateSecondaryUiButton(
      this, l10n_util::GetStringUTF16(IDS_OK));

  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  layout->AddPaddingRow(
      0,
      layout_provider->GetInsetsMetric(views::INSETS_DIALOG_CONTENTS).bottom());
  BuildColumnSet(layout, SINGLE_BUTTON_COLUMN_SET);
  layout->StartRowWithPadding(
      0, SINGLE_BUTTON_COLUMN_SET, 0,
      layout_provider->GetInsetsMetric(views::INSETS_DIALOG_BUTTON_ROW).top());
  layout->AddView(ok_button_);

  parent_->set_initially_focused_view(ok_button_);
}

ManagePasswordsBubbleView::SaveConfirmationView::~SaveConfirmationView() {
}

void ManagePasswordsBubbleView::SaveConfirmationView::StyledLabelLinkClicked(
    views::StyledLabel* label,
    const gfx::Range& range,
    int event_flags) {
  DCHECK_EQ(range, parent_->model()->save_confirmation_link_range());
  parent_->model()->OnNavigateToPasswordManagerAccountDashboardLinkClicked();
  parent_->CloseBubble();
}

void ManagePasswordsBubbleView::SaveConfirmationView::ButtonPressed(
    views::Button* sender, const ui::Event& event) {
  DCHECK_EQ(sender, ok_button_);
  parent_->model()->OnOKClicked();
  parent_->CloseBubble();
}

// ManagePasswordsBubbleView::SignInPromoView ---------------------------------

// A view that offers user to sign in to Chrome.
class ManagePasswordsBubbleView::SignInPromoView
    : public views::View,
      public views::ButtonListener {
 public:
  explicit SignInPromoView(ManagePasswordsBubbleView* parent);

 private:
  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  ManagePasswordsBubbleView* parent_;

  views::Button* signin_button_;
  views::Button* no_button_;

  DISALLOW_COPY_AND_ASSIGN(SignInPromoView);
};

ManagePasswordsBubbleView::SignInPromoView::SignInPromoView(
    ManagePasswordsBubbleView* parent)
    : parent_(parent) {
  views::GridLayout* layout = new views::GridLayout(this);
  layout->set_minimum_size(gfx::Size(kDesiredBubbleWidth, 0));
  SetLayoutManager(layout);

  signin_button_ = views::MdTextButton::CreateSecondaryUiBlueButton(
      this,
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_SIGNIN_PROMO_SIGN_IN));
  no_button_ = views::MdTextButton::CreateSecondaryUiButton(
      this,
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_SIGNIN_PROMO_NO_THANKS));

  // Button row.
  BuildColumnSet(layout, DOUBLE_BUTTON_COLUMN_SET);
  layout->StartRow(0, DOUBLE_BUTTON_COLUMN_SET);
  layout->AddView(signin_button_);
  layout->AddView(no_button_);

  parent_->set_initially_focused_view(signin_button_);
  base::RecordAction(
      base::UserMetricsAction("Signin_Impression_FromPasswordBubble"));
}

void ManagePasswordsBubbleView::SignInPromoView::ButtonPressed(
    views::Button* sender,
    const ui::Event& event) {
  if (sender == signin_button_)
    parent_->model()->OnSignInToChromeClicked();
  else if (sender == no_button_)
    parent_->model()->OnSkipSignInClicked();
  else
    NOTREACHED();

  parent_->CloseBubble();
}

// ManagePasswordsBubbleView::UpdatePendingView -------------------------------

// A view offering the user the ability to update credentials. Contains a
// single ManagePasswordItemsView (in case of one credentials) or
// CredentialsSelectionView otherwise, along with a "Update Passwords" button
// and a rejection button.
class ManagePasswordsBubbleView::UpdatePendingView
    : public views::View,
      public views::ButtonListener {
 public:
  explicit UpdatePendingView(ManagePasswordsBubbleView* parent);
  ~UpdatePendingView() override;

 private:
  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  ManagePasswordsBubbleView* parent_;

  CredentialsSelectionView* selection_view_;

  views::Button* update_button_;
  views::Button* nope_button_;

  DISALLOW_COPY_AND_ASSIGN(UpdatePendingView);
};

ManagePasswordsBubbleView::UpdatePendingView::UpdatePendingView(
    ManagePasswordsBubbleView* parent)
    : parent_(parent), selection_view_(nullptr) {
  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  views::GridLayout* layout = new views::GridLayout(this);
  layout->set_minimum_size(gfx::Size(kDesiredBubbleWidth, 0));
  SetLayoutManager(layout);

  // Credential row.
  if (parent->model()->ShouldShowMultipleAccountUpdateUI()) {
    BuildColumnSet(layout, SINGLE_VIEW_COLUMN_SET);
    layout->StartRow(0, SINGLE_VIEW_COLUMN_SET);
    layout->AddView(new CredentialsSelectionView(parent->model()));
  } else {
    BuildColumnSet(layout, DOUBLE_VIEW_COLUMN_SET);
    layout->StartRow(0, DOUBLE_VIEW_COLUMN_SET);
    const autofill::PasswordForm* password_form =
        &parent_->model()->pending_password();
    layout->AddView(GenerateUsernameLabel(*password_form).release());
    layout->AddView(GeneratePasswordLabel(*password_form).release());
  }
  layout->AddPaddingRow(
      0,
      layout_provider->GetInsetsMetric(views::INSETS_DIALOG_CONTENTS).bottom());

  // Button row.
  nope_button_ = views::MdTextButton::CreateSecondaryUiButton(
      this, l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_CANCEL_BUTTON));

  update_button_ = views::MdTextButton::CreateSecondaryUiBlueButton(
      this, l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_UPDATE_BUTTON));
  BuildColumnSet(layout, DOUBLE_BUTTON_COLUMN_SET);
  layout->StartRowWithPadding(
      0, DOUBLE_BUTTON_COLUMN_SET, 0,
      layout_provider->GetInsetsMetric(views::INSETS_DIALOG_BUTTON_ROW).top());
  layout->AddView(update_button_);
  layout->AddView(nope_button_);

  parent_->set_initially_focused_view(update_button_);
}

ManagePasswordsBubbleView::UpdatePendingView::~UpdatePendingView() {}

void ManagePasswordsBubbleView::UpdatePendingView::ButtonPressed(
    views::Button* sender,
    const ui::Event& event) {
  DCHECK(sender == update_button_ || sender == nope_button_);
  if (sender == update_button_) {
    if (selection_view_) {
      // Multi account case.
      parent_->model()->OnUpdateClicked(
          *selection_view_->GetSelectedCredentials());
    } else {
      parent_->model()->OnUpdateClicked(parent_->model()->pending_password());
    }
  } else {
    parent_->model()->OnNopeUpdateClicked();
  }
  parent_->CloseBubble();
}

// ManagePasswordsBubbleView --------------------------------------------------

// static
ManagePasswordsBubbleView* ManagePasswordsBubbleView::manage_passwords_bubble_ =
    NULL;

#if !defined(OS_MACOSX) || BUILDFLAG(MAC_VIEWS_BROWSER)
// static
void ManagePasswordsBubbleView::ShowBubble(
    content::WebContents* web_contents,
    DisplayReason reason) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  DCHECK(browser);
  DCHECK(browser->window());
  DCHECK(!manage_passwords_bubble_ ||
         !manage_passwords_bubble_->GetWidget()->IsVisible());

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  bool is_fullscreen = browser_view->IsFullscreen();
  views::View* anchor_view = nullptr;
  if (!is_fullscreen) {
    if (ui::MaterialDesignController::IsSecondaryUiMaterial()) {
      anchor_view = browser_view->GetLocationBarView();
    } else {
      anchor_view =
          browser_view->GetLocationBarView()->manage_passwords_icon_view();
    }
  }
  new ManagePasswordsBubbleView(web_contents, anchor_view, gfx::Point(),
                                reason);
  DCHECK(manage_passwords_bubble_);

  if (is_fullscreen)
    manage_passwords_bubble_->set_parent_window(web_contents->GetNativeView());

  views::Widget* manage_passwords_bubble_widget =
      views::BubbleDialogDelegateView::CreateBubble(manage_passwords_bubble_);
  if (anchor_view) {
    manage_passwords_bubble_widget->AddObserver(
        browser_view->GetLocationBarView()->manage_passwords_icon_view());
  }

  // Adjust for fullscreen after creation as it relies on the content size.
  if (is_fullscreen) {
    manage_passwords_bubble_->AdjustForFullscreen(
        browser_view->GetBoundsInScreen());
  }

  manage_passwords_bubble_->ShowForReason(reason);
}
#endif  // !defined(OS_MACOSX) || BUILDFLAG(MAC_VIEWS_BROWSER)

// static
void ManagePasswordsBubbleView::CloseCurrentBubble() {
  if (manage_passwords_bubble_)
    manage_passwords_bubble_->CloseBubble();
}

// static
void ManagePasswordsBubbleView::ActivateBubble() {
  DCHECK(manage_passwords_bubble_);
  DCHECK(manage_passwords_bubble_->GetWidget()->IsVisible());
  manage_passwords_bubble_->GetWidget()->Activate();
}

content::WebContents* ManagePasswordsBubbleView::web_contents() const {
  return model_.GetWebContents();
}

ManagePasswordsBubbleView::ManagePasswordsBubbleView(
    content::WebContents* web_contents,
    views::View* anchor_view,
    const gfx::Point& anchor_point,
    DisplayReason reason)
    : LocationBarBubbleDelegateView(anchor_view, anchor_point, web_contents),
      model_(PasswordsModelDelegateFromWebContents(web_contents),
             reason == AUTOMATIC ? ManagePasswordsBubbleModel::AUTOMATIC
                                 : ManagePasswordsBubbleModel::USER_ACTION),
      initially_focused_view_(nullptr) {
  mouse_handler_.reset(new WebContentMouseHandler(this, this->web_contents()));
  manage_passwords_bubble_ = this;
  chrome::RecordDialogCreation(chrome::DialogIdentifier::MANAGE_PASSWORDS);
}

ManagePasswordsBubbleView::~ManagePasswordsBubbleView() {
  if (manage_passwords_bubble_ == this)
    manage_passwords_bubble_ = nullptr;
}

int ManagePasswordsBubbleView::GetDialogButtons() const {
  // TODO(tapted): DialogClientView should manage buttons.
  return ui::DIALOG_BUTTON_NONE;
}

views::View* ManagePasswordsBubbleView::GetInitiallyFocusedView() {
  return initially_focused_view_;
}

void ManagePasswordsBubbleView::Init() {
  SetLayoutManager(new views::FillLayout);

  CreateChild();
}

void ManagePasswordsBubbleView::CloseBubble() {
  mouse_handler_.reset();
  LocationBarBubbleDelegateView::CloseBubble();
}

void ManagePasswordsBubbleView::AddedToWidget() {
  auto title_view =
      base::MakeUnique<views::StyledLabel>(base::string16(), this);
  title_view->SetBaseFontList(views::style::GetFont(
      views::style::CONTEXT_DIALOG_TITLE, views::style::STYLE_PRIMARY));
  UpdateTitleText(title_view.get());
  GetBubbleFrameView()->SetTitleView(std::move(title_view));
}

void ManagePasswordsBubbleView::UpdateTitleText(
    views::StyledLabel* title_view) {
  title_view->SetText(GetWindowTitle());
  if (!model_.title_brand_link_range().is_empty())
    title_view->AddStyleRange(model_.title_brand_link_range(), GetLinkStyle());
}

base::string16 ManagePasswordsBubbleView::GetWindowTitle() const {
  return model_.title();
}

gfx::ImageSkia ManagePasswordsBubbleView::GetWindowIcon() {
#if defined(OS_WIN)
  if (model_.state() == password_manager::ui::CHROME_DESKTOP_IOS_PROMO_STATE) {
    return desktop_ios_promotion::GetPromoImage(
        GetNativeTheme()->GetSystemColor(
            ui::NativeTheme::kColorId_TextfieldDefaultColor));
  }
#endif
  return gfx::ImageSkia();
}

bool ManagePasswordsBubbleView::ShouldShowWindowIcon() const {
  return model_.state() == password_manager::ui::CHROME_DESKTOP_IOS_PROMO_STATE;
}

bool ManagePasswordsBubbleView::ShouldShowCloseButton() const {
  return model_.state() == password_manager::ui::PENDING_PASSWORD_STATE ||
         model_.state() == password_manager::ui::CHROME_SIGN_IN_PROMO_STATE ||
         model_.state() == password_manager::ui::CHROME_DESKTOP_IOS_PROMO_STATE;
}

void ManagePasswordsBubbleView::StyledLabelLinkClicked(
    views::StyledLabel* label,
    const gfx::Range& range,
    int event_flags) {
  DCHECK_EQ(model_.title_brand_link_range(), range);
  model_.OnBrandLinkClicked();
}

void ManagePasswordsBubbleView::Refresh() {
  RemoveAllChildViews(true);
  initially_focused_view_ = NULL;
  CreateChild();
  // Show/hide the close button.
  GetWidget()->non_client_view()->ResetWindowControls();
  GetWidget()->UpdateWindowIcon();
  UpdateTitleText(
      static_cast<views::StyledLabel*>(GetBubbleFrameView()->title()));
  if (model_.state() == password_manager::ui::CHROME_DESKTOP_IOS_PROMO_STATE) {
    // Update the height and keep the existing width.
    gfx::Rect bubble_bounds = GetWidget()->GetWindowBoundsInScreen();
    bubble_bounds.set_height(
        GetWidget()->GetRootView()->GetHeightForWidth(bubble_bounds.width()));
    GetWidget()->SetBounds(bubble_bounds);
  } else {
    SizeToContents();
  }
}

void ManagePasswordsBubbleView::CreateChild() {
  if (model_.state() == password_manager::ui::PENDING_PASSWORD_STATE) {
    AddChildView(new PendingView(this));
  } else if (model_.state() ==
             password_manager::ui::PENDING_PASSWORD_UPDATE_STATE) {
    AddChildView(new UpdatePendingView(this));
  } else if (model_.state() == password_manager::ui::CONFIRMATION_STATE) {
    AddChildView(new SaveConfirmationView(this));
  } else if (model_.state() == password_manager::ui::AUTO_SIGNIN_STATE) {
    AddChildView(new AutoSigninView(this));
  } else if (model_.state() ==
             password_manager::ui::CHROME_SIGN_IN_PROMO_STATE) {
    AddChildView(new SignInPromoView(this));
#if defined(OS_WIN)
  } else if (model_.state() ==
             password_manager::ui::CHROME_DESKTOP_IOS_PROMO_STATE) {
    AddChildView(new DesktopIOSPromotionBubbleView(
        model_.GetProfile(),
        desktop_ios_promotion::PromotionEntryPoint::SAVE_PASSWORD_BUBBLE));
#endif
  } else {
    AddChildView(new ManageView(this));
  }
}
