// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/manage_passwords_bubble_view.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/passwords/manage_password_item_view.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_icon_view.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents_view.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/button/blue_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"


// Helpers --------------------------------------------------------------------

namespace {

// Updates either the biggest possible width for the username field in the
// manage passwords bubble or the biggest possible width for the password field.
void UpdateBiggestWidth(const autofill::PasswordForm& password_form,
                        bool username,
                        int* biggest_width) {
  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  gfx::FontList font_list(rb->GetFontList(ui::ResourceBundle::BaseFont));
  base::string16 display_string(username ?
      password_form.username_value :
      ManagePasswordItemView::GetPasswordDisplayString(
          password_form.password_value));
  *biggest_width = std::max(
      gfx::Canvas::GetStringWidth(display_string, font_list), *biggest_width);
}

}  // namespace


// ManagePasswordsBubbleView --------------------------------------------------

// static
ManagePasswordsBubbleView* ManagePasswordsBubbleView::manage_passwords_bubble_ =
    NULL;

// static
void ManagePasswordsBubbleView::ShowBubble(content::WebContents* web_contents,
                                           ManagePasswordsIconView* icon_view) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  DCHECK(browser);
  DCHECK(browser->window());
  DCHECK(browser->fullscreen_controller());
  DCHECK(!IsShowing());

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  bool is_fullscreen = browser_view->IsFullscreen();
  views::View* anchor_view = is_fullscreen ?
      NULL : browser_view->GetLocationBarView()->manage_passwords_icon_view();
  manage_passwords_bubble_ =
      new ManagePasswordsBubbleView(web_contents, anchor_view, icon_view);

  if (is_fullscreen) {
    manage_passwords_bubble_->set_parent_window(
        web_contents->GetView()->GetTopLevelNativeWindow());
  }

  views::BubbleDelegateView::CreateBubble(manage_passwords_bubble_);

  // Adjust for fullscreen after creation as it relies on the content size.
  if (is_fullscreen) {
    manage_passwords_bubble_->AdjustForFullscreen(
        browser_view->GetBoundsInScreen());
  }

  manage_passwords_bubble_->GetWidget()->Show();
}

// static
void ManagePasswordsBubbleView::CloseBubble() {
  if (manage_passwords_bubble_)
   manage_passwords_bubble_->Close();
}

// static
bool ManagePasswordsBubbleView::IsShowing() {
  // The bubble may be in the process of closing.
  return (manage_passwords_bubble_ != NULL) &&
      manage_passwords_bubble_->GetWidget()->IsVisible();
}

ManagePasswordsBubbleView::ManagePasswordsBubbleView(
    content::WebContents* web_contents,
    views::View* anchor_view,
    ManagePasswordsIconView* icon_view)
    : BubbleDelegateView(
          anchor_view,
          anchor_view ?
              views::BubbleBorder::TOP_RIGHT : views::BubbleBorder::NONE),
      manage_passwords_bubble_model_(
          new ManagePasswordsBubbleModel(web_contents)),
      icon_view_(icon_view) {
  // Compensate for built-in vertical padding in the anchor view's image.
  set_anchor_view_insets(gfx::Insets(5, 0, 5, 0));
  set_notify_enter_exit_on_child(true);
}

ManagePasswordsBubbleView::~ManagePasswordsBubbleView() {}

