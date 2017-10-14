// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/intent_picker_bubble_view.h"

#include "base/bind.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "content/public/browser/navigation_handle.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/animation/ink_drop_host_view.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/window/dialog_client_view.h"

namespace {

// TODO(djacobo): Replace this limit to correctly reflect the UI mocks, which
// now instead of limiting the results to 3.5 will allow whatever fits in 256pt.
// Using |kMaxAppResults| as a measure of how many apps we want to show.
constexpr size_t kMaxAppResults = arc::ArcNavigationThrottle::kMaxAppResults;
// Main components sizes
constexpr int kTitlePadding = 16;
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
IntentPickerBubbleView* IntentPickerBubbleView::intent_picker_bubble_ = nullptr;

// static
views::Widget* IntentPickerBubbleView::ShowBubble(
    views::View* anchor_view,
    content::WebContents* web_contents,
    const std::vector<AppInfo>& app_info,
    const IntentPickerResponse& intent_picker_cb) {
  if (intent_picker_bubble_) {
    views::Widget* widget =
        views::BubbleDialogDelegateView::CreateBubble(intent_picker_bubble_);
    widget->Show();
    return widget;
  }
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser || !BrowserView::GetBrowserViewForBrowser(browser)) {
    intent_picker_cb.Run(kInvalidPackageName,
                         arc::ArcNavigationThrottle::CloseReason::ERROR);
    return nullptr;
  }
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  intent_picker_bubble_ =
      new IntentPickerBubbleView(app_info, intent_picker_cb, web_contents);
  intent_picker_bubble_->set_margins(gfx::Insets());

  if (anchor_view) {
    intent_picker_bubble_->SetAnchorView(anchor_view);
    intent_picker_bubble_->set_arrow(views::BubbleBorder::TOP_RIGHT);
  } else {
    intent_picker_bubble_->set_parent_window(browser_view->GetNativeWindow());
    // Using the TopContainerBoundsInScreen Rect to specify an anchor for the
    // the UI. Rect allow us to set the coordinates(x,y), the width and height
    // for the new Rectangle.
    intent_picker_bubble_->SetAnchorRect(
        gfx::Rect(browser_view->GetTopContainerBoundsInScreen().x(),
                  browser_view->GetTopContainerBoundsInScreen().y(),
                  browser_view->GetTopContainerBoundsInScreen().width(),
                  browser_view->GetTopContainerBoundsInScreen().height() -
                      kTopContainerMerge));
  }
  views::Widget* widget =
      views::BubbleDialogDelegateView::CreateBubble(intent_picker_bubble_);
  intent_picker_bubble_->GetDialogClientView()->Layout();
  intent_picker_bubble_->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  intent_picker_bubble_->GetIntentPickerLabelButtonAt(0)->MarkAsSelected(
      nullptr);
  widget->Show();
  return widget;
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

// static
void IntentPickerBubbleView::CloseCurrentBubble() {
  if (intent_picker_bubble_)
    intent_picker_bubble_->CloseBubble();
}

void IntentPickerBubbleView::CloseBubble() {
  intent_picker_bubble_ = nullptr;
  LocationBarBubbleDelegateView::CloseBubble();
}

bool IntentPickerBubbleView::Accept() {
  RunCallback(
      app_info_[selected_app_tag_].package_name,
      remember_selection_checkbox_->checked()
          ? arc::ArcNavigationThrottle::CloseReason::ARC_APP_PREFERRED_PRESSED
          : arc::ArcNavigationThrottle::CloseReason::ARC_APP_PRESSED);
  return true;
}

bool IntentPickerBubbleView::Cancel() {
  RunCallback(
      arc::ArcIntentHelperBridge::kArcIntentHelperPackageName,
      remember_selection_checkbox_->checked()
          ? arc::ArcNavigationThrottle::CloseReason::CHROME_PREFERRED_PRESSED
          : arc::ArcNavigationThrottle::CloseReason::CHROME_PRESSED);
  return true;
}

bool IntentPickerBubbleView::Close() {
  // Whenever closing the bubble without pressing |Just once| or |Always| we
  // need to report back that the user didn't select anything.
  RunCallback(kInvalidPackageName,
              arc::ArcNavigationThrottle::CloseReason::DIALOG_DEACTIVATED);
  return true;
}

bool IntentPickerBubbleView::ShouldShowCloseButton() const {
  return true;
}

void IntentPickerBubbleView::Init() {
  views::GridLayout* layout = views::GridLayout::CreateAndInstall(this);

  // Creates a view to hold the views for each app.
  views::View* scrollable_view = new views::View();
  views::BoxLayout* scrollable_layout =
      new views::BoxLayout(views::BoxLayout::kVertical);
  scrollable_view->SetLayoutManager(scrollable_layout);

  size_t i = 0;
  size_t to_erase = app_info_.size();
  for (AppInfo app_info : app_info_) {
    if (arc::ArcIntentHelperBridge::IsIntentHelperPackage(
            app_info.package_name)) {
      to_erase = i;
      continue;
    }
    IntentPickerLabelButton* app_button = new IntentPickerLabelButton(
        this, &app_info.icon, app_info.package_name, app_info.activity_name);
    app_button->set_tag(i);
    scrollable_view->AddChildViewAt(app_button, i++);
  }
  // We should delete at most one entry, this is the case when Chrome is listed
  // as a candidate to handle a given URL.
  if (to_erase != app_info_.size())
    app_info_.erase(app_info_.begin() + to_erase);

  scroll_view_ = new views::ScrollView();
  scroll_view_->SetBackgroundColor(SK_ColorWHITE);
  scroll_view_->SetContents(scrollable_view);
  // This part gives the scroll a fixed width and height. The height depends on
  // how many app candidates we got and how many we actually want to show.
  // The added 0.5 on the else block allow us to let the user know there are
  // more than |kMaxAppResults| apps accessible by scrolling the list.
  size_t rows = GetScrollViewSize();
  if (rows <= kMaxAppResults) {
    scroll_view_->ClipHeightTo(kRowHeight, rows * kRowHeight);
  } else {
    scroll_view_->ClipHeightTo(kRowHeight, (kMaxAppResults + 0.5) * kRowHeight);
  }

  constexpr int kColumnSetId = 0;
  views::ColumnSet* cs = layout->AddColumnSet(kColumnSetId);
  cs->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER, 0,
                views::GridLayout::FIXED, kMaxWidth, 0);

