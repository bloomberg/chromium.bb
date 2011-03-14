// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/infobar_view.h"

#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/tab_contents/confirm_infobar_delegate.h"
#include "chrome/browser/ui/views/infobars/infobar_background.h"
#include "chrome/browser/ui/views/infobars/infobar_button_border.h"
#include "chrome/browser/ui/views/infobars/infobar_container.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas_skia_paint.h"
#include "views/controls/button/image_button.h"
#include "views/controls/button/menu_button.h"
#include "views/controls/button/text_button.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"
#include "views/controls/link.h"
#include "views/focus/external_focus_tracker.h"
#include "views/widget/widget.h"

#if defined(OS_WIN)
#include <shellapi.h>

#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "ui/base/win/hwnd_util.h"
#include "ui/gfx/icon_util.h"
#endif

// static
const int InfoBarView::kDefaultTargetHeight = 36;
const int InfoBarView::kButtonButtonSpacing = 10;
const int InfoBarView::kEndOfLabelSpacing = 16;
const int InfoBarView::kHorizontalPadding = 6;

const int InfoBarView::kCurveWidth = 13;
const int InfoBarView::kMaxIconWidth = 30;
const int InfoBarView::kTabHeight = 9;
const int InfoBarView::kTabIconPadding = 2;

const int InfoBarView::kTabWidth = (kCurveWidth + kTabIconPadding) * 2 +
    kMaxIconWidth;

InfoBarView::InfoBarView(InfoBarDelegate* delegate)
    : InfoBar(delegate),
      container_(NULL),
      delegate_(delegate),
      icon_(NULL),
      close_button_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(animation_(new ui::SlideAnimation(this))),
      ALLOW_THIS_IN_INITIALIZER_LIST(delete_factory_(this)),
      target_height_(kDefaultTargetHeight),
      fill_path_(new SkPath),
      stroke_path_(new SkPath) {
  set_parent_owned(false);  // InfoBar deletes itself at the appropriate time.

  InfoBarDelegate::Type infobar_type = delegate->GetInfoBarType();
  set_background(new InfoBarBackground(infobar_type));

  animation_->SetTweenType(ui::Tween::LINEAR);
}

void InfoBarView::Show(bool animate) {
  if (animate) {
    animation_->Show();
  } else {
    animation_->Reset(1.0);
    if (container_)
      container_->OnInfoBarAnimated(true);
  }
}

void InfoBarView::Hide(bool animate) {
  if (animate) {
    bool restore_focus = true;
  #if defined(OS_WIN)
    // Do not restore focus (and active state with it) on Windows if some other
    // top-level window became active.
    if (GetWidget() &&
        !ui::DoesWindowBelongToActiveWindow(GetWidget()->GetNativeView()))
      restore_focus = false;
  #endif  // defined(OS_WIN)
    DestroyFocusTracker(restore_focus);
    animation_->Hide();
  } else {
    animation_->Reset(0.0);
    Close();
  }
}

InfoBarView::~InfoBarView() {
}

// static
views::Label* InfoBarView::CreateLabel(const string16& text) {
  views::Label* label = new views::Label(UTF16ToWideHack(text),
      ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::MediumFont));
  label->SetColor(SK_ColorBLACK);
  label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  return label;
}

// static
views::Link* InfoBarView::CreateLink(const string16& text,
                                     views::LinkController* controller,
                                     const SkColor& background_color) {
  views::Link* link = new views::Link;
  link->SetText(UTF16ToWideHack(text));
  link->SetFont(
      ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::MediumFont));
  link->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  link->SetController(controller);
  link->MakeReadableOverBackgroundColor(background_color);
  return link;
}

// static
views::MenuButton* InfoBarView::CreateMenuButton(
    const string16& text,
    bool normal_has_border,
    views::ViewMenuDelegate* menu_delegate) {
  views::MenuButton* menu_button =
      new views::MenuButton(NULL, UTF16ToWideHack(text), menu_delegate, true);
  menu_button->set_border(new InfoBarButtonBorder);
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  menu_button->set_menu_marker(
      rb.GetBitmapNamed(IDR_INFOBARBUTTON_MENU_DROPARROW));
  if (normal_has_border) {
    menu_button->SetNormalHasBorder(true);
    menu_button->SetAnimationDuration(0);
  }
  menu_button->SetEnabledColor(SK_ColorBLACK);
  menu_button->SetHighlightColor(SK_ColorBLACK);
  menu_button->SetHoverColor(SK_ColorBLACK);
  menu_button->SetFont(rb.GetFont(ResourceBundle::MediumFont));
  return menu_button;
}

