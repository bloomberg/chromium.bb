// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/infobar_view.h"

#if defined(OS_WIN)
#include <shellapi.h>
#endif

#include <algorithm>

#include "base/memory/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/infobars/infobar_delegate.h"
#include "chrome/browser/ui/views/infobars/infobar_background.h"
#include "chrome/browser/ui/views/infobars/infobar_button_border.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/theme_resources_standard.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas_skia_paint.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/focus/external_focus_tracker.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

#if defined(OS_WIN)
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "ui/base/win/hwnd_util.h"
#include "ui/gfx/icon_util.h"
#endif

// static
const int InfoBar::kSeparatorLineHeight =
    views::NonClientFrameView::kClientEdgeThickness;
const int InfoBar::kDefaultArrowTargetHeight = 9;
const int InfoBar::kMaximumArrowTargetHeight = 24;
const int InfoBar::kDefaultArrowTargetHalfWidth = kDefaultArrowTargetHeight;
const int InfoBar::kMaximumArrowTargetHalfWidth = 14;
const int InfoBar::kDefaultBarTargetHeight = 36;

const int InfoBarView::kButtonButtonSpacing = 10;
const int InfoBarView::kEndOfLabelSpacing = 16;
const int InfoBarView::kHorizontalPadding = 6;

InfoBarView::InfoBarView(InfoBarTabHelper* owner, InfoBarDelegate* delegate)
    : InfoBar(owner, delegate),
      icon_(NULL),
      close_button_(NULL) {
  set_parent_owned(false);  // InfoBar deletes itself at the appropriate time.
  set_background(new InfoBarBackground(delegate->GetInfoBarType()));
}

InfoBarView::~InfoBarView() {
  // We should have closed any open menus in PlatformSpecificHide(), then
  // subclasses' RunMenu() functions should have prevented opening any new ones
  // once we became unowned.
  DCHECK(!menu_runner_.get());
}

views::Label* InfoBarView::CreateLabel(const string16& text) const {
  views::Label* label = new views::Label(text,
      ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::MediumFont));
  label->SetBackgroundColor(background()->get_color());
  label->SetEnabledColor(SK_ColorBLACK);
  label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  return label;
}

views::Link* InfoBarView::CreateLink(const string16& text,
                                     views::LinkListener* listener) const {
  views::Link* link = new views::Link;
  link->SetText(text);
  link->SetFont(
      ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::MediumFont));
  link->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  link->set_listener(listener);
  link->SetBackgroundColor(background()->get_color());
  link->set_focusable(true);
  return link;
}

// static
views::MenuButton* InfoBarView::CreateMenuButton(
    const string16& text,
    views::ViewMenuDelegate* menu_delegate) {
  views::MenuButton* menu_button = new views::MenuButton(
      NULL, text, menu_delegate, true);
  menu_button->set_border(new InfoBarButtonBorder);
  menu_button->set_animate_on_state_change(false);
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  menu_button->set_menu_marker(
      rb.GetBitmapNamed(IDR_INFOBARBUTTON_MENU_DROPARROW));
  menu_button->SetEnabledColor(SK_ColorBLACK);
  menu_button->SetHighlightColor(SK_ColorBLACK);
  menu_button->SetHoverColor(SK_ColorBLACK);
  menu_button->SetFont(rb.GetFont(ResourceBundle::MediumFont));
  menu_button->set_focusable(true);
  return menu_button;
}

// static
views::TextButton* InfoBarView::CreateTextButton(
    views::ButtonListener* listener,
    const string16& text,
    bool needs_elevation) {
  views::TextButton* text_button = new views::TextButton(listener, text);
  text_button->set_border(new InfoBarButtonBorder);
  text_button->set_animate_on_state_change(false);
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
    if (SUCCEEDED((*func)(SIID_SHIELD, SHGSI_ICON | SHGSI_SMALLICON,
                          &icon_info))) {
      scoped_ptr<SkBitmap> icon(IconUtil::CreateSkBitmapFromHICON(
          icon_info.hIcon, gfx::Size(GetSystemMetrics(SM_CXSMICON),
                                     GetSystemMetrics(SM_CYSMICON))));
      if (icon.get())
        text_button->SetIcon(*icon);
      DestroyIcon(icon_info.hIcon);
    }
  }