  layout->StartRowWithPadding(0, kColumnSetId, 0, kTitlePadding);
  layout->AddView(scroll_view_);

  // This second ColumnSet has a padding column in order to manipulate the
  // Checkbox positioning freely.
  constexpr int kColumnSetIdPadded = 1;
  views::ColumnSet* cs_padded = layout->AddColumnSet(kColumnSetIdPadded);
  cs_padded->AddPaddingColumn(0, kTitlePadding);
  cs_padded->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER, 0,
                       views::GridLayout::FIXED, kMaxWidth - 2 * kTitlePadding,
                       0);

  layout->StartRowWithPadding(0, kColumnSetIdPadded, 0, kTitlePadding / 2);
  remember_selection_checkbox_ = new views::Checkbox(l10n_util::GetStringUTF16(
      IDS_INTENT_PICKER_BUBBLE_VIEW_REMEMBER_SELECTION));
  layout->AddView(remember_selection_checkbox_);

  layout->AddPaddingRow(0, kTitlePadding / 2);
}

base::string16 IntentPickerBubbleView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_INTENT_PICKER_BUBBLE_VIEW_OPEN_WITH);
}

base::string16 IntentPickerBubbleView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return l10n_util::GetStringUTF16(
      button == ui::DIALOG_BUTTON_OK
          ? IDS_INTENT_PICKER_BUBBLE_VIEW_USE_APP
          : IDS_INTENT_PICKER_BUBBLE_VIEW_STAY_IN_CHROME);
}

IntentPickerBubbleView::IntentPickerBubbleView(
    const std::vector<AppInfo>& app_info,
    IntentPickerResponse intent_picker_cb,
    content::WebContents* web_contents)
    : LocationBarBubbleDelegateView(nullptr /* anchor_view */, web_contents),
      intent_picker_cb_(intent_picker_cb),
      selected_app_tag_(0),
      scroll_view_(nullptr),
      app_info_(app_info),
      remember_selection_checkbox_(nullptr) {
  chrome::RecordDialogCreation(chrome::DialogIdentifier::INTENT_PICKER);
}

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

  intent_picker_bubble_ = nullptr;
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

size_t IntentPickerBubbleView::GetScrollViewSizeForTesting() const {
  return GetScrollViewSize();
}

std::string IntentPickerBubbleView::GetPackageNameForTesting(
    size_t index) const {
  return app_info_[index].package_name;
}
