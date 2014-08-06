// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/manage_passwords_bubble_view.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/passwords/manage_password_item_view.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_icon_view.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "content/public/browser/notification_source.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/controls/button/blue_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"


// Helpers --------------------------------------------------------------------

namespace {

// The number of seconds the inactive bubble should stay alive.
const int kBubbleCloseDelay = 15;

const int kDesiredBubbleWidth = 370;

enum ColumnSetType {
  // | | (FILL, FILL) | |
  // Used for the bubble's header, the credentials list, and for simple
  // messages like "No passwords".
  SINGLE_VIEW_COLUMN_SET = 0,

  // | | (TRAILING, CENTER) | | (TRAILING, CENTER) | |
  // Used for buttons at the bottom of the bubble which should nest at the
  // bottom-right corner.
  DOUBLE_BUTTON_COLUMN_SET = 1,

  // | | (LEADING, CENTER) | | (TRAILING, CENTER) | |
  // Used for buttons at the bottom of the bubble which should occupy
  // the corners.
  LINK_BUTTON_COLUMN_SET = 2,

  // | | (TRAILING, CENTER) | |
  // Used when there is only one button which should next at the bottom-right
  // corner.
  SINGLE_BUTTON_COLUMN_SET = 3,
};

// Construct an appropriate ColumnSet for the given |type|, and add it
// to |layout|.
void BuildColumnSet(views::GridLayout* layout, ColumnSetType type) {
  views::ColumnSet* column_set = layout->AddColumnSet(type);
  column_set->AddPaddingColumn(0, views::kPanelHorizMargin);
  int full_width = kDesiredBubbleWidth - (2 * views::kPanelHorizMargin);
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
  }
  column_set->AddPaddingColumn(0, views::kPanelHorizMargin);
}

// Given a layout and a model, add an appropriate title using a
// SINGLE_VIEW_COLUMN_SET, followed by a spacer row.
void AddTitleRow(views::GridLayout* layout, ManagePasswordsBubbleModel* model) {
  views::Label* title_label = new views::Label(model->title());
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label->SetMultiLine(true);
  title_label->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::MediumFont));

  // Add the title to the layout with appropriate padding.
  layout->StartRowWithPadding(
      0, SINGLE_VIEW_COLUMN_SET, 0, views::kRelatedControlSmallVerticalSpacing);
  layout->AddView(title_label);
  layout->AddPaddingRow(0, views::kUnrelatedControlVerticalSpacing);
}

}  // namespace


// Globals --------------------------------------------------------------------

namespace chrome {

void ShowManagePasswordsBubble(content::WebContents* web_contents) {
  if (ManagePasswordsBubbleView::IsShowing()) {
    // The bubble is currently shown for some other tab. We should close it now
    // and open for |web_contents|.
    ManagePasswordsBubbleView::CloseBubble();
  }
  ManagePasswordsUIController* controller =
      ManagePasswordsUIController::FromWebContents(web_contents);
  ManagePasswordsBubbleView::ShowBubble(
      web_contents,
      password_manager::ui::IsAutomaticDisplayState(controller->state())
          ? ManagePasswordsBubbleView::AUTOMATIC
          : ManagePasswordsBubbleView::USER_ACTION);
}

}  // namespace chrome


// ManagePasswordsBubbleView::PendingView -------------------------------------

