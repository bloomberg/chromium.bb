// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/infobars.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/views/event_utils.h"
#include "chrome/browser/ui/views/infobars/infobar_container.h"
#include "chrome/browser/ui/views/infobars/infobar_text_button.h"
#include "gfx/canvas.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/animation/slide_animation.h"
#include "views/background.h"
#include "views/controls/button/image_button.h"
#include "views/controls/button/native_button.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"
#include "views/focus/external_focus_tracker.h"
#include "views/widget/widget.h"

#if defined(OS_WIN)
#include "ui/base/win/hwnd_util.h"
#endif

// static
const double InfoBar::kDefaultTargetHeight = 36.0;
const int InfoBar::kHorizontalPadding = 6;
const int InfoBar::kIconLabelSpacing = 6;
const int InfoBar::kButtonButtonSpacing = 10;
const int InfoBar::kEndOfLabelSpacing = 16;
const int InfoBar::kCloseButtonSpacing = 12;
const int InfoBar::kButtonInLabelSpacing = 5;

static const SkColor kWarningBackgroundColorTop = SkColorSetRGB(255, 242, 183);
static const SkColor kWarningBackgroundColorBottom =
    SkColorSetRGB(250, 230, 145);

static const SkColor kPageActionBackgroundColorTop =
    SkColorSetRGB(218, 231, 249);
static const SkColor kPageActionBackgroundColorBottom =
    SkColorSetRGB(179, 202, 231);

static const int kSeparatorLineHeight = 1;

// InfoBarBackground, public: --------------------------------------------------

InfoBarBackground::InfoBarBackground(InfoBarDelegate::Type infobar_type) {
  SkColor top_color;
  SkColor bottom_color;
  switch (infobar_type) {
    case InfoBarDelegate::WARNING_TYPE:
      top_color = kWarningBackgroundColorTop;
      bottom_color = kWarningBackgroundColorBottom;
      break;
    case InfoBarDelegate::PAGE_ACTION_TYPE:
      top_color = kPageActionBackgroundColorTop;
      bottom_color = kPageActionBackgroundColorBottom;
      break;
    default:
      NOTREACHED();
      break;
  }
  gradient_background_.reset(
      views::Background::CreateVerticalGradientBackground(top_color,
                                                          bottom_color));
}

// InfoBarBackground, views::Background overrides: -----------------------------

void InfoBarBackground::Paint(gfx::Canvas* canvas, views::View* view) const {
  // First paint the gradient background.
  gradient_background_->Paint(canvas, view);

  // Now paint the separator line.
  canvas->FillRectInt(ResourceBundle::toolbar_separator_color, 0,
                      view->height() - kSeparatorLineHeight, view->width(),
                      kSeparatorLineHeight);
}

// InfoBar, public: ------------------------------------------------------------

InfoBar::InfoBar(InfoBarDelegate* delegate)
    : delegate_(delegate),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          close_button_(new views::ImageButton(this))),
      ALLOW_THIS_IN_INITIALIZER_LIST(delete_factory_(this)),
      target_height_(kDefaultTargetHeight) {
  // We delete ourselves when we're removed from the view hierarchy.
  set_parent_owned(false);

  set_background(new InfoBarBackground(delegate->GetInfoBarType()));

  switch (delegate->GetInfoBarType()) {
    case InfoBarDelegate::WARNING_TYPE:
      SetAccessibleName(
          l10n_util::GetStringUTF16(IDS_ACCNAME_INFOBAR_WARNING));
      break;
    case InfoBarDelegate::PAGE_ACTION_TYPE:
      SetAccessibleName(
          l10n_util::GetStringUTF16(IDS_ACCNAME_INFOBAR_PAGE_ACTION));
      break;
    default:
      NOTREACHED();
      break;
  }

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  close_button_->SetImage(views::CustomButton::BS_NORMAL,
                          rb.GetBitmapNamed(IDR_CLOSE_BAR));
  close_button_->SetImage(views::CustomButton::BS_HOT,
                          rb.GetBitmapNamed(IDR_CLOSE_BAR_H));
  close_button_->SetImage(views::CustomButton::BS_PUSHED,
                          rb.GetBitmapNamed(IDR_CLOSE_BAR_P));
  close_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_CLOSE));
  AddChildView(close_button_);

  animation_.reset(new ui::SlideAnimation(this));
  animation_->SetTweenType(ui::Tween::LINEAR);
}

