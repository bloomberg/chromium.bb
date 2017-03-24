// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/manage_passwords_bubble_view.h"

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/timer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/passwords/password_dialog_prompts.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/harmony/layout_delegate.h"
#include "chrome/browser/ui/views/passwords/credentials_item_view.h"
#include "chrome/browser/ui/views/passwords/credentials_selection_view.h"
#include "chrome/browser/ui/views/passwords/manage_password_items_view.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_icon_views.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/user_metrics.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/button/blue_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/styled_label_listener.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

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
  switch (type) {
    case SINGLE_VIEW_COLUMN_SET:
      column_set->AddColumn(views::GridLayout::FILL,
                            views::GridLayout::FILL,
                            0,
                            views::GridLayout::FIXED,
                            full_width,
                            0);
      break;
    case DOUBLE_BUTTON_COLUMN_SET:
      column_set->AddColumn(views::GridLayout::TRAILING,
                            views::GridLayout::CENTER,
                            1,
                            views::GridLayout::USE_PREF,
                            0,
                            0);
      column_set->AddPaddingColumn(0, views::kRelatedButtonHSpacing);
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
      column_set->AddPaddingColumn(0, views::kRelatedButtonHSpacing);
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
      column_set->AddPaddingColumn(0, views::kRelatedButtonHSpacing);
      column_set->AddColumn(views::GridLayout::TRAILING,
                            views::GridLayout::CENTER,
                            0,
                            views::GridLayout::USE_PREF,
                            0,
                            0);
      column_set->AddPaddingColumn(0, views::kRelatedButtonHSpacing);
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

// If a special title is required (i.e. one that contains links), creates a
// title view and a row for it in |layout|.
// TODO(estade): this should be removed and a replaced by a normal title (via
// GetWindowTitle).
void AddTitleRowWithLink(views::GridLayout* layout,
                         ManagePasswordsBubbleModel* model,
                         views::StyledLabelListener* listener) {
  if (model->title_brand_link_range().is_empty())
    return;

  views::StyledLabel* title_label =
      new views::StyledLabel(model->title(), listener);
  title_label->SetBaseFontList(
      ui::ResourceBundle::GetSharedInstance().GetFontList(
          ui::ResourceBundle::MediumFont));
  title_label->AddStyleRange(model->title_brand_link_range(), GetLinkStyle());
  layout->StartRow(0, SINGLE_VIEW_COLUMN_SET);
  layout->AddView(title_label);
  layout->AddPaddingRow(0, views::kUnrelatedControlVerticalSpacing);
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
  CredentialsItemView* credential = new CredentialsItemView(
      this, base::string16(),
      l10n_util::GetStringFUTF16(IDS_MANAGE_PASSWORDS_AUTO_SIGNIN_TITLE,
                                 form.username_value),
      kButtonHoverColor, &form,
      parent_->model()->GetProfile()->GetRequestContext());
  credential->SetEnabled(false);
  AddChildView(credential);

  // Setup the observer and maybe start the timer.
  Browser* browser =
      chrome::FindBrowserWithWebContents(parent_->web_contents());
  DCHECK(browser);
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  observed_browser_.Add(browser_view->GetWidget());

  if (browser_view->IsActive())
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
// single ManagePasswordItemsView, along with a "Save Passwords" button
// and a "Never" button.
class ManagePasswordsBubbleView::PendingView
    : public views::View,
      public views::ButtonListener,
      public views::StyledLabelListener {
 public:
  explicit PendingView(ManagePasswordsBubbleView* parent);
  ~PendingView() override;

 private:
  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::StyledLabelListener:
  void StyledLabelLinkClicked(views::StyledLabel* label,
                              const gfx::Range& range,
                              int event_flags) override;

  ManagePasswordsBubbleView* parent_;

  views::Button* save_button_;
  views::Button* never_button_;

  DISALLOW_COPY_AND_ASSIGN(PendingView);
};

ManagePasswordsBubbleView::PendingView::PendingView(
    ManagePasswordsBubbleView* parent)
    : parent_(parent) {
  views::GridLayout* layout = new views::GridLayout(this);
  layout->set_minimum_size(gfx::Size(kDesiredBubbleWidth, 0));
  SetLayoutManager(layout);

  // Create the pending credential item, save button and refusal combobox.
  ManagePasswordItemsView* item = nullptr;
  if (!parent->model()->pending_password().username_value.empty()) {
    item = new ManagePasswordItemsView(parent_->model(),
                                       &parent->model()->pending_password());
  }
  save_button_ = views::MdTextButton::CreateSecondaryUiBlueButton(
      this, l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_SAVE_BUTTON));
  never_button_ = views::MdTextButton::CreateSecondaryUiButton(
      this,
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_BUBBLE_BLACKLIST_BUTTON));

  // Title row.
  BuildColumnSet(layout, SINGLE_VIEW_COLUMN_SET);
  AddTitleRowWithLink(layout, parent_->model(), this);

  // Credential row.
  if (item) {
    layout->StartRow(0, SINGLE_VIEW_COLUMN_SET);
    layout->AddView(item);
    layout->AddPaddingRow(0, views::kUnrelatedControlVerticalSpacing);
  }

  // Button row.
  BuildColumnSet(layout, DOUBLE_BUTTON_COLUMN_SET);
  layout->StartRow(0, DOUBLE_BUTTON_COLUMN_SET);
  layout->AddView(save_button_);
  layout->AddView(never_button_);

  parent_->set_initially_focused_view(save_button_);
}