ManagePasswordsBubbleView::PendingView::PendingView(
    ManagePasswordsBubbleView* parent)
    : parent_(parent) {
  views::GridLayout* layout = new views::GridLayout(this);
  layout->set_minimum_size(gfx::Size(kDesiredBubbleWidth, 0));
  SetLayoutManager(layout);

  // Create the pending credential item, save button and refusal combobox.
  ManagePasswordItemView* item =
      new ManagePasswordItemView(parent_->model(),
                                 parent_->model()->pending_credentials(),
                                 ManagePasswordItemView::FIRST_ITEM);
  save_button_ = new views::BlueButton(
      this, l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_SAVE_BUTTON));
  save_button_->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::SmallFont));

  combobox_model_.reset(new SavePasswordRefusalComboboxModel());
  refuse_combobox_.reset(new views::Combobox(combobox_model_.get()));
  refuse_combobox_->set_listener(this);
  refuse_combobox_->SetStyle(views::Combobox::STYLE_ACTION);
  // TODO(mkwst): Need a mechanism to pipe a font list down into a combobox.

  // Title row.
  BuildColumnSet(layout, SINGLE_VIEW_COLUMN_SET);
  AddTitleRow(layout, parent_->model());

  // Credential row.
  layout->StartRow(0, SINGLE_VIEW_COLUMN_SET);
  layout->AddView(item);

  // Button row.
  BuildColumnSet(layout, DOUBLE_BUTTON_COLUMN_SET);
  layout->StartRowWithPadding(
      0, DOUBLE_BUTTON_COLUMN_SET, 0, views::kRelatedControlVerticalSpacing);
  layout->AddView(save_button_);
  layout->AddView(refuse_combobox_.get());

  // Extra padding for visual awesomeness.
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  parent_->set_initially_focused_view(save_button_);
}

ManagePasswordsBubbleView::PendingView::~PendingView() {
}

void ManagePasswordsBubbleView::PendingView::ButtonPressed(
    views::Button* sender,
    const ui::Event& event) {
  DCHECK(sender == save_button_);
  parent_->model()->OnSaveClicked();
  parent_->Close();
}

void ManagePasswordsBubbleView::PendingView::OnPerformAction(
    views::Combobox* source) {
  DCHECK_EQ(source, refuse_combobox_);
  switch (refuse_combobox_->selected_index()) {
    case SavePasswordRefusalComboboxModel::INDEX_NOPE:
      parent_->model()->OnNopeClicked();
      parent_->Close();
      break;
    case SavePasswordRefusalComboboxModel::INDEX_NEVER_FOR_THIS_SITE:
      parent_->NotifyNeverForThisSiteClicked();
      break;
  }
}

// ManagePasswordsBubbleView::ConfirmNeverView ---------------------------------

ManagePasswordsBubbleView::ConfirmNeverView::ConfirmNeverView(
    ManagePasswordsBubbleView* parent)
    : parent_(parent) {
  views::GridLayout* layout = new views::GridLayout(this);
  layout->set_minimum_size(gfx::Size(kDesiredBubbleWidth, 0));
  SetLayoutManager(layout);

  // Title row.
  BuildColumnSet(layout, SINGLE_VIEW_COLUMN_SET);
  views::Label* title_label = new views::Label(l10n_util::GetStringUTF16(
      IDS_MANAGE_PASSWORDS_BLACKLIST_CONFIRMATION_TITLE));
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label->SetMultiLine(true);
  title_label->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::MediumFont));
  layout->StartRowWithPadding(
      0, SINGLE_VIEW_COLUMN_SET, 0, views::kRelatedControlSmallVerticalSpacing);
  layout->AddView(title_label);
  layout->AddPaddingRow(0, views::kUnrelatedControlVerticalSpacing);

  // Confirmation text.
  views::Label* confirmation = new views::Label(l10n_util::GetStringUTF16(
      IDS_MANAGE_PASSWORDS_BLACKLIST_CONFIRMATION_TEXT));
  confirmation->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  confirmation->SetMultiLine(true);
  confirmation->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::SmallFont));
  layout->StartRow(0, SINGLE_VIEW_COLUMN_SET);
  layout->AddView(confirmation);
  layout->AddPaddingRow(0, views::kRelatedControlSmallVerticalSpacing);

  // Confirm and undo buttons.
  BuildColumnSet(layout, DOUBLE_BUTTON_COLUMN_SET);
  layout->StartRowWithPadding(
      0, DOUBLE_BUTTON_COLUMN_SET, 0, views::kRelatedControlVerticalSpacing);

  confirm_button_ = new views::LabelButton(
      this,
      l10n_util::GetStringUTF16(
          IDS_MANAGE_PASSWORDS_BLACKLIST_CONFIRMATION_BUTTON));
  confirm_button_->SetStyle(views::Button::STYLE_BUTTON);
  confirm_button_->SetFontList(
      ui::ResourceBundle::GetSharedInstance().GetFontList(
          ui::ResourceBundle::SmallFont));
  layout->AddView(confirm_button_);

  undo_button_ =
      new views::LabelButton(this, l10n_util::GetStringUTF16(IDS_CANCEL));
  undo_button_->SetStyle(views::Button::STYLE_BUTTON);
  undo_button_->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::SmallFont));
  layout->AddView(undo_button_);

  // Extra padding for visual awesomeness.
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  parent_->set_initially_focused_view(confirm_button_);
}