InfoBar::~InfoBar() {
}

// InfoBar, views::View overrides: ---------------------------------------------

AccessibilityTypes::Role InfoBar::GetAccessibleRole() {
  return AccessibilityTypes::ROLE_ALERT;
}

gfx::Size InfoBar::GetPreferredSize() {
  int height = static_cast<int>(target_height_ * animation_->GetCurrentValue());
  return gfx::Size(0, height);
}

void InfoBar::Layout() {
  gfx::Size button_ps = close_button_->GetPreferredSize();
  close_button_->SetBounds(width() - kHorizontalPadding - button_ps.width(),
                           OffsetY(this, button_ps), button_ps.width(),
                           button_ps.height());
}

void InfoBar::ViewHierarchyChanged(bool is_add, views::View* parent,
                                   views::View* child) {
  if (child == this) {
    if (is_add) {
      InfoBarAdded();
    } else {
      InfoBarRemoved();
    }
  }

  if (GetWidget() && GetWidget()->IsAccessibleWidget()) {
    // For accessibility, make the close button the last child view.
    if (parent == this && child != close_button_ &&
        HasChildView(close_button_) &&
        GetChildViewAt(GetChildViewCount() - 1) != close_button_) {
      RemoveChildView(close_button_);
      AddChildView(close_button_);
    }

    // Allow screen reader users to focus the close button.
    close_button_->SetFocusable(true);
  }
}

// InfoBar, protected: ---------------------------------------------------------

int InfoBar::GetAvailableWidth() const {
  return close_button_->x() - kCloseButtonSpacing;
}

void InfoBar::RemoveInfoBar() const {
  if (container_)
    container_->RemoveDelegate(delegate());
}

int InfoBar::CenterY(const gfx::Size prefsize) {
  return std::max((static_cast<int>(target_height_) -
      prefsize.height()) / 2, 0);
}

int InfoBar::OffsetY(views::View* parent, const gfx::Size prefsize) {
  return CenterY(prefsize) -
      (static_cast<int>(target_height_) - parent->height());
}

// InfoBar, views::ButtonListener implementation: ------------------

void InfoBar::ButtonPressed(views::Button* sender, const views::Event& event) {
  if (sender == close_button_) {
    if (delegate_)
      delegate_->InfoBarDismissed();
    RemoveInfoBar();
  }
}

// InfoBar, views::FocusChangeListener implementation: ------------------

void InfoBar::FocusWillChange(View* focused_before, View* focused_now) {
  // This will trigger some screen readers to read the entire contents of this
  // infobar.
  if (focused_before && focused_now &&
      !this->IsParentOf(focused_before) && this->IsParentOf(focused_now)) {
    NotifyAccessibilityEvent(AccessibilityTypes::EVENT_ALERT);
  }
}

// InfoBar, ui::AnimationDelegate implementation: ------------------------------

void InfoBar::AnimationProgressed(const ui::Animation* animation) {
  if (container_)
    container_->InfoBarAnimated(true);
}

void InfoBar::AnimationEnded(const ui::Animation* animation) {
  if (container_) {
    container_->InfoBarAnimated(false);

    if (!animation_->IsShowing())
      Close();
  }
}

// InfoBar, private: -----------------------------------------------------------

void InfoBar::AnimateOpen() {
  animation_->Show();
}

void InfoBar::Open() {
  // Set the animation value to 1.0 so that GetPreferredSize() returns the right
  // size.
  animation_->Reset(1.0);
  if (container_)
    container_->InfoBarAnimated(false);
}

void InfoBar::AnimateClose() {
  bool restore_focus = true;
#if defined(OS_WIN)
  // Do not restore focus (and active state with it) on Windows if some other
  // top-level window became active.
  if (GetWidget() &&
      !ui::DoesWindowBelongToActiveWindow(GetWidget()->GetNativeView())) {
    restore_focus = false;
  }
#endif  // defined(OS_WIN)
  DestroyFocusTracker(restore_focus);
  animation_->Hide();
}

