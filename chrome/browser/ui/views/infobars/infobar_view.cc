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

InfoBarView::InfoBarView(InfoBarDelegate* delegate)
    : InfoBar(delegate),
      delegate_(delegate),
      icon_(NULL),
      close_button_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(delete_factory_(this)),
      target_height_(kDefaultTargetHeight) {
  set_parent_owned(false);  // InfoBar deletes itself at the appropriate time.

  InfoBarDelegate::Type infobar_type = delegate->GetInfoBarType();
  set_background(new InfoBarBackground(infobar_type));
  SetAccessibleName(l10n_util::GetStringUTF16(
      (infobar_type == InfoBarDelegate::WARNING_TYPE) ?
      IDS_ACCNAME_INFOBAR_WARNING : IDS_ACCNAME_INFOBAR_PAGE_ACTION));

  animation_.reset(new ui::SlideAnimation(this));
  animation_->SetTweenType(ui::Tween::LINEAR);
}

void InfoBarView::AnimateOpen() {
  animation_->Show();
}

void InfoBarView::Open() {
  // Set the animation value to 1.0 so that GetPreferredSize() returns the right
  // size.
  animation_->Reset(1.0);
  if (container_)
    container_->OnInfoBarAnimated(true);
}

void InfoBarView::AnimateClose() {
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
}

void InfoBarView::Close() {
  parent()->RemoveChildView(this);
  if (container_)
    container_->OnInfoBarAnimated(true);
  // Note that we only tell the delegate we're closed here, and not when we're
  // simply destroyed (by virtue of a tab switch or being moved from window to
  // window), since this action can cause the delegate to destroy itself.
  if (delegate_) {
    delegate_->InfoBarClosed();
    delegate_ = NULL;
  }
}

void InfoBarView::PaintArrow(gfx::Canvas* canvas,
                             View* outer_view,
                             int arrow_center_x) {
  gfx::Point infobar_top(0, y());
  ConvertPointToView(parent(), outer_view, &infobar_top);
  int infobar_top_y = infobar_top.y();
  SkPoint gradient_points[2] = {
      {SkIntToScalar(0), SkIntToScalar(infobar_top_y)},
      {SkIntToScalar(0), SkIntToScalar(infobar_top_y + target_height_)}
  };
  InfoBarDelegate::Type infobar_type = delegate_->GetInfoBarType();
  SkColor gradient_colors[2] = {
      InfoBarBackground::GetTopColor(infobar_type),
      InfoBarBackground::GetBottomColor(infobar_type),
  };
  SkShader* gradient_shader = SkGradientShader::CreateLinear(gradient_points,
      gradient_colors, NULL, 2, SkShader::kMirror_TileMode);
  SkPaint paint;
  paint.setStrokeWidth(1);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setShader(gradient_shader);
  gradient_shader->unref();

  // The size of the arrow (its height; also half its width).  The
  // arrow area is |arrow_size| ^ 2.  By taking the square root of the
  // animation value, we cause a linear animation of the area, which
  // matches the perception of the animation of the InfoBar.
  const int kArrowSize = 10;
  int arrow_size = static_cast<int>(kArrowSize *
                                    sqrt(animation_->GetCurrentValue()));
  SkPath fill_path;
  fill_path.moveTo(SkPoint::Make(SkIntToScalar(arrow_center_x - arrow_size),
                                 SkIntToScalar(infobar_top_y)));
  fill_path.rLineTo(SkIntToScalar(arrow_size), SkIntToScalar(-arrow_size));
  fill_path.rLineTo(SkIntToScalar(arrow_size), SkIntToScalar(arrow_size));
  SkPath border_path(fill_path);
  fill_path.close();
  gfx::CanvasSkia* canvas_skia = canvas->AsCanvasSkia();
  canvas_skia->drawPath(fill_path, paint);

  // Fill and stroke have different opinions about how to treat paths.  Because
  // in Skia integral coordinates represent pixel boundaries, offsetting the
  // path makes it go exactly through pixel centers; this results in lines that
  // are exactly where we expect, instead of having odd "off by one" issues.
  // Were we to do this for |fill_path|, however, which tries to fill "inside"
  // the path (using some questionable math), we'd get a fill at a very
  // different place than we'd want.
  border_path.offset(SK_ScalarHalf, SK_ScalarHalf);
  paint.setShader(NULL);
  paint.setColor(SkColorSetA(ResourceBundle::toolbar_separator_color,
                             SkColorGetA(gradient_colors[0])));
  paint.setStyle(SkPaint::kStroke_Style);
  canvas_skia->drawPath(border_path, paint);
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
    gfx::Size icon_size = icon_->GetPreferredSize();
    icon_->SetBounds(start_x, OffsetY(this, icon_size), icon_size.width(),
                     icon_size.height());
    start_x += icon_->bounds().right();
  }

  gfx::Size button_size = close_button_->GetPreferredSize();
  close_button_->SetBounds(std::max(start_x + ContentMinimumWidth(),
      width() - kHorizontalPadding - button_size.width()),
      OffsetY(this, button_size), button_size.width(), button_size.height());
}

void InfoBarView::ViewHierarchyChanged(bool is_add, View* parent, View* child) {
  View::ViewHierarchyChanged(is_add, parent, child);

  if (child == this) {
    if (is_add) {
      // The container_ pointer must be set before adding to the view hierarchy.
      DCHECK(container_);
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
      NotifyAccessibilityEvent(AccessibilityTypes::EVENT_ALERT);

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

int InfoBarView::OffsetY(View* parent, const gfx::Size prefsize) const {
  return CenterY(prefsize) - (target_height_ - parent->height());
}

AccessibilityTypes::Role InfoBarView::GetAccessibleRole() {
  return AccessibilityTypes::ROLE_ALERT;
}

gfx::Size InfoBarView::GetPreferredSize() {
  return gfx::Size(0,
      static_cast<int>(target_height_ * animation_->GetCurrentValue()));
}

void InfoBarView::FocusWillChange(View* focused_before, View* focused_now) {
  // This will trigger some screen readers to read the entire contents of this
  // infobar.
  if (focused_before && focused_now && !this->Contains(focused_before) &&
      this->Contains(focused_now))
    NotifyAccessibilityEvent(AccessibilityTypes::EVENT_ALERT);
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

void InfoBarView::DeleteSelf() {
  delete this;
}
