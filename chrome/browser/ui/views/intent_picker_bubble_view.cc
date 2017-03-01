// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/intent_picker_bubble_view.h"

#include "base/bind.h"
#include "base/i18n/rtl.h"
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
#include "ui/views/animation/ink_drop_host_view.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/scrollbar/overlay_scroll_bar.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/window/dialog_client_view.h"

namespace {

// Using |kMaxAppResults| as a measure of how many apps we want to show.
constexpr size_t kMaxAppResults = arc::ArcNavigationThrottle::kMaxAppResults;
// Main components sizes
constexpr int kTitlePadding = 16;
constexpr int kDialogDelegateInsets = 16;
constexpr int kRowHeight = 40;
constexpr int kMaxWidth = 320;

// UI position wrt the Top Container
constexpr int kTopContainerMerge = 3;

constexpr char kInvalidPackageName[] = "";

bool IsKeyboardCodeArrow(ui::KeyboardCode key_code) {
  return key_code == ui::VKEY_UP || key_code == ui::VKEY_DOWN ||
         key_code == ui::VKEY_RIGHT || key_code == ui::VKEY_LEFT;
}

}  // namespace

// IntentPickerLabelButton

// A button that represents a candidate intent handler.
class IntentPickerLabelButton : public views::LabelButton {
 public:
  IntentPickerLabelButton(views::ButtonListener* listener,
                          gfx::Image* icon,
                          const std::string& package_name,
                          const std::string& activity_name)
      : LabelButton(listener,
                    base::UTF8ToUTF16(base::StringPiece(activity_name))),
        package_name_(package_name) {
    SetHorizontalAlignment(gfx::ALIGN_LEFT);
    SetMinSize(gfx::Size(kMaxWidth, kRowHeight));
    SetInkDropMode(InkDropMode::ON);
    if (!icon->IsEmpty())
      SetImage(views::ImageButton::STATE_NORMAL, *icon->ToImageSkia());
    SetBorder(views::CreateEmptyBorder(10, 16, 10, 0));
  }

  SkColor GetInkDropBaseColor() const override { return SK_ColorBLACK; }

  void MarkAsUnselected(const ui::Event* event) {
    AnimateInkDrop(views::InkDropState::HIDDEN,
                   ui::LocatedEvent::FromIfValid(event));
  }

  void MarkAsSelected(const ui::Event* event) {
    AnimateInkDrop(views::InkDropState::ACTIVATED,
                   ui::LocatedEvent::FromIfValid(event));
  }

  views::InkDropState GetTargetInkDropState() {
    return GetInkDrop()->GetTargetInkDropState();
  }

 private:
  std::string package_name_;

  DISALLOW_COPY_AND_ASSIGN(IntentPickerLabelButton);
};

// static
void IntentPickerBubbleView::ShowBubble(
    content::WebContents* web_contents,
    const std::vector<AppInfo>& app_info,
    const IntentPickerResponse& intent_picker_cb) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser || !BrowserView::GetBrowserViewForBrowser(browser)) {
    intent_picker_cb.Run(kInvalidPackageName,
                         arc::ArcNavigationThrottle::CloseReason::ERROR);
    return;
  }
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  IntentPickerBubbleView* delegate =
      new IntentPickerBubbleView(app_info, intent_picker_cb, web_contents);
  delegate->set_margins(gfx::Insets());
  delegate->set_parent_window(browser_view->GetNativeWindow());
  views::Widget* widget =
      views::BubbleDialogDelegateView::CreateBubble(delegate);
  delegate->GetDialogClientView()->set_button_row_insets(
      gfx::Insets(kDialogDelegateInsets));

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
  delegate->GetDialogClientView()->Layout();
  delegate->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  delegate->GetIntentPickerLabelButtonAt(0)->MarkAsSelected(nullptr);
  widget->Show();
}

// static
std::unique_ptr<IntentPickerBubbleView>
IntentPickerBubbleView::CreateBubbleView(
    const std::vector<AppInfo>& app_info,
    const IntentPickerResponse& intent_picker_cb,
    content::WebContents* web_contents) {
  std::unique_ptr<IntentPickerBubbleView> bubble(
      new IntentPickerBubbleView(app_info, intent_picker_cb, web_contents));
  bubble->Init();
  return bubble;
}