void InfoBar::Close() {
  GetParent()->RemoveChildView(this);
  // Note that we only tell the delegate we're closed here, and not when we're
  // simply destroyed (by virtue of a tab switch or being moved from window to
  // window), since this action can cause the delegate to destroy itself.
  if (delegate_) {
    delegate_->InfoBarClosed();
    delegate_ = NULL;
  }
}

void InfoBar::InfoBarAdded() {
  // The container_ pointer must be set before adding to the view hierarchy.
  DCHECK(container_);
#if defined(OS_WIN)
  // When we're added to a view hierarchy within a widget, we create an
  // external focus tracker to track what was focused in case we obtain
  // focus so that we can restore focus when we're removed.
  views::Widget* widget = GetWidget();
  if (widget) {
    focus_tracker_.reset(new views::ExternalFocusTracker(this,
                                                         GetFocusManager()));
  }
#endif

  if (GetFocusManager())
    GetFocusManager()->AddFocusChangeListener(this);

  NotifyAccessibilityEvent(AccessibilityTypes::EVENT_ALERT);
}

void InfoBar::InfoBarRemoved() {
  DestroyFocusTracker(false);
  // NULL our container_ pointer so that if Animation::Stop results in
  // AnimationEnded being called, we do not try and delete ourselves twice.
  container_ = NULL;
  animation_->Stop();
  // Finally, clean ourselves up when we're removed from the view hierarchy
  // since no-one refers to us now.
  MessageLoop::current()->PostTask(FROM_HERE,
      delete_factory_.NewRunnableMethod(&InfoBar::DeleteSelf));

  if (GetFocusManager())
    GetFocusManager()->RemoveFocusChangeListener(this);
}

void InfoBar::DestroyFocusTracker(bool restore_focus) {
  if (focus_tracker_.get()) {
    if (restore_focus)
      focus_tracker_->FocusLastFocusedExternalView();
    focus_tracker_->SetFocusManager(NULL);
    focus_tracker_.reset(NULL);
  }
}

void InfoBar::DeleteSelf() {
  delete this;
}

// AlertInfoBar, public: -------------------------------------------------------

AlertInfoBar::AlertInfoBar(AlertInfoBarDelegate* delegate)
    : InfoBar(delegate) {
  label_ = new views::Label(
      UTF16ToWideHack(delegate->GetMessageText()),
      ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::MediumFont));
  label_->SetColor(SK_ColorBLACK);
  label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  AddChildView(label_);

  icon_ = new views::ImageView;
  if (delegate->GetIcon())
    icon_->SetImage(delegate->GetIcon());
  AddChildView(icon_);
}

AlertInfoBar::~AlertInfoBar() {
}

// AlertInfoBar, views::View overrides: ----------------------------------------

void AlertInfoBar::Layout() {
  // Layout the close button.
  InfoBar::Layout();

  // Layout the icon and text.
  gfx::Size icon_ps = icon_->GetPreferredSize();
  icon_->SetBounds(kHorizontalPadding, OffsetY(this, icon_ps), icon_ps.width(),
                   icon_ps.height());

  gfx::Size text_ps = label_->GetPreferredSize();
  int text_width = std::min(
      text_ps.width(),
      GetAvailableWidth() - icon_->bounds().right() - kIconLabelSpacing);
  label_->SetBounds(icon_->bounds().right() + kIconLabelSpacing,
                    OffsetY(this, text_ps), text_width, text_ps.height());
}

// AlertInfoBar, private: ------------------------------------------------------

AlertInfoBarDelegate* AlertInfoBar::GetDelegate() {
  return delegate()->AsAlertInfoBarDelegate();
}

// LinkInfoBar, public: --------------------------------------------------------

LinkInfoBar::LinkInfoBar(LinkInfoBarDelegate* delegate)
    : InfoBar(delegate),
      icon_(new views::ImageView),
      label_1_(new views::Label),
      label_2_(new views::Label),
      link_(new views::Link) {
  // Set up the icon.
  if (delegate->GetIcon())
    icon_->SetImage(delegate->GetIcon());
  AddChildView(icon_);

  // Set up the labels.
  size_t offset;
  string16 message_text = delegate->GetMessageTextWithOffset(&offset);
  if (offset != string16::npos) {
    label_1_->SetText(UTF16ToWideHack(message_text.substr(0, offset)));
    label_2_->SetText(UTF16ToWideHack(message_text.substr(offset)));
  } else {
    label_1_->SetText(UTF16ToWideHack(message_text));
  }
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  label_1_->SetFont(rb.GetFont(ResourceBundle::MediumFont));
  label_2_->SetFont(rb.GetFont(ResourceBundle::MediumFont));
  label_1_->SetColor(SK_ColorBLACK);
  label_2_->SetColor(SK_ColorBLACK);
  label_1_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  label_2_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  AddChildView(label_1_);
  AddChildView(label_2_);

  // Set up the link.
  link_->SetText(UTF16ToWideHack(delegate->GetLinkText()));
  link_->SetFont(rb.GetFont(ResourceBundle::MediumFont));
  link_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  link_->SetController(this);
  link_->MakeReadableOverBackgroundColor(background()->get_color());
  AddChildView(link_);
}