ManagePasswordsBubbleView::ConfirmNeverView::~ConfirmNeverView() {
}

void ManagePasswordsBubbleView::ConfirmNeverView::ButtonPressed(
    views::Button* sender,
    const ui::Event& event) {
  DCHECK(sender == confirm_button_ || sender == undo_button_);
  if (sender == confirm_button_)
    parent_->NotifyConfirmedNeverForThisSite();
  else
    parent_->NotifyUndoNeverForThisSite();
}

// ManagePasswordsBubbleView::ManageView --------------------------------------

ManagePasswordsBubbleView::ManageView::ManageView(
    ManagePasswordsBubbleView* parent)
    : parent_(parent) {
  views::GridLayout* layout = new views::GridLayout(this);
  layout->set_minimum_size(gfx::Size(kDesiredBubbleWidth, 0));
  SetLayoutManager(layout);

  // Add the title.
  BuildColumnSet(layout, SINGLE_VIEW_COLUMN_SET);
  AddTitleRow(layout, parent_->model());

  // If we have a list of passwords to store for the current site, display
  // them to the user for management. Otherwise, render a "No passwords for
  // this site" message.
  if (!parent_->model()->best_matches().empty()) {
    for (autofill::ConstPasswordFormMap::const_iterator i(
             parent_->model()->best_matches().begin());
         i != parent_->model()->best_matches().end();
         ++i) {
      ManagePasswordItemView* item = new ManagePasswordItemView(
          parent_->model(),
          *i->second,
          i == parent_->model()->best_matches().begin()
              ? ManagePasswordItemView::FIRST_ITEM
              : ManagePasswordItemView::SUBSEQUENT_ITEM);

      layout->StartRow(0, SINGLE_VIEW_COLUMN_SET);
      layout->AddView(item);
    }
  } else {
    views::Label* empty_label = new views::Label(
        l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_NO_PASSWORDS));
    empty_label->SetMultiLine(true);
    empty_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    empty_label->SetFontList(
        ui::ResourceBundle::GetSharedInstance().GetFontList(
            ui::ResourceBundle::SmallFont));

    layout->StartRow(0, SINGLE_VIEW_COLUMN_SET);
    layout->AddView(empty_label);
    layout->AddPaddingRow(0, views::kRelatedControlSmallVerticalSpacing);
  }

  // Then add the "manage passwords" link and "Done" button.
  manage_link_ = new views::Link(parent_->model()->manage_link());
  manage_link_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  manage_link_->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::SmallFont));
  manage_link_->SetUnderline(false);
  manage_link_->set_listener(this);

  done_button_ =
      new views::LabelButton(this, l10n_util::GetStringUTF16(IDS_DONE));
  done_button_->SetStyle(views::Button::STYLE_BUTTON);
  done_button_->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::SmallFont));

  BuildColumnSet(layout, LINK_BUTTON_COLUMN_SET);
  layout->StartRowWithPadding(
      0, LINK_BUTTON_COLUMN_SET, 0, views::kRelatedControlVerticalSpacing);
  layout->AddView(manage_link_);
  layout->AddView(done_button_);

  // Extra padding for visual awesomeness.
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  parent_->set_initially_focused_view(done_button_);
}

ManagePasswordsBubbleView::ManageView::~ManageView() {
}