// static
views::TextButton* InfoBarView::CreateTextButton(
    views::ButtonListener* listener,
    const string16& text,
    bool needs_elevation) {
  views::TextButton* text_button =
      new views::TextButton(listener, UTF16ToWideHack(text));
  text_button->set_border(new InfoBarButtonBorder);
  text_button->SetNormalHasBorder(true);
  text_button->SetAnimationDuration(0);
  text_button->SetEnabledColor(SK_ColorBLACK);
  text_button->SetHighlightColor(SK_ColorBLACK);
  text_button->SetHoverColor(SK_ColorBLACK);
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  text_button->SetFont(rb.GetFont(ResourceBundle::MediumFont));
#if defined(OS_WIN)
  if (needs_elevation &&
      (base::win::GetVersion() >= base::win::VERSION_VISTA) &&
      base::win::UserAccountControlIsEnabled()) {
    SHSTOCKICONINFO icon_info = { sizeof SHSTOCKICONINFO };
    // Even with the runtime guard above, we have to use GetProcAddress() here,
    // because otherwise the loader will try to resolve the function address on
    // startup, which will break on XP.
    typedef HRESULT (STDAPICALLTYPE *GetStockIconInfo)(SHSTOCKICONID, UINT,
                                                       SHSTOCKICONINFO*);
    GetStockIconInfo func = reinterpret_cast<GetStockIconInfo>(
        GetProcAddress(GetModuleHandle(L"shell32.dll"), "SHGetStockIconInfo"));
    (*func)(SIID_SHIELD, SHGSI_ICON | SHGSI_SMALLICON, &icon_info);
    text_button->SetIcon(*IconUtil::CreateSkBitmapFromHICON(icon_info.hIcon,
        gfx::Size(GetSystemMetrics(SM_CXSMICON),
                  GetSystemMetrics(SM_CYSMICON))));
  }
#endif
  return text_button;
}

void InfoBarView::Layout() {
  int start_x = kHorizontalPadding;
  if (icon_ != NULL) {
    // Center the icon horizontally within the tab, and vertically between the
    // entire height (tab + bar).
    gfx::Size icon_size = icon_->GetPreferredSize();
    int center_x = std::max((kTabWidth - icon_size.width()) / 2, 0);
    int full_height = target_height_ + kTabHeight;

    // This duplicates OffsetY except centered within the entire height (tab +
    // bar) instead of just within the bar.
    int offset_y =
        std::max((full_height - icon_size.height()) / 2, 0) -
        (full_height - height());
    icon_->SetBounds(center_x, offset_y, icon_size.width(), icon_size.height());
    start_x += icon_->bounds().right();
  }

  gfx::Size button_size = close_button_->GetPreferredSize();
  close_button_->SetBounds(std::max(start_x + ContentMinimumWidth(),
      width() - kHorizontalPadding - button_size.width()),
      OffsetY(button_size), button_size.width(), button_size.height());
}