LinkInfoBar::~LinkInfoBar() {
}

// LinkInfoBar, views::LinkController implementation: --------------------------

void LinkInfoBar::LinkActivated(views::Link* source, int event_flags) {
  DCHECK(source == link_);
  if (GetDelegate()->LinkClicked(
          event_utils::DispositionFromEventFlags(event_flags))) {
    RemoveInfoBar();
  }
}

// LinkInfoBar, views::View overrides: -----------------------------------------

void LinkInfoBar::Layout() {
  // Layout the close button.
  InfoBar::Layout();

  // Layout the icon.
  gfx::Size icon_ps = icon_->GetPreferredSize();
  icon_->SetBounds(kHorizontalPadding, OffsetY(this, icon_ps), icon_ps.width(),
                   icon_ps.height());

  int label_1_x = icon_->bounds().right() + kIconLabelSpacing;

  // Figure out the amount of space available to the rest of the content now
  // that the close button and the icon have been positioned.
  int available_width = GetAvailableWidth() - label_1_x;

  // Layout the left label.
  gfx::Size label_1_ps = label_1_->GetPreferredSize();
  label_1_->SetBounds(label_1_x, OffsetY(this, label_1_ps), label_1_ps.width(),
                      label_1_ps.height());

  // Layout the link.
  gfx::Size link_ps = link_->GetPreferredSize();
  bool has_second_label = !label_2_->GetText().empty();
  if (has_second_label) {
    // Embed the link in the text string between the two labels.
    link_->SetBounds(label_1_->bounds().right(),
                     OffsetY(this, link_ps), link_ps.width(), link_ps.height());
  } else {
    // Right-align the link toward the edge of the InfoBar.
    link_->SetBounds(label_1_x + available_width - link_ps.width(),
                     OffsetY(this, link_ps), link_ps.width(), link_ps.height());
  }

  // Layout the right label (we do this regardless of whether or not it has
  // text).
  gfx::Size label_2_ps = label_2_->GetPreferredSize();
  label_2_->SetBounds(link_->bounds().right(),
                      OffsetY(this, label_2_ps), label_2_ps.width(),
                      label_2_ps.height());
}

// LinkInfoBar, private: -------------------------------------------------------

LinkInfoBarDelegate* LinkInfoBar::GetDelegate() {
  return delegate()->AsLinkInfoBarDelegate();
}

// ConfirmInfoBar, public: -----------------------------------------------------

ConfirmInfoBar::ConfirmInfoBar(ConfirmInfoBarDelegate* delegate)
    : AlertInfoBar(delegate),
      ok_button_(NULL),
      cancel_button_(NULL),
      link_(NULL),
      initialized_(false) {
  ok_button_ = InfoBarTextButton::Create(this,
      delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK));
  ok_button_->SetAccessibleName(WideToUTF16Hack(ok_button_->text()));
  cancel_button_ = InfoBarTextButton::Create(this,
      delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_CANCEL));
  cancel_button_->SetAccessibleName(WideToUTF16Hack(cancel_button_->text()));

  // Set up the link.
  link_ = new views::Link;
  link_->SetText(UTF16ToWideHack(delegate->GetLinkText()));
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  link_->SetFont(rb.GetFont(ResourceBundle::MediumFont));
  link_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  link_->SetController(this);
  link_->MakeReadableOverBackgroundColor(background()->get_color());
}

ConfirmInfoBar::~ConfirmInfoBar() {
  if (!initialized_) {
    delete ok_button_;
    delete cancel_button_;
    delete link_;
  }
}