bool IntentPickerBubbleView::Accept() {
  RunCallback(app_info_[selected_app_tag_].package_name,
              arc::ArcNavigationThrottle::CloseReason::JUST_ONCE_PRESSED);
  return true;
}

bool IntentPickerBubbleView::Cancel() {
  RunCallback(app_info_[selected_app_tag_].package_name,
              arc::ArcNavigationThrottle::CloseReason::ALWAYS_PRESSED);
  return true;
}

bool IntentPickerBubbleView::Close() {
  // Whenever closing the bubble without pressing |Just once| or |Always| we
  // need to report back that the user didn't select anything.
  RunCallback(kInvalidPackageName,
              arc::ArcNavigationThrottle::CloseReason::DIALOG_DEACTIVATED);
  return true;
}

void IntentPickerBubbleView::Init() {
  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);

  // Creates a view to hold the views for each app.
  views::View* scrollable_view = new views::View();
  views::BoxLayout* scrollable_layout =
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0);
  scrollable_view->SetLayoutManager(scrollable_layout);
  for (size_t i = 0; i < app_info_.size(); ++i) {
    IntentPickerLabelButton* app_button = new IntentPickerLabelButton(
        this, &app_info_[i].icon, app_info_[i].package_name,
        app_info_[i].activity_name);
    app_button->set_tag(i);
    scrollable_view->AddChildViewAt(app_button, i);
  }

  scroll_view_ = new views::ScrollView();
  scroll_view_->EnableViewPortLayer();
  scroll_view_->SetContents(scrollable_view);
  // Setting a customized ScrollBar which is shown only when the mouse pointer
  // is inside the ScrollView.
  scroll_view_->SetVerticalScrollBar(new views::OverlayScrollBar(false));
  // This part gives the scroll a fixed width and height. The height depends on
  // how many app candidates we got and how many we actually want to show.
  // The added 0.5 on the else block allow us to let the user know there are
  // more than |kMaxAppResults| apps accessible by scrolling the list.
  if (app_info_.size() <= kMaxAppResults) {
    scroll_view_->ClipHeightTo(kRowHeight, app_info_.size() * kRowHeight);
  } else {
    scroll_view_->ClipHeightTo(kRowHeight, (kMaxAppResults + 0.5) * kRowHeight);
  }

  const int cs_id = 0;
  views::ColumnSet* cs = layout->AddColumnSet(cs_id);
  cs->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER, 0,
                views::GridLayout::FIXED, kMaxWidth, 0);

  layout->AddPaddingRow(0, kTitlePadding);
  layout->StartRow(0, cs_id);
  layout->AddView(scroll_view_);
}

base::string16 IntentPickerBubbleView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_INTENT_PICKER_BUBBLE_VIEW_OPEN_WITH);
}

base::string16 IntentPickerBubbleView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return l10n_util::GetStringUTF16(button == ui::DIALOG_BUTTON_OK
                                       ? IDS_INTENT_PICKER_BUBBLE_VIEW_JUST_ONCE
                                       : IDS_INTENT_PICKER_BUBBLE_VIEW_ALWAYS);
}

IntentPickerBubbleView::IntentPickerBubbleView(
    const std::vector<AppInfo>& app_info,
    IntentPickerResponse intent_picker_cb,
    content::WebContents* web_contents)
    : views::BubbleDialogDelegateView(nullptr /* anchor_view */,
                                      views::BubbleBorder::TOP_CENTER),
      WebContentsObserver(web_contents),
      intent_picker_cb_(intent_picker_cb),
      selected_app_tag_(0),
      scroll_view_(nullptr),
      app_info_(app_info) {}

IntentPickerBubbleView::~IntentPickerBubbleView() {
  SetLayoutManager(nullptr);
}

// If the widget gets closed without an app being selected we still need to use
// the callback so the caller can Resume the navigation.
void IntentPickerBubbleView::OnWidgetDestroying(views::Widget* widget) {
  RunCallback(kInvalidPackageName,
              arc::ArcNavigationThrottle::CloseReason::DIALOG_DEACTIVATED);
}