void InfoBarView::ViewHierarchyChanged(bool is_add, View* parent, View* child) {
  View::ViewHierarchyChanged(is_add, parent, child);

  if (child == this) {
    if (is_add) {
#if defined(OS_WIN)
      // When we're added to a view hierarchy within a widget, we create an
      // external focus tracker to track what was focused in case we obtain
      // focus so that we can restore focus when we're removed.
      views::Widget* widget = GetWidget();
      if (widget) {
        focus_tracker_.reset(
            new views::ExternalFocusTracker(this, GetFocusManager()));
      }
#endif
      if (GetFocusManager())
        GetFocusManager()->AddFocusChangeListener(this);
      if (GetWidget()) {
        GetWidget()->NotifyAccessibilityEvent(
            this, ui::AccessibilityTypes::EVENT_ALERT, true);
      }

      if (close_button_ == NULL) {
        SkBitmap* image = delegate_->GetIcon();
        if (image) {
          icon_ = new views::ImageView;
          icon_->SetImage(image);
          AddChildView(icon_);
        }

        close_button_ = new views::ImageButton(this);
        ResourceBundle& rb = ResourceBundle::GetSharedInstance();
        close_button_->SetImage(views::CustomButton::BS_NORMAL,
                                rb.GetBitmapNamed(IDR_CLOSE_BAR));
        close_button_->SetImage(views::CustomButton::BS_HOT,
                                rb.GetBitmapNamed(IDR_CLOSE_BAR_H));
        close_button_->SetImage(views::CustomButton::BS_PUSHED,
                                rb.GetBitmapNamed(IDR_CLOSE_BAR_P));
        close_button_->SetAccessibleName(
            l10n_util::GetStringUTF16(IDS_ACCNAME_CLOSE));
        close_button_->SetFocusable(true);
        AddChildView(close_button_);
      }
    } else {
      DestroyFocusTracker(false);
      // NULL our container_ pointer so that if Animation::Stop results in
      // AnimationEnded being called, we do not try and delete ourselves twice.
      container_ = NULL;
      animation_->Stop();
      // Finally, clean ourselves up when we're removed from the view hierarchy
      // since no-one refers to us now.
      MessageLoop::current()->PostTask(FROM_HERE,
          delete_factory_.NewRunnableMethod(&InfoBarView::DeleteSelf));
      if (GetFocusManager())
        GetFocusManager()->RemoveFocusChangeListener(this);
    }
  }

  // For accessibility, ensure the close button is the last child view.
  if ((close_button_ != NULL) && (parent == this) && (child != close_button_) &&
      (close_button_->parent() == this) &&
      (GetChildViewAt(child_count() - 1) != close_button_)) {
    RemoveChildView(close_button_);
    AddChildView(close_button_);
  }
}

void InfoBarView::PaintChildren(gfx::Canvas* canvas) {
  canvas->Save();

  // TODO(scr): This really should be the |fill_path_|, but the clipPath seems
  // broken on non-Windows platforms (crbug.com/75154). For now, just clip to
  // the bar bounds.
  //
  // gfx::CanvasSkia* canvas_skia = canvas->AsCanvasSkia();
  // canvas_skia->clipPath(*fill_path_);
  int tab_height = AnimatedTabHeight();
  int bar_height = AnimatedBarHeight();
  DCHECK_EQ(tab_height + bar_height, height())
      << "Animation progressed between OnBoundsChanged & PaintChildren.";
  canvas->ClipRectInt(0, tab_height, width(), bar_height);

  views::View::PaintChildren(canvas);
  canvas->Restore();
}

void InfoBarView::ButtonPressed(views::Button* sender,
                                const views::Event& event) {
  if (sender == close_button_) {
    if (delegate_)
      delegate_->InfoBarDismissed();
    RemoveInfoBar();
  }
}

void InfoBarView::AnimationProgressed(const ui::Animation* animation) {
  if (container_)
    container_->OnInfoBarAnimated(false);
}

int InfoBarView::ContentMinimumWidth() const {
  return 0;
}

void InfoBarView::RemoveInfoBar() const {
  if (container_)
    container_->RemoveDelegate(delegate());
}

int InfoBarView::StartX() const {
  // Ensure we don't return a value greater than EndX(), so children can safely
  // set something's width to "EndX() - StartX()" without risking that being
  // negative.
  return std::min(EndX(),
      ((icon_ != NULL) ? icon_->bounds().right() : 0) + kHorizontalPadding);
}

int InfoBarView::EndX() const {
  const int kCloseButtonSpacing = 12;
  return close_button_->x() - kCloseButtonSpacing;
}

int InfoBarView::CenterY(const gfx::Size prefsize) const {
  return std::max((target_height_ - prefsize.height()) / 2, 0);
}

int InfoBarView::OffsetY(const gfx::Size prefsize) const {
  return CenterY(prefsize) + AnimatedTabHeight() -
      (target_height_ - AnimatedBarHeight());
}

void InfoBarView::GetAccessibleState(ui::AccessibleViewState* state) {
  if (delegate_) {
    state->name = l10n_util::GetStringUTF16(
        (delegate_->GetInfoBarType() == InfoBarDelegate::WARNING_TYPE) ?
        IDS_ACCNAME_INFOBAR_WARNING : IDS_ACCNAME_INFOBAR_PAGE_ACTION);
  }
  state->role = ui::AccessibilityTypes::ROLE_ALERT;
}