void ManagePasswordsBubbleView::ManageView::ButtonPressed(
    views::Button* sender,
    const ui::Event& event) {
  DCHECK(sender == done_button_);
  parent_->model()->OnDoneClicked();
  parent_->Close();
}

void ManagePasswordsBubbleView::ManageView::LinkClicked(views::Link* source,
                                                        int event_flags) {
  DCHECK_EQ(source, manage_link_);
  parent_->model()->OnManageLinkClicked();
  parent_->Close();
}

// ManagePasswordsBubbleView::BlacklistedView ---------------------------------

ManagePasswordsBubbleView::BlacklistedView::BlacklistedView(
    ManagePasswordsBubbleView* parent)
    : parent_(parent) {
  views::GridLayout* layout = new views::GridLayout(this);
  layout->set_minimum_size(gfx::Size(kDesiredBubbleWidth, 0));
  SetLayoutManager(layout);

  // Add the title.
  BuildColumnSet(layout, SINGLE_VIEW_COLUMN_SET);
  AddTitleRow(layout, parent_->model());

  // Add the "Hey! You blacklisted this site!" text.
  views::Label* blacklisted = new views::Label(
      l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_BLACKLISTED));
  blacklisted->SetMultiLine(true);
  blacklisted->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::SmallFont));
  layout->StartRow(0, SINGLE_VIEW_COLUMN_SET);
  layout->AddView(blacklisted);
  layout->AddPaddingRow(0, views::kRelatedControlSmallVerticalSpacing);

  // Then add the "enable password manager" and "Done" buttons.
  unblacklist_button_ = new views::BlueButton(
      this, l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_UNBLACKLIST_BUTTON));
  unblacklist_button_->SetFontList(
      ui::ResourceBundle::GetSharedInstance().GetFontList(
          ui::ResourceBundle::SmallFont));
  done_button_ =
      new views::LabelButton(this, l10n_util::GetStringUTF16(IDS_DONE));
  done_button_->SetStyle(views::Button::STYLE_BUTTON);
  done_button_->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::SmallFont));

  BuildColumnSet(layout, DOUBLE_BUTTON_COLUMN_SET);
  layout->StartRowWithPadding(
      0, DOUBLE_BUTTON_COLUMN_SET, 0, views::kRelatedControlVerticalSpacing);
  layout->AddView(unblacklist_button_);
  layout->AddView(done_button_);

  // Extra padding for visual awesomeness.
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  parent_->set_initially_focused_view(unblacklist_button_);
}

ManagePasswordsBubbleView::BlacklistedView::~BlacklistedView() {
}

void ManagePasswordsBubbleView::BlacklistedView::ButtonPressed(
    views::Button* sender,
    const ui::Event& event) {
  if (sender == done_button_)
    parent_->model()->OnDoneClicked();
  else if (sender == unblacklist_button_)
    parent_->model()->OnUnblacklistClicked();
  else
    NOTREACHED();
  parent_->Close();
}

// ManagePasswordsBubbleView::SaveConfirmationView ----------------------------

ManagePasswordsBubbleView::SaveConfirmationView::SaveConfirmationView(
    ManagePasswordsBubbleView* parent)
    : parent_(parent) {
  views::GridLayout* layout = new views::GridLayout(this);
  layout->set_minimum_size(gfx::Size(kDesiredBubbleWidth, 0));
  SetLayoutManager(layout);

  BuildColumnSet(layout, SINGLE_VIEW_COLUMN_SET);
  AddTitleRow(layout, parent_->model());

  views::StyledLabel* confirmation =
      new views::StyledLabel(parent_->model()->save_confirmation_text(), this);
  confirmation->SetBaseFontList(
      ui::ResourceBundle::GetSharedInstance().GetFontList(
          ui::ResourceBundle::SmallFont));
  confirmation->AddStyleRange(
      parent_->model()->save_confirmation_link_range(),
      views::StyledLabel::RangeStyleInfo::CreateForLink());

  layout->StartRow(0, SINGLE_VIEW_COLUMN_SET);
  layout->AddView(confirmation);

  ok_button_ = new views::LabelButton(this, l10n_util::GetStringUTF16(IDS_OK));
  ok_button_->SetStyle(views::Button::STYLE_BUTTON);
  ok_button_->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::SmallFont));

  BuildColumnSet(layout, SINGLE_BUTTON_COLUMN_SET);
  layout->StartRowWithPadding(
      0, SINGLE_BUTTON_COLUMN_SET, 0, views::kRelatedControlVerticalSpacing);
  layout->AddView(ok_button_);

  // Extra padding for visual awesomeness.
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  parent_->set_initially_focused_view(ok_button_);
}

