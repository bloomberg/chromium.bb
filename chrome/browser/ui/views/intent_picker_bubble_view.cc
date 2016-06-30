// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/intent_picker_bubble_view.h"

#include <algorithm>

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/navigation_handle.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/window/dialog_client_view.h"

namespace {

// TODO(djacobo): Add a scroll bar and remove kMaxAppResults.
// Discriminating app results when the list is longer than |kMaxAppResults|
constexpr size_t kMaxAppResults = 3;
// Main components sizes
constexpr int kRowHeight = 40;
constexpr int kMaxWidth = 320;
constexpr int kHeaderHeight = 60;
constexpr int kFooterHeight = 68;
// Inter components padding
constexpr int kButtonSeparation = 8;
constexpr int kLabelImageSeparation = 12;

// UI position wrt the Top Container
constexpr int kTopContainerMerge = 3;
constexpr int kAppTagNoneSelected = -1;

// Arbitrary negative values to use as tags on the |always_button_| and
// |just_once_button_|. These are negative to differentiate from the app's tags
// which are always >= 0.
enum class Option : int { ALWAYS = -2, JUST_ONCE };

}  // namespace

// static
void IntentPickerBubbleView::ShowBubble(
    content::NavigationHandle* handle,
    const std::vector<NameAndIcon>& app_info,
    const ThrottleCallback& throttle_cb) {
  Browser* browser =
      chrome::FindBrowserWithWebContents(handle->GetWebContents());
  if (!browser) {
    throttle_cb.Run(kAppTagNoneSelected,
                    arc::ArcNavigationThrottle::CloseReason::ERROR);
    return;
  }
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view) {
    throttle_cb.Run(kAppTagNoneSelected,
                    arc::ArcNavigationThrottle::CloseReason::ERROR);
    return;
  }

  IntentPickerBubbleView* delegate = new IntentPickerBubbleView(
      app_info, throttle_cb, handle->GetWebContents());
  delegate->set_margins(gfx::Insets());
  delegate->set_parent_window(browser_view->GetNativeWindow());
  views::Widget* widget =
      views::BubbleDialogDelegateView::CreateBubble(delegate);

  delegate->SetArrowPaintType(views::BubbleBorder::PAINT_NONE);
  delegate->SetAlignment(views::BubbleBorder::ALIGN_EDGE_TO_ANCHOR_EDGE);

  // Using the TopContainerBoundsInScreen Rect to specify an anchor for the the
  // UI. Rect allow us to set the coordinates(x,y), the width and height for the
  // new Rectangle.
  delegate->SetAnchorRect(
      gfx::Rect(browser_view->GetTopContainerBoundsInScreen().x(),
                browser_view->GetTopContainerBoundsInScreen().y(),
                browser_view->GetTopContainerBoundsInScreen().width(),
                browser_view->GetTopContainerBoundsInScreen().height() -
                    kTopContainerMerge));
  widget->Show();
}

void IntentPickerBubbleView::Init() {
  SkColor button_text_color = SkColorSetRGB(0x42, 0x85, 0xf4);

  views::BoxLayout* general_layout =
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0);
  SetLayoutManager(general_layout);

  // Create a view for the upper part of the UI, managed by a GridLayout to
  // allow precise padding.
  View* header = new View();
  views::GridLayout* header_layout = new views::GridLayout(header);
  header->SetLayoutManager(header_layout);
  views::Label* open_with = new views::Label(
      l10n_util::GetStringUTF16(IDS_INTENT_PICKER_BUBBLE_VIEW_OPEN_WITH));
  open_with->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  open_with->SetFontList(gfx::FontList("Roboto-medium, 15px"));

  views::ColumnSet* column_set = header_layout->AddColumnSet(0);
  column_set->AddPaddingColumn(0, 16);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                        views::GridLayout::FIXED, kMaxWidth, kMaxWidth);
  column_set->AddPaddingColumn(0, 16);
  // Tell the GridLayout where to start, then proceed to place the views.
  header_layout->AddPaddingRow(0.0, 21);
  header_layout->StartRow(0, 0);
  header_layout->AddView(open_with);
  header_layout->AddPaddingRow(0.0, 24);

  always_button_ = new views::LabelButton(
      this, l10n_util::GetStringUTF16(IDS_INTENT_PICKER_BUBBLE_VIEW_ALWAYS));
  always_button_->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  always_button_->SetFontList(gfx::FontList("Roboto-medium, 13px"));
  always_button_->set_tag(static_cast<int>(Option::ALWAYS));
  always_button_->SetMinSize(gfx::Size(80, 32));
  always_button_->SetTextColor(views::Button::STATE_DISABLED, SK_ColorGRAY);
  always_button_->SetTextColor(views::Button::STATE_NORMAL, button_text_color);
  always_button_->SetTextColor(views::Button::STATE_HOVERED, button_text_color);
  always_button_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  always_button_->SetState(views::Button::STATE_DISABLED);

  just_once_button_ = new views::LabelButton(
      this, l10n_util::GetStringUTF16(IDS_INTENT_PICKER_BUBBLE_VIEW_JUST_ONCE));
  just_once_button_->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  just_once_button_->SetFontList(gfx::FontList("Roboto-medium, 13px"));
  just_once_button_->set_tag(static_cast<int>(Option::JUST_ONCE));
  just_once_button_->SetMinSize(gfx::Size(80, 32));
  just_once_button_->SetState(views::Button::STATE_DISABLED);
  just_once_button_->SetTextColor(views::Button::STATE_DISABLED, SK_ColorGRAY);
  just_once_button_->SetTextColor(views::Button::STATE_NORMAL,
                                  button_text_color);
  just_once_button_->SetTextColor(views::Button::STATE_HOVERED,
                                  button_text_color);
  just_once_button_->SetHorizontalAlignment(gfx::ALIGN_CENTER);

  for (size_t i = 0; i < std::min(app_info_.size(), kMaxAppResults); ++i) {
    views::LabelButton* tmp_label = new views::LabelButton(
        this, base::UTF8ToUTF16(base::StringPiece(app_info_[i].first)));
    tmp_label->SetFocusBehavior(View::FocusBehavior::ALWAYS);
    tmp_label->SetFontList(gfx::FontList("Roboto-regular, 13px"));
    tmp_label->SetImageLabelSpacing(kLabelImageSeparation);
    tmp_label->SetMinSize(gfx::Size(kMaxWidth, kRowHeight));
    tmp_label->SetMaxSize(gfx::Size(kMaxWidth, kRowHeight));
    tmp_label->set_tag(i);
    const gfx::ImageSkia* icon_ = app_info_[i].second.ToImageSkia();
    tmp_label->SetImage(views::ImageButton::STATE_NORMAL, *icon_);
    header_layout->StartRow(0, 0);
    header_layout->AddView(tmp_label);
  }

  // Adding the Upper part of the Intent Picker |open_with| label and all the
  // app options to |this|.
  header_layout->StartRow(0, 0);
  header_layout->AddPaddingRow(0, 12);
  AddChildView(header);

  // The lower part of the Picker contains only the 2 buttons
  // |just_once_button_| and |always_button_|.
  View* footer = new View();
  views::BoxLayout* buttons_layout = new views::BoxLayout(
      views::BoxLayout::kHorizontal, 12, 12, kButtonSeparation);
  footer->SetLayoutManager(buttons_layout);

  buttons_layout->set_main_axis_alignment(
      views::BoxLayout::MAIN_AXIS_ALIGNMENT_END);
  footer->AddChildView(just_once_button_);
  footer->AddChildView(always_button_);
  AddChildView(footer);
}