#endif
  text_button->set_focusable(true);
  return text_button;
}

void InfoBarView::Layout() {
  // Calculate the fill and stroke paths.  We do this here, rather than in
  // PlatformSpecificRecalculateHeight(), because this is also reached when our
  // width is changed, which affects both paths.
  stroke_path_.rewind();
  fill_path_.rewind();
  const InfoBarContainer::Delegate* delegate = container_delegate();
  if (delegate) {
    static_cast<InfoBarBackground*>(background())->set_separator_color(
        delegate->GetInfoBarSeparatorColor());
    int arrow_x;
    SkScalar arrow_fill_height =
        SkIntToScalar(std::max(arrow_height() - kSeparatorLineHeight, 0));
    SkScalar arrow_fill_half_width = SkIntToScalar(arrow_half_width());
    SkScalar separator_height = SkIntToScalar(kSeparatorLineHeight);
    if (delegate->DrawInfoBarArrows(&arrow_x) && arrow_fill_height) {
      // Skia pixel centers are at the half-values, so the arrow is horizontally
      // centered at |arrow_x| + 0.5.  Vertically, the stroke path is the center
      // of the separator, while the fill path is a closed path that extends up
      // through the entire height of the separator and down to the bottom of
      // the arrow where it joins the bar.
      stroke_path_.moveTo(
          SkIntToScalar(arrow_x) + SK_ScalarHalf - arrow_fill_half_width,
          SkIntToScalar(arrow_height()) - (separator_height * SK_ScalarHalf));
      stroke_path_.rLineTo(arrow_fill_half_width, -arrow_fill_height);
      stroke_path_.rLineTo(arrow_fill_half_width, arrow_fill_height);

      fill_path_ = stroke_path_;
      // Move the top of the fill path up to the top of the separator and then
      // extend it down all the way through.
      fill_path_.offset(0, -separator_height * SK_ScalarHalf);
      // This 0.01 hack prevents the fill from filling more pixels on the right
      // edge of the arrow than on the left.
      const SkScalar epsilon = 0.01f;
      fill_path_.rLineTo(-epsilon, 0);
      fill_path_.rLineTo(0, separator_height);
      fill_path_.rLineTo(epsilon - (arrow_fill_half_width * 2), 0);
      fill_path_.close();
    }
  }
  if (bar_height()) {
    fill_path_.addRect(0.0, SkIntToScalar(arrow_height()),
        SkIntToScalar(width()), SkIntToScalar(height() - kSeparatorLineHeight));
  }

  int start_x = kHorizontalPadding;
  if (icon_ != NULL) {
    gfx::Size icon_size = icon_->GetPreferredSize();
    icon_->SetBounds(start_x, OffsetY(icon_size), icon_size.width(),
                     icon_size.height());
  }

  gfx::Size button_size = close_button_->GetPreferredSize();
  close_button_->SetBounds(std::max(start_x + ContentMinimumWidth(),
      width() - kHorizontalPadding - button_size.width()), OffsetY(button_size),
      button_size.width(), button_size.height());
}