ManagePasswordsBubbleView::SaveConfirmationView::~SaveConfirmationView() {
}

void ManagePasswordsBubbleView::SaveConfirmationView::StyledLabelLinkClicked(
    const gfx::Range& range, int event_flags) {
  DCHECK_EQ(range, parent_->model()->save_confirmation_link_range());
  parent_->model()->OnRemoteManageLinkClicked();
  parent_->Close();
}

void ManagePasswordsBubbleView::SaveConfirmationView::ButtonPressed(
    views::Button* sender, const ui::Event& event) {
  DCHECK_EQ(sender, ok_button_);
  parent_->model()->OnOKClicked();
  parent_->Close();
}

// ManagePasswordsBubbleView --------------------------------------------------

// static
ManagePasswordsBubbleView* ManagePasswordsBubbleView::manage_passwords_bubble_ =
    NULL;

// static
void ManagePasswordsBubbleView::ShowBubble(content::WebContents* web_contents,
                                           DisplayReason reason) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  DCHECK(browser);
  DCHECK(browser->window());
  DCHECK(browser->fullscreen_controller());

  if (IsShowing())
    return;

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  bool is_fullscreen = browser_view->IsFullscreen();
  ManagePasswordsIconView* anchor_view =
      is_fullscreen
          ? NULL
          : browser_view->GetLocationBarView()->manage_passwords_icon_view();
  manage_passwords_bubble_ = new ManagePasswordsBubbleView(
      web_contents, anchor_view, reason);

  if (is_fullscreen) {
    manage_passwords_bubble_->set_parent_window(
        web_contents->GetTopLevelNativeWindow());
  }

  views::BubbleDelegateView::CreateBubble(manage_passwords_bubble_);

  // Adjust for fullscreen after creation as it relies on the content size.
  if (is_fullscreen) {
    manage_passwords_bubble_->AdjustForFullscreen(
        browser_view->GetBoundsInScreen());
  }
  if (reason == AUTOMATIC)
    manage_passwords_bubble_->GetWidget()->ShowInactive();
  else
    manage_passwords_bubble_->GetWidget()->Show();
  manage_passwords_bubble_->StartTimerIfNecessary();
}

// static
void ManagePasswordsBubbleView::CloseBubble() {
  if (manage_passwords_bubble_)
    manage_passwords_bubble_->Close();
}

// static
void ManagePasswordsBubbleView::ActivateBubble() {
  if (!IsShowing())
    return;
  manage_passwords_bubble_->GetWidget()->Activate();
}

// static
bool ManagePasswordsBubbleView::IsShowing() {
  // The bubble may be in the process of closing.
  return (manage_passwords_bubble_ != NULL) &&
      manage_passwords_bubble_->GetWidget()->IsVisible();
}

ManagePasswordsBubbleView::ManagePasswordsBubbleView(
    content::WebContents* web_contents,
    ManagePasswordsIconView* anchor_view,
    DisplayReason reason)
    : ManagePasswordsBubble(web_contents, reason),
      BubbleDelegateView(anchor_view,
                         anchor_view ? views::BubbleBorder::TOP_RIGHT
                                     : views::BubbleBorder::NONE),
      anchor_view_(anchor_view),
      never_save_passwords_(false),
      initially_focused_view_(NULL) {
  // Compensate for built-in vertical padding in the anchor view's image.
  set_anchor_view_insets(gfx::Insets(5, 0, 5, 0));
  set_notify_enter_exit_on_child(true);
  if (anchor_view)
    anchor_view->SetActive(true);
}