ManagePasswordsBubbleView::PendingView::~PendingView() {
}

void ManagePasswordsBubbleView::PendingView::ButtonPressed(
    views::Button* sender,
    const ui::Event& event) {
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

void ManagePasswordsBubbleView::PendingView::StyledLabelLinkClicked(
    views::StyledLabel* label,
    const gfx::Range& range,
    int event_flags) {
  DCHECK_EQ(range, parent_->model()->title_brand_link_range());
  parent_->model()->OnBrandLinkClicked();
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

  // If we have a list of passwords to store for the current site, display
  // them to the user for management. Otherwise, render a "No passwords for
  // this site" message.
  BuildColumnSet(layout, SINGLE_VIEW_COLUMN_SET);
  if (!parent_->model()->local_credentials().empty()) {
    ManagePasswordItemsView* item = new ManagePasswordItemsView(
        parent_->model(), &parent_->model()->local_credentials());
    layout->StartRowWithPadding(0, SINGLE_VIEW_COLUMN_SET, 0,
                                views::kUnrelatedControlVerticalSpacing);
    layout->AddView(item);
  } else {
    views::Label* empty_label = new views::Label(
        l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_NO_PASSWORDS));
    empty_label->SetMultiLine(true);
    empty_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    empty_label->SetFontList(
        ui::ResourceBundle::GetSharedInstance().GetFontList(
            ui::ResourceBundle::SmallFont));

    layout->StartRowWithPadding(0, SINGLE_VIEW_COLUMN_SET, 0,
                                views::kUnrelatedControlVerticalSpacing);
    layout->AddView(empty_label);
  }

  // Then add the "manage passwords" link and "Done" button.
  manage_link_ = new views::Link(parent_->model()->manage_link());
  manage_link_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  manage_link_->SetUnderline(false);
  manage_link_->set_listener(this);

  done_button_ = views::MdTextButton::CreateSecondaryUiButton(
      this, l10n_util::GetStringUTF16(IDS_DONE));

  BuildColumnSet(layout, LINK_BUTTON_COLUMN_SET);
  layout->StartRowWithPadding(0, LINK_BUTTON_COLUMN_SET, 0,
                              views::kUnrelatedControlVerticalSpacing);
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
  confirmation->SetBaseFontList(
      ui::ResourceBundle::GetSharedInstance().GetFontList(
          ui::ResourceBundle::SmallFont));
  confirmation->AddStyleRange(
      parent_->model()->save_confirmation_link_range(), GetLinkStyle());

  BuildColumnSet(layout, SINGLE_VIEW_COLUMN_SET);
  layout->StartRow(0, SINGLE_VIEW_COLUMN_SET);
  layout->AddView(confirmation);

  ok_button_ = views::MdTextButton::CreateSecondaryUiButton(
      this, l10n_util::GetStringUTF16(IDS_OK));

  BuildColumnSet(layout, SINGLE_BUTTON_COLUMN_SET);
  layout->StartRowWithPadding(
      0, SINGLE_BUTTON_COLUMN_SET, 0, views::kRelatedControlVerticalSpacing);
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
  content::RecordAction(
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
      public views::ButtonListener,
      public views::StyledLabelListener {
 public:
  explicit UpdatePendingView(ManagePasswordsBubbleView* parent);
  ~UpdatePendingView() override;

 private:
  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::StyledLabelListener:
  void StyledLabelLinkClicked(views::StyledLabel* label,
                              const gfx::Range& range,
                              int event_flags) override;

  ManagePasswordsBubbleView* parent_;

  CredentialsSelectionView* selection_view_;

  views::Button* update_button_;
  views::Button* nope_button_;

  DISALLOW_COPY_AND_ASSIGN(UpdatePendingView);
};

ManagePasswordsBubbleView::UpdatePendingView::UpdatePendingView(
    ManagePasswordsBubbleView* parent)
    : parent_(parent), selection_view_(nullptr) {
  views::GridLayout* layout = new views::GridLayout(this);
  layout->set_minimum_size(gfx::Size(kDesiredBubbleWidth, 0));
  SetLayoutManager(layout);

  // Create the pending credential item, update button.
  View* item = nullptr;
  if (parent->model()->ShouldShowMultipleAccountUpdateUI()) {
    selection_view_ = new CredentialsSelectionView(parent->model());
    item = selection_view_;
  } else {
    item = new ManagePasswordItemsView(parent_->model(),
                                       &parent->model()->pending_password());
  }
  nope_button_ = views::MdTextButton::CreateSecondaryUiButton(
      this, l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_CANCEL_BUTTON));

  update_button_ = views::MdTextButton::CreateSecondaryUiBlueButton(
      this, l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_UPDATE_BUTTON));

  // Title row.
  BuildColumnSet(layout, SINGLE_VIEW_COLUMN_SET);
  AddTitleRowWithLink(layout, parent_->model(), this);

  // Credential row.
  layout->StartRow(0, SINGLE_VIEW_COLUMN_SET);
  layout->AddView(item);

  // Button row.
  BuildColumnSet(layout, DOUBLE_BUTTON_COLUMN_SET);
  layout->StartRowWithPadding(0, DOUBLE_BUTTON_COLUMN_SET, 0,
                              views::kUnrelatedControlVerticalSpacing);
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

void ManagePasswordsBubbleView::UpdatePendingView::StyledLabelLinkClicked(
    views::StyledLabel* label,
    const gfx::Range& range,
    int event_flags) {
  DCHECK_EQ(range, parent_->model()->title_brand_link_range());
  parent_->model()->OnBrandLinkClicked();
}

// ManagePasswordsBubbleView --------------------------------------------------

// static
ManagePasswordsBubbleView* ManagePasswordsBubbleView::manage_passwords_bubble_ =
    NULL;

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
  manage_passwords_bubble_ = new ManagePasswordsBubbleView(
      web_contents, anchor_view, reason);

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
    DisplayReason reason)
    : LocationBarBubbleDelegateView(anchor_view, web_contents),
      model_(PasswordsModelDelegateFromWebContents(web_contents),
             reason == AUTOMATIC ? ManagePasswordsBubbleModel::AUTOMATIC
                                 : ManagePasswordsBubbleModel::USER_ACTION),
      initially_focused_view_(nullptr) {
  mouse_handler_.reset(new WebContentMouseHandler(this, this->web_contents()));
  // Set title margins to make the title and the content left aligned.
  const int side_margin = margins().left();
  set_title_margins(
      gfx::Insets(LayoutDelegate::Get()->GetMetric(
                      LayoutDelegate::Metric::PANEL_CONTENT_MARGIN),
                  side_margin, 0, side_margin));
}