// ConfirmInfoBar, views::LinkController implementation: -----------------------

void ConfirmInfoBar::LinkActivated(views::Link* source, int event_flags) {
  DCHECK(source == link_);
  DCHECK(link_->IsVisible());
  DCHECK(!link_->GetText().empty());
  if (GetDelegate()->LinkClicked(
          event_utils::DispositionFromEventFlags(event_flags))) {
    RemoveInfoBar();
  }
}

// ConfirmInfoBar, views::View overrides: --------------------------------------

void ConfirmInfoBar::Layout() {
  // First layout right aligned items (from right to left) in order to determine
  // the space avalable, then layout the left aligned items.

  // Layout the close button.
  InfoBar::Layout();

  // Layout the cancel and OK buttons.
  int available_width = AlertInfoBar::GetAvailableWidth();
  int ok_button_width = 0;
  int cancel_button_width = 0;
  gfx::Size ok_ps = ok_button_->GetPreferredSize();
  gfx::Size cancel_ps = cancel_button_->GetPreferredSize();

  if (GetDelegate()->GetButtons() & ConfirmInfoBarDelegate::BUTTON_OK) {
    ok_button_width = ok_ps.width();
  } else {
    ok_button_->SetVisible(false);
  }
  if (GetDelegate()->GetButtons() & ConfirmInfoBarDelegate::BUTTON_CANCEL) {
    cancel_button_width = cancel_ps.width();
  } else {
    cancel_button_->SetVisible(false);
  }

  cancel_button_->SetBounds(available_width - cancel_button_width,
                            OffsetY(this, cancel_ps), cancel_ps.width(),
                            cancel_ps.height());
  int spacing = cancel_button_width > 0 ? kButtonButtonSpacing : 0;
  ok_button_->SetBounds(cancel_button_->x() - spacing - ok_button_width,
                        OffsetY(this, ok_ps), ok_ps.width(), ok_ps.height());

  // Layout the icon and label.
  AlertInfoBar::Layout();

  // Now append the link to the label's right edge.
  link_->SetVisible(!link_->GetText().empty());
  gfx::Size link_ps = link_->GetPreferredSize();
  int link_x = label()->bounds().right() + kEndOfLabelSpacing;
  int link_w = std::min(GetAvailableWidth() - link_x, link_ps.width());
  link_->SetBounds(link_x, OffsetY(this, link_ps), link_w, link_ps.height());
}

void ConfirmInfoBar::ViewHierarchyChanged(bool is_add,
                                          views::View* parent,
                                          views::View* child) {
  if (is_add && child == this && !initialized_) {
    Init();
    initialized_ = true;
  }
  InfoBar::ViewHierarchyChanged(is_add, parent, child);
}

// ConfirmInfoBar, views::ButtonListener implementation: ---------------

void ConfirmInfoBar::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  InfoBar::ButtonPressed(sender, event);
  if (sender == ok_button_) {
    if (GetDelegate()->Accept())
      RemoveInfoBar();
  } else if (sender == cancel_button_) {
    if (GetDelegate()->Cancel())
      RemoveInfoBar();
  }
}

// ConfirmInfoBar, InfoBar overrides: ------------------------------------------

int ConfirmInfoBar::GetAvailableWidth() const {
  return ok_button_->x() - kEndOfLabelSpacing;
}

// ConfirmInfoBar, private: ----------------------------------------------------

ConfirmInfoBarDelegate* ConfirmInfoBar::GetDelegate() {
  return delegate()->AsConfirmInfoBarDelegate();
}

void ConfirmInfoBar::Init() {
  AddChildView(ok_button_);
  AddChildView(cancel_button_);
  AddChildView(link_);
}

// AlertInfoBarDelegate, InfoBarDelegate overrides: ----------------------------

InfoBar* AlertInfoBarDelegate::CreateInfoBar() {
  return new AlertInfoBar(this);
}

// LinkInfoBarDelegate, InfoBarDelegate overrides: -----------------------------

InfoBar* LinkInfoBarDelegate::CreateInfoBar() {
  return new LinkInfoBar(this);
}

// ConfirmInfoBarDelegate, InfoBarDelegate overrides: --------------------------

InfoBar* ConfirmInfoBarDelegate::CreateInfoBar() {
  return new ConfirmInfoBar(this);
}