int InfoBarView::AnimatedTabHeight() const {
  return static_cast<int>(kTabHeight * animation_->GetCurrentValue());
}

int InfoBarView::AnimatedBarHeight() const {
  return static_cast<int>(target_height_ * animation_->GetCurrentValue());
}

gfx::Size InfoBarView::GetPreferredSize() {
  return gfx::Size(0, AnimatedTabHeight() + AnimatedBarHeight());
}

void InfoBarView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  int tab_height = AnimatedTabHeight();
  int bar_height = AnimatedBarHeight();
  int divider_y = tab_height - 1;
  DCHECK_EQ(tab_height + bar_height, height())
      << "Animation progressed between Layout & OnBoundsChanged.";

  int mirrored_x = GetMirroredXWithWidthInView(0, kTabWidth);
  stroke_path_->rewind();
  fill_path_->rewind();

  if (tab_height) {
    stroke_path_->moveTo(SkIntToScalar(mirrored_x),
                         SkIntToScalar(divider_y));
    stroke_path_->rCubicTo(
        SkScalarDiv(kCurveWidth, 2), 0.0,
        SkScalarDiv(kCurveWidth, 2),
        SkIntToScalar(-divider_y),
        SkIntToScalar(kCurveWidth),
        SkIntToScalar(-divider_y));
    stroke_path_->rLineTo(SkScalarMulAdd(kTabIconPadding, 2, kMaxIconWidth),
                          0.0);
    stroke_path_->rCubicTo(
        SkScalarDiv(kCurveWidth, 2), 0.0,
        SkScalarDiv(kCurveWidth, 2),
        SkIntToScalar(divider_y),
        SkIntToScalar(kCurveWidth),
        SkIntToScalar(divider_y));

    // Create the fill portion of the tab.  Because the fill is inside the
    // bounds and will not cover the separator, we need to extend downward by a
    // pixel before closing.
    *fill_path_ = *stroke_path_;
    fill_path_->rLineTo(0.0, 1.0);
    fill_path_->rLineTo(-SkIntToScalar(kTabWidth), 0.0);
    fill_path_->close();

    // Fill and stroke have different opinions about how to treat paths.
    // Because in Skia integral coordinates represent pixel boundaries,
    // offsetting the path makes it go exactly through pixel centers; this
    // results in lines that are exactly where we expect, instead of having odd
    // "off by one" issues.  Were we to do this for |fill_path|, however, which
    // tries to fill "inside" the path (using some questionable math), we'd get
    // a fill at a very different place than we'd want.
    stroke_path_->offset(SK_ScalarHalf, SK_ScalarHalf);
  }
  if (bar_height) {
    fill_path_->addRect(0.0, SkIntToScalar(tab_height),
                        SkIntToScalar(width()), SkIntToScalar(height()));
  }
}

void InfoBarView::FocusWillChange(View* focused_before, View* focused_now) {
  // This will trigger some screen readers to read the entire contents of this
  // infobar.
  if (focused_before && focused_now && !this->Contains(focused_before) &&
      this->Contains(focused_now) && GetWidget()) {
    GetWidget()->NotifyAccessibilityEvent(
        this, ui::AccessibilityTypes::EVENT_ALERT, true);
  }
}

void InfoBarView::AnimationEnded(const ui::Animation* animation) {
  if (container_ && !animation_->IsShowing())
    Close();
}

void InfoBarView::DestroyFocusTracker(bool restore_focus) {
  if (focus_tracker_ != NULL) {
    if (restore_focus)
      focus_tracker_->FocusLastFocusedExternalView();
    focus_tracker_->SetFocusManager(NULL);
    focus_tracker_.reset();
  }
}

void InfoBarView::Close() {
  DCHECK(container_);
  // WARNING: RemoveInfoBar() will eventually call back to
  // ViewHierarchyChanged(), which nulls |container_|!
  InfoBarContainer* container = container_;
  container->RemoveInfoBar(this);
  container->OnInfoBarAnimated(true);

  // Note that we only tell the delegate we're closed here, and not when we're
  // simply destroyed (by virtue of a tab switch or being moved from window to
  // window), since this action can cause the delegate to destroy itself.
  if (delegate_) {
    delegate_->InfoBarClosed();
    delegate_ = NULL;
  }
}

void InfoBarView::DeleteSelf() {
  delete this;
}
