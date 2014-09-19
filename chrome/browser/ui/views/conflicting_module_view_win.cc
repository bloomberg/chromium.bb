// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/conflicting_module_view_win.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/enumerate_modules_model_win.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/locale_settings.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/user_metrics.h"
#include "grit/theme_resources.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

using base::UserMetricsAction;

namespace {

// Layout constants.
const int kInsetBottomRight = 13;
const int kInsetLeft = 14;
const int kInsetTop = 9;
const int kHeadlineMessagePadding = 4;
const int kMessageBubblePadding = 11;

// How often to show this bubble.
const int kShowConflictingModuleBubbleMax = 3;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ConflictingModuleView

ConflictingModuleView::ConflictingModuleView(
    views::View* anchor_view,
    Browser* browser,
    const GURL& help_center_url)
    : BubbleDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT),
      browser_(browser),
      explanation_(NULL),
      learn_more_button_(NULL),
      not_now_button_(NULL),
      help_center_url_(help_center_url) {
  set_close_on_deactivate(false);
  set_close_on_esc(true);

  // Compensate for built-in vertical padding in the anchor view's image.
  set_anchor_view_insets(gfx::Insets(5, 0, 5, 0));

  registrar_.Add(this, chrome::NOTIFICATION_MODULE_INCOMPATIBILITY_BADGE_CHANGE,
                 content::NotificationService::AllSources());
}

// static
void ConflictingModuleView::MaybeShow(Browser* browser,
                                      views::View* anchor_view) {
  static bool done_checking = false;
  if (done_checking)
    return;  // Only show the bubble once per launch.

  EnumerateModulesModel* model = EnumerateModulesModel::GetInstance();
  GURL url = model->GetFirstNotableConflict();
  if (!url.is_valid()) {
    done_checking = true;
    return;
  }

  // A pref that counts how often the Sideload Wipeout bubble has been shown.
  IntegerPrefMember bubble_shown;
  bubble_shown.Init(prefs::kModuleConflictBubbleShown,
                    browser->profile()->GetPrefs());
  if (bubble_shown.GetValue() >= kShowConflictingModuleBubbleMax) {
    done_checking = true;
    return;
  }

  // |anchor_view| must be in a widget (the browser's widget). If not, |browser|
  // may be destroyed before us, and we'll crash trying to access |browser|
  // later on. We can't DCHECK |browser|'s widget here as we may be called from
  // creation of BrowserWindow, which means browser->window() may return NULL.
  DCHECK(anchor_view);
  DCHECK(anchor_view->GetWidget());

  ConflictingModuleView* bubble_delegate =
      new ConflictingModuleView(anchor_view, browser, url);
  views::BubbleDelegateView::CreateBubble(bubble_delegate);
  bubble_delegate->ShowBubble();

  done_checking = true;
}

////////////////////////////////////////////////////////////////////////////////
// ConflictingModuleView - private.

ConflictingModuleView::~ConflictingModuleView() {
}

void ConflictingModuleView::ShowBubble() {
  GetWidget()->Show();

  IntegerPrefMember bubble_shown;
  bubble_shown.Init(
      prefs::kModuleConflictBubbleShown,
      browser_->profile()->GetPrefs());
  bubble_shown.SetValue(bubble_shown.GetValue() + 1);
}

void ConflictingModuleView::DismissBubble() {
  GetWidget()->Close();

  content::RecordAction(
      UserMetricsAction("ConflictingModuleNotificationDismissed"));
}