IntentPickerBubbleView::IntentPickerBubbleView(
    const std::vector<NameAndIcon>& app_info,
    ThrottleCallback throttle_cb,
    content::WebContents* web_contents)
    : views::BubbleDialogDelegateView(nullptr /* anchor_view */,
                                      views::BubbleBorder::TOP_CENTER),
      WebContentsObserver(web_contents),
      was_callback_run_(false),
      throttle_cb_(throttle_cb),
      selected_app_tag_(kAppTagNoneSelected),
      always_button_(nullptr),
      just_once_button_(nullptr),
      app_info_(app_info) {}

IntentPickerBubbleView::~IntentPickerBubbleView() {
  SetLayoutManager(nullptr);
}

// If the widget gets closed without an app being selected we still need to use
// the callback so the caller can Resume the navigation.
void IntentPickerBubbleView::OnWidgetDestroying(views::Widget* widget) {
  if (!was_callback_run_) {
    throttle_cb_.Run(
        kAppTagNoneSelected,
        arc::ArcNavigationThrottle::CloseReason::DIALOG_DEACTIVATED);
    was_callback_run_ = true;
  }
}

int IntentPickerBubbleView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

void IntentPickerBubbleView::ButtonPressed(views::Button* sender,
                                           const ui::Event& event) {
  switch (sender->tag()) {
    case static_cast<int>(Option::ALWAYS):
      throttle_cb_.Run(selected_app_tag_,
                       arc::ArcNavigationThrottle::CloseReason::ALWAYS_PRESSED);
      was_callback_run_ = true;
      GetWidget()->Close();
      break;
    case static_cast<int>(Option::JUST_ONCE):
      throttle_cb_.Run(
          selected_app_tag_,
          arc::ArcNavigationThrottle::CloseReason::JUST_ONCE_PRESSED);
      was_callback_run_ = true;
      GetWidget()->Close();
      break;
    default:
      // TODO(djacobo): Paint the background of the selected button on a
      // different color, so the user has a clear remainder of his selection.
      // The user cannot click on the |always_button_| or |just_once_button_|
      // unless an explicit app has been selected.
      selected_app_tag_ = sender->tag();
      always_button_->SetState(views::Button::STATE_NORMAL);
      just_once_button_->SetState(views::Button::STATE_NORMAL);
  }
}

gfx::Size IntentPickerBubbleView::GetPreferredSize() const {
  gfx::Size ps;
  ps.set_width(kMaxWidth);
  ps.set_height((std::min(app_info_.size(), kMaxAppResults)) * kRowHeight +
                kHeaderHeight + kFooterHeight);
  return ps;
}

// If the actual web_contents gets destroyed in the middle of the process we
// should inform the caller about this error.
void IntentPickerBubbleView::WebContentsDestroyed() {
  if (!was_callback_run_) {
    throttle_cb_.Run(kAppTagNoneSelected,
                     arc::ArcNavigationThrottle::CloseReason::ERROR);
    was_callback_run_ = true;
  }
  GetWidget()->Close();
}