ManagePasswordsBubbleView::~ManagePasswordsBubbleView() {
  if (manage_passwords_bubble_ == this)
    manage_passwords_bubble_ = NULL;
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

bool ManagePasswordsBubbleView::ShouldShowWindowTitle() const {
  // Since bubble titles don't support links, fall back to a custom title view
  // if we need to show a link. Only use the normal title path if there's no
  // link.
  return model_.title_brand_link_range().is_empty();
}

bool ManagePasswordsBubbleView::ShouldShowWindowIcon() const {
  return model_.state() == password_manager::ui::CHROME_DESKTOP_IOS_PROMO_STATE;
}

bool ManagePasswordsBubbleView::ShouldShowCloseButton() const {
  return model_.state() == password_manager::ui::PENDING_PASSWORD_STATE ||
         model_.state() == password_manager::ui::CHROME_SIGN_IN_PROMO_STATE ||
         model_.state() == password_manager::ui::CHROME_DESKTOP_IOS_PROMO_STATE;
}

void ManagePasswordsBubbleView::Refresh() {
  RemoveAllChildViews(true);
  initially_focused_view_ = NULL;
  CreateChild();

  // Show/hide the close button.
  GetWidget()->non_client_view()->ResetWindowControls();
  GetWidget()->UpdateWindowIcon();
  GetWidget()->UpdateWindowTitle();
  SizeToContents();
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