void ConflictingModuleView::Init() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  views::GridLayout* layout = views::GridLayout::CreatePanel(this);
  layout->SetInsets(kInsetTop, kInsetLeft,
                    kInsetBottomRight, kInsetBottomRight);
  SetLayoutManager(layout);

  views::ImageView* icon = new views::ImageView();
  icon->SetImage(rb.GetNativeImageNamed(IDR_INPUT_ALERT_MENU).ToImageSkia());
  gfx::Size icon_size = icon->GetPreferredSize();

  const int text_column_set_id = 0;
  views::ColumnSet* upper_columns = layout->AddColumnSet(text_column_set_id);
  upper_columns->AddColumn(
      views::GridLayout::LEADING, views::GridLayout::LEADING,
      0, views::GridLayout::FIXED, icon_size.width(), icon_size.height());
  upper_columns->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
  upper_columns->AddColumn(
      views::GridLayout::LEADING, views::GridLayout::LEADING,
      0, views::GridLayout::USE_PREF, 0, 0);

  layout->StartRowWithPadding(
      0, text_column_set_id, 0, kHeadlineMessagePadding);
  layout->AddView(icon);
  explanation_ = new views::Label();
  explanation_->SetMultiLine(true);
  explanation_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  explanation_->SetText(l10n_util::GetStringUTF16(
      IDS_OPTIONS_CONFLICTING_MODULE));
  explanation_->SizeToFit(views::Widget::GetLocalizedContentsWidth(
      IDS_CONFLICTING_MODULE_BUBBLE_WIDTH_CHARS));
  layout->AddView(explanation_);

  const int action_row_column_set_id = 1;
  views::ColumnSet* bottom_columns =
      layout->AddColumnSet(action_row_column_set_id);
  bottom_columns->AddPaddingColumn(1, 0);
  bottom_columns->AddColumn(views::GridLayout::TRAILING,
      views::GridLayout::CENTER, 0, views::GridLayout::USE_PREF, 0, 0);
  bottom_columns->AddPaddingColumn(0, views::kRelatedButtonHSpacing);
  bottom_columns->AddColumn(views::GridLayout::TRAILING,
      views::GridLayout::CENTER, 0, views::GridLayout::USE_PREF, 0, 0);
  layout->AddPaddingRow(0, 7);

  layout->StartRowWithPadding(0, action_row_column_set_id,
                              0, kMessageBubblePadding);
  learn_more_button_ = new views::LabelButton(this,
      l10n_util::GetStringUTF16(IDS_CONFLICTS_LEARN_MORE));
  learn_more_button_->SetStyle(views::Button::STYLE_BUTTON);
  layout->AddView(learn_more_button_);
  not_now_button_ = new views::LabelButton(this,
      l10n_util::GetStringUTF16(IDS_CONFLICTS_NOT_NOW));
  not_now_button_->SetStyle(views::Button::STYLE_BUTTON);
  layout->AddView(not_now_button_);

  content::RecordAction(
      UserMetricsAction("ConflictingModuleNotificationShown"));

  UMA_HISTOGRAM_ENUMERATION("ConflictingModule.UserSelection",
      EnumerateModulesModel::ACTION_BUBBLE_SHOWN,
      EnumerateModulesModel::ACTION_BOUNDARY);
}

void ConflictingModuleView::ButtonPressed(views::Button* sender,
                                          const ui::Event& event) {
  if (sender == learn_more_button_) {
    browser_->OpenURL(
        content::OpenURLParams(help_center_url_,
                               content::Referrer(),
                               NEW_FOREGROUND_TAB,
                               ui::PAGE_TRANSITION_LINK,
                               false));

    EnumerateModulesModel* model = EnumerateModulesModel::GetInstance();
    model->AcknowledgeConflictNotification();
    DismissBubble();
  } else if (sender == not_now_button_) {
    DismissBubble();
  }
}

void ConflictingModuleView::GetAccessibleState(
    ui::AXViewState* state) {
  state->role = ui::AX_ROLE_ALERT;
}

void ConflictingModuleView::ViewHierarchyChanged(
  const ViewHierarchyChangedDetails& details) {
  if (details.is_add && details.child == this)
    NotifyAccessibilityEvent(ui::AX_EVENT_ALERT, true);
}

void ConflictingModuleView::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_MODULE_INCOMPATIBILITY_BADGE_CHANGE);
  EnumerateModulesModel* model = EnumerateModulesModel::GetInstance();
  if (!model->ShouldShowConflictWarning())
    GetWidget()->Close();
}