int ManagePasswordsBubbleView::GetMaximumUsernameOrPasswordWidth(
    bool username) {
  int biggest_width = 0;
  if (manage_passwords_bubble_model_->manage_passwords_bubble_state() !=
      ManagePasswordsBubbleModel::PASSWORD_TO_BE_SAVED) {
    // If we are in the PASSWORD_TO_BE_SAVED state we only display the
    // password that was just submitted and should not take these into account.
    for (autofill::PasswordFormMap::const_iterator i(
             manage_passwords_bubble_model_->best_matches().begin());
         i != manage_passwords_bubble_model_->best_matches().end(); ++i) {
      UpdateBiggestWidth((*i->second), username, &biggest_width);
    }
  }
  if (manage_passwords_bubble_model_->password_submitted()) {
    UpdateBiggestWidth(manage_passwords_bubble_model_->pending_credentials(),
                       username, &biggest_width);
  }
  return biggest_width;
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
  using views::GridLayout;

  GridLayout* layout = new GridLayout(this);
  SetLayoutManager(layout);

  // This calculates the necessary widths for the list of credentials in the
  // bubble. We do not need to clamp the password field width because
  // ManagePasswordItemView::GetPasswordFisplayString() does this.

  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  const int predefined_username_field_max_width =
      rb->GetFont(ui::ResourceBundle::BaseFont).GetAverageCharacterWidth() * 22;
  const int max_username_or_password_width =
      std::min(GetMaximumUsernameOrPasswordWidth(true),
               predefined_username_field_max_width);
  const int first_field_width = std::max(max_username_or_password_width,
      views::Label(l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_DELETED)).
          GetPreferredSize().width());

  const int second_field_width = std::max(
      GetMaximumUsernameOrPasswordWidth(false),
      views::Label(l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_UNDO)).
          GetPreferredSize().width());

  const int kSingleColumnSetId = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(kSingleColumnSetId);
  column_set->AddPaddingColumn(0, views::kPanelHorizMargin);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, views::kPanelHorizMargin);

  views::Label* title_label =
      new views::Label(manage_passwords_bubble_model_->title());
  title_label->SetMultiLine(true);
  title_label->SetFontList(rb->GetFontList(ui::ResourceBundle::MediumFont));

  layout->StartRowWithPadding(0, kSingleColumnSetId,
                              0, views::kRelatedControlSmallVerticalSpacing);
  layout->AddView(title_label);
  layout->AddPaddingRow(0, views::kUnrelatedControlVerticalSpacing);

  if (manage_passwords_bubble_model_->manage_passwords_bubble_state() ==
      ManagePasswordsBubbleModel::PASSWORD_TO_BE_SAVED) {
    const int kSingleColumnCredentialsId = 1;
    views::ColumnSet* single_column =
        layout->AddColumnSet(kSingleColumnCredentialsId);
    single_column->AddPaddingColumn(0, views::kPanelHorizMargin);
    single_column->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                             GridLayout::USE_PREF, 0, 0);
    single_column->AddPaddingColumn(0, views::kPanelHorizMargin);

    layout->StartRow(0, kSingleColumnCredentialsId);
    ManagePasswordItemView* item = new ManagePasswordItemView(
        manage_passwords_bubble_model_,
        manage_passwords_bubble_model_->pending_credentials(),
        first_field_width, second_field_width);
    item->set_border(views::Border::CreateSolidSidedBorder(
        1, 0, 1, 0, GetNativeTheme()->GetSystemColor(
            ui::NativeTheme::kColorId_EnabledMenuButtonBorderColor)));
    layout->AddView(item);

    const int kDoubleColumnSetId = 2;
    views::ColumnSet* double_column_set =
        layout->AddColumnSet(kDoubleColumnSetId);
    double_column_set->AddPaddingColumn(0, views::kPanelHorizMargin);
    double_column_set->AddColumn(GridLayout::TRAILING, GridLayout::CENTER, 1,
                                 GridLayout::USE_PREF, 0, 0);
    double_column_set->AddPaddingColumn(0, views::kRelatedButtonHSpacing);
    double_column_set->AddColumn(GridLayout::TRAILING, GridLayout::CENTER, 0,
                                 GridLayout::USE_PREF, 0, 0);
    double_column_set->AddPaddingColumn(0, views::kPanelHorizMargin);

    cancel_button_ = new views::LabelButton(
        this, l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_CANCEL_BUTTON));
    cancel_button_->SetStyle(views::Button::STYLE_NATIVE_TEXTBUTTON);
    save_button_ = new views::BlueButton(
        this, l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_SAVE_BUTTON));

    layout->StartRowWithPadding(0, kDoubleColumnSetId,
                                0, views::kRelatedControlVerticalSpacing);
    layout->AddView(save_button_);
    layout->AddView(cancel_button_);
    layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
  } else {
    const int kSingleButtonSetId = 3;
    views::ColumnSet* single_column_set =
        layout->AddColumnSet(kSingleButtonSetId);
    single_column_set->AddPaddingColumn(0, views::kPanelHorizMargin);
    single_column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 1,
                                 GridLayout::USE_PREF, 0, 0);
    single_column_set->AddPaddingColumn(0,
        views::kUnrelatedControlHorizontalSpacing);
    single_column_set->AddColumn(GridLayout::TRAILING, GridLayout::CENTER, 0,
                                 GridLayout::USE_PREF, 0, 0);
    single_column_set->AddPaddingColumn(0, views::kPanelHorizMargin);

    const int kSingleColumnCredentialsId = 1;
    views::ColumnSet* single_column =
        layout->AddColumnSet(kSingleColumnCredentialsId);
    single_column->AddPaddingColumn(0, views::kPanelHorizMargin);
    single_column->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                             GridLayout::USE_PREF, 0, 0);
    single_column->AddPaddingColumn(0, views::kPanelHorizMargin);

    if (!manage_passwords_bubble_model_->best_matches().empty()) {
      for (autofill::PasswordFormMap::const_iterator i(
               manage_passwords_bubble_model_->best_matches().begin());
           i != manage_passwords_bubble_model_->best_matches().end(); ++i) {
        layout->StartRow(0, kSingleColumnCredentialsId);
        ManagePasswordItemView* item = new ManagePasswordItemView(
            manage_passwords_bubble_model_, *i->second, first_field_width,
            second_field_width);
        if (i == manage_passwords_bubble_model_->best_matches().begin()) {
          item->set_border(views::Border::CreateSolidSidedBorder(
              1, 0, 1, 0, GetNativeTheme()->GetSystemColor(
                  ui::NativeTheme::kColorId_EnabledMenuButtonBorderColor)));
        } else {
          item->set_border(views::Border::CreateSolidSidedBorder(
              0, 0, 1, 0, GetNativeTheme()->GetSystemColor(
                  ui::NativeTheme::kColorId_EnabledMenuButtonBorderColor)));
        }
        layout->AddView(item);
      }
    } else if (!manage_passwords_bubble_model_->password_submitted()) {
        views::Label* empty_label = new views::Label(
            l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_NO_PASSWORDS));
        empty_label->SetMultiLine(true);
        layout->StartRow(0, kSingleColumnSetId);
        layout->AddView(empty_label);
    }

    if (manage_passwords_bubble_model_->password_submitted()) {
      layout->StartRow(0, kSingleColumnCredentialsId);
      ManagePasswordItemView* item = new ManagePasswordItemView(
          manage_passwords_bubble_model_,
          manage_passwords_bubble_model_->pending_credentials(),
          first_field_width, second_field_width);
      if (manage_passwords_bubble_model_->best_matches().empty()) {
        item->set_border(views::Border::CreateSolidSidedBorder(1, 0, 1, 0,
            GetNativeTheme()->GetSystemColor(
                ui::NativeTheme::kColorId_EnabledMenuButtonBorderColor)));
      } else {
        item->set_border(views::Border::CreateSolidSidedBorder(0, 0, 1, 0,
            GetNativeTheme()->GetSystemColor(
                ui::NativeTheme::kColorId_EnabledMenuButtonBorderColor)));
      }
      layout->AddView(item);
    }

    manage_link_ =
        new views::Link(manage_passwords_bubble_model_->manage_link());
    manage_link_->set_listener(this);
    layout->StartRowWithPadding(0, kSingleButtonSetId,
                                0, views::kRelatedControlVerticalSpacing);
    layout->AddView(manage_link_);

    done_button_ =
        new views::LabelButton(this, l10n_util::GetStringUTF16(IDS_DONE));
    done_button_->SetStyle(views::Button::STYLE_NATIVE_TEXTBUTTON);
    layout->AddView(done_button_);
  }
}

void ManagePasswordsBubbleView::WindowClosing() {
  // Close() closes the window asynchronously, so by the time we reach here,
  // |manage_passwords_bubble_| may have already been reset.
  if (manage_passwords_bubble_ == this)
    manage_passwords_bubble_ = NULL;
}

void ManagePasswordsBubbleView::ButtonPressed(views::Button* sender,
                                              const ui::Event& event) {
  if (sender == save_button_)
    manage_passwords_bubble_model_->OnSaveClicked();
  else if (sender == cancel_button_)
    manage_passwords_bubble_model_->OnCancelClicked();
  else
    DCHECK_EQ(done_button_, sender);
  icon_view_->SetTooltip(
      manage_passwords_bubble_model_->manage_passwords_bubble_state() ==
          ManagePasswordsBubbleModel::PASSWORD_TO_BE_SAVED);
  Close();
}

void ManagePasswordsBubbleView::LinkClicked(views::Link* source,
                                            int event_flags) {
  DCHECK_EQ(source, manage_link_);
  manage_passwords_bubble_model_->OnManageLinkClicked();
}