void InfoBarView::ViewHierarchyChanged(bool is_add, View* parent, View* child) {
  View::ViewHierarchyChanged(is_add, parent, child);

  if (is_add && (child == this) && (close_button_ == NULL)) {
    gfx::Image* image = delegate()->GetIcon();
    if (image) {
      icon_ = new views::ImageView;
      icon_->SetImage(image->ToSkBitmap());
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
    close_button_->set_focusable(true);
    AddChildView(close_button_);
  } else if ((close_button_ != NULL) && (parent == this) &&
      (child != close_button_) && (close_button_->parent() == this) &&
      (child_at(child_count() - 1) != close_button_)) {
    // For accessibility, ensure the close button is the last child view.
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
  // canvas_skia->clipPath(fill_path_);
  DCHECK_EQ(total_height(), height())
      << "Infobar piecewise heights do not match overall height";
  canvas->ClipRect(gfx::Rect(0, arrow_height(), width(), bar_height()));
  views::View::PaintChildren(canvas);
  canvas->Restore();
}

void InfoBarView::ButtonPressed(views::Button* sender,
                                const views::Event& event) {
  if (!owned())
    return;  // We're closing; don't call anything, it might access the owner.
  if (sender == close_button_) {
    delegate()->InfoBarDismissed();
    RemoveSelf();
  }
}

int InfoBarView::ContentMinimumWidth() const {
  return 0;
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

const InfoBarContainer::Delegate* InfoBarView::container_delegate() const {
  const InfoBarContainer* infobar_container = container();
  return infobar_container ? infobar_container->delegate() : NULL;
}

void InfoBarView::RunMenuAt(ui::MenuModel* menu_model,
                            views::MenuButton* button,
                            views::MenuItemView::AnchorPosition anchor) {
  DCHECK(owned());  // We'd better not open any menus while we're closing.
  views::MenuModelAdapter adapter(menu_model);
  gfx::Point screen_point;
  views::View::ConvertPointToScreen(button, &screen_point);
  menu_runner_.reset(new views::MenuRunner(adapter.CreateMenu()));
  // Ignore the result since we don't need to handle a deleted menu specially.
  ignore_result(menu_runner_->RunMenuAt(
      GetWidget(), button, gfx::Rect(screen_point, button->size()), anchor,
      views::MenuRunner::HAS_MNEMONICS));
}

void InfoBarView::PlatformSpecificShow(bool animate) {
  views::Widget* widget = GetWidget();
  views::FocusManager* focus_manager = GetFocusManager();
#if defined(OS_WIN)
  // If we gain focus, we want to restore it to the previously-focused element
  // when we're hidden.  So when we're in a Widget, create a focus tracker so
  // that if we gain focus we'll know what the previously-focused element was.
  if (widget) {
    focus_tracker_.reset(
        new views::ExternalFocusTracker(this, focus_manager));
  }
#endif
  if (focus_manager)
    focus_manager->AddFocusChangeListener(this);
  if (widget) {
    widget->NotifyAccessibilityEvent(
        this, ui::AccessibilityTypes::EVENT_ALERT, true);
  }
}

void InfoBarView::PlatformSpecificHide(bool animate) {
  // Cancel any menus we may have open.  It doesn't make sense to leave them
  // open while we're hidden, and if we're going to become unowned, we can't
  // allow the user to choose any options and potentially call functions that
  // try to access the owner.
  menu_runner_.reset();

  // It's possible to be called twice (once with |animate| true and once with it
  // false); in this case the second RemoveFocusChangeListener() call will
  // silently no-op.
  views::FocusManager* focus_manager = GetFocusManager();
  if (focus_manager)
    focus_manager->RemoveFocusChangeListener(this);

#if defined(OS_WIN) && !defined(USE_AURA)
  if (!animate || !focus_tracker_.get())
    return;

  // Do not restore focus (and active state with it) if some other top-level
  // window became active.
  views::Widget* widget = GetWidget();
  if (!widget || ui::DoesWindowBelongToActiveWindow(widget->GetNativeView()))
    focus_tracker_->FocusLastFocusedExternalView();
  focus_tracker_.reset();
#endif
}

void InfoBarView::PlatformSpecificOnHeightsRecalculated() {
  // Ensure that notifying our container of our size change will result in a
  // re-layout.
  InvalidateLayout();
}

void InfoBarView::GetAccessibleState(ui::AccessibleViewState* state) {
  if (delegate()) {
    state->name = l10n_util::GetStringUTF16(
        (delegate()->GetInfoBarType() == InfoBarDelegate::WARNING_TYPE) ?
        IDS_ACCNAME_INFOBAR_WARNING : IDS_ACCNAME_INFOBAR_PAGE_ACTION);
  }
  state->role = ui::AccessibilityTypes::ROLE_ALERT;
}

gfx::Size InfoBarView::GetPreferredSize() {
  return gfx::Size(0, total_height());
}

void InfoBarView::OnWillChangeFocus(View* focused_before, View* focused_now) {
  // This will trigger some screen readers to read the entire contents of this
  // infobar.
  if (focused_before && focused_now && !Contains(focused_before) &&
      Contains(focused_now) && GetWidget()) {
    GetWidget()->NotifyAccessibilityEvent(
        this, ui::AccessibilityTypes::EVENT_ALERT, true);
  }
}

void InfoBarView::OnDidChangeFocus(View* focused_before, View* focused_now) {
}