void IntentPickerBubbleView::ButtonPressed(views::Button* sender,
                                           const ui::Event& event) {
  SetSelectedAppIndex(sender->tag(), &event);
  RequestFocus();
}

void IntentPickerBubbleView::ArrowButtonPressed(int index) {
  SetSelectedAppIndex(index, nullptr);
  AdjustScrollViewVisibleRegion();
}

// If the actual web_contents gets destroyed in the middle of the process we
// should inform the caller about this error.
void IntentPickerBubbleView::WebContentsDestroyed() {
  GetWidget()->Close();
}

void IntentPickerBubbleView::OnKeyEvent(ui::KeyEvent* event) {
  if (!IsKeyboardCodeArrow(event->key_code()) ||
      event->type() != ui::ET_KEY_RELEASED)
    return;

  int delta = 0;
  switch (event->key_code()) {
    case ui::VKEY_UP:
      delta = -1;
      break;
    case ui::VKEY_DOWN:
      delta = 1;
      break;
    case ui::VKEY_LEFT:
      delta = base::i18n::IsRTL() ? 1 : -1;
      break;
    case ui::VKEY_RIGHT:
      delta = base::i18n::IsRTL() ? -1 : 1;
      break;
    default:
      NOTREACHED();
      break;
  }
  ArrowButtonPressed(CalculateNextAppIndex(delta));

  View::OnKeyEvent(event);
}

IntentPickerLabelButton* IntentPickerBubbleView::GetIntentPickerLabelButtonAt(
    size_t index) {
  views::View* temp_contents = scroll_view_->contents();
  return static_cast<IntentPickerLabelButton*>(temp_contents->child_at(index));
}

void IntentPickerBubbleView::RunCallback(
    std::string package,
    arc::ArcNavigationThrottle::CloseReason close_reason) {
  if (!intent_picker_cb_.is_null()) {
    // We must ensure |intent_picker_cb_| is only Run() once, this is why we
    // have a temporary |callback| helper, so we can set the original callback
    // to null and still report back to whoever started the UI.
    auto callback = intent_picker_cb_;
    intent_picker_cb_.Reset();
    callback.Run(package, close_reason);
  }
}

size_t IntentPickerBubbleView::GetScrollViewSize() const {
  return scroll_view_->contents()->child_count();
}

void IntentPickerBubbleView::AdjustScrollViewVisibleRegion() {
  const views::ScrollBar* bar = scroll_view_->vertical_scroll_bar();
  if (bar) {
    scroll_view_->ScrollToPosition(const_cast<views::ScrollBar*>(bar),
                                   (selected_app_tag_ - 1) * kRowHeight);
  }
}

void IntentPickerBubbleView::SetSelectedAppIndex(int index,
                                                 const ui::Event* event) {
  // The selected app must be a value in the range [0, app_info_.size()-1].
  DCHECK_LT(static_cast<size_t>(index), app_info_.size());

  GetIntentPickerLabelButtonAt(selected_app_tag_)->MarkAsUnselected(nullptr);
  selected_app_tag_ = index;
  GetIntentPickerLabelButtonAt(selected_app_tag_)->MarkAsSelected(event);
}

size_t IntentPickerBubbleView::CalculateNextAppIndex(int delta) {
  size_t size = GetScrollViewSize();
  return static_cast<size_t>((selected_app_tag_ + size + delta) % size);
}

gfx::ImageSkia IntentPickerBubbleView::GetAppImageForTesting(size_t index) {
  return GetIntentPickerLabelButtonAt(index)->GetImage(
      views::Button::ButtonState::STATE_NORMAL);
}

views::InkDropState IntentPickerBubbleView::GetInkDropStateForTesting(
    size_t index) {
  return GetIntentPickerLabelButtonAt(index)->GetTargetInkDropState();
}

void IntentPickerBubbleView::PressButtonForTesting(size_t index,
                                                   const ui::Event& event) {
  views::Button* button =
      static_cast<views::Button*>(GetIntentPickerLabelButtonAt(index));
  ButtonPressed(button, event);
}