ManagePasswordsBubbleView::~ManagePasswordsBubbleView() {
  if (anchor_view_)
    anchor_view_->SetActive(false);
}

void ManagePasswordsBubbleView::AdjustForFullscreen(
    const gfx::Rect& screen_bounds) {
  if (GetAnchorView())
    return;

  // The bubble's padding from the screen edge, used in fullscreen.
  const int kFullscreenPaddingEnd = 20;
  const size_t bubble_half_width = width() / 2;
  const int x_pos = base::i18n::IsRTL() ?
      screen_bounds.x() + bubble_half_width + kFullscreenPaddingEnd :
      screen_bounds.right() - bubble_half_width - kFullscreenPaddingEnd;
  SetAnchorRect(gfx::Rect(x_pos, screen_bounds.y(), 0, 0));
}

void ManagePasswordsBubbleView::Close() {
  GetWidget()->Close();
}

void ManagePasswordsBubbleView::Init() {
  views::FillLayout* layout = new views::FillLayout();
  SetLayoutManager(layout);

  Refresh();
}

void ManagePasswordsBubbleView::WindowClosing() {
  // Close() closes the window asynchronously, so by the time we reach here,
  // |manage_passwords_bubble_| may have already been reset.
  if (manage_passwords_bubble_ == this)
    manage_passwords_bubble_ = NULL;
}

void ManagePasswordsBubbleView::OnWidgetActivationChanged(views::Widget* widget,
                                                          bool active) {
  if (active && widget == GetWidget())
    timer_.Stop();
  BubbleDelegateView::OnWidgetActivationChanged(widget, active);
}

views::View* ManagePasswordsBubbleView::GetInitiallyFocusedView() {
  return initially_focused_view_;
}

void ManagePasswordsBubbleView::OnMouseEntered(const ui::MouseEvent& event) {
  timer_.Stop();
}

void ManagePasswordsBubbleView::OnMouseExited(const ui::MouseEvent& event) {
  StartTimerIfNecessary();
}

void ManagePasswordsBubbleView::Refresh() {
  RemoveAllChildViews(true);
  initially_focused_view_ = NULL;
  if (password_manager::ui::IsPendingState(model()->state())) {
    if (never_save_passwords_)
      AddChildView(new ConfirmNeverView(this));
    else
      AddChildView(new PendingView(this));
  } else if (model()->state() == password_manager::ui::BLACKLIST_STATE) {
    AddChildView(new BlacklistedView(this));
  } else if (model()->state() == password_manager::ui::CONFIRMATION_STATE) {
    AddChildView(new SaveConfirmationView(this));
  } else {
    AddChildView(new ManageView(this));
  }
  GetLayoutManager()->Layout(this);
  // If we refresh the existing bubble we may want to restart the timer.
  if (GetWidget())
    StartTimerIfNecessary();
}

void ManagePasswordsBubbleView::NotifyNeverForThisSiteClicked() {
  if (model()->best_matches().empty()) {
    // Skip confirmation if there are no existing passwords for this site.
    NotifyConfirmedNeverForThisSite();
  } else {
    never_save_passwords_ = true;
    Refresh();
  }
}

void ManagePasswordsBubbleView::NotifyConfirmedNeverForThisSite() {
  model()->OnNeverForThisSiteClicked();
  Close();
}

void ManagePasswordsBubbleView::NotifyUndoNeverForThisSite() {
  never_save_passwords_ = false;
  Refresh();
}

void ManagePasswordsBubbleView::StartTimerIfNecessary() {
  // Active bubble will stay visible until it loses focus.
  if (GetWidget()->IsActive())
    return;
  timer_.Start(FROM_HERE,
               base::TimeDelta::FromSeconds(kBubbleCloseDelay),
               this,
               &ManagePasswordsBubbleView::Close);
}
