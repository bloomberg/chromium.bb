// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bubble/bubble.h"

#include <vector>

#include "chrome/browser/ui/views/bubble/border_contents.h"
#include "chrome/common/chrome_notification_types.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/window/client_view.h"
#include "views/layout/fill_layout.h"
#include "views/widget/widget.h"

#if defined(OS_CHROMEOS) && defined(TOOLKIT_USES_GTK)
#include "chrome/browser/chromeos/legacy_window_manager/wm_ipc.h"
#include "third_party/cros_system_api/window_manager/chromeos_wm_ipc_enums.h"
#endif

#if defined(OS_WIN) && !defined(USE_AURA)
#include "chrome/browser/ui/views/bubble/border_widget_win.h"
#endif

using std::vector;

// How long the fade should last for.
static const int kHideFadeDurationMS = 200;

// Background color of the bubble.
#if defined(OS_WIN) && !defined(USE_AURA)
const SkColor Bubble::kBackgroundColor =
    color_utils::GetSysSkColor(COLOR_WINDOW);
#else
// TODO(beng): source from theme provider.
const SkColor Bubble::kBackgroundColor = SK_ColorWHITE;
#endif

// BubbleDelegate ---------------------------------------------------------

string16 BubbleDelegate::GetAccessibleName() {
  return string16();
}

// Bubble -----------------------------------------------------------------

// static
Bubble* Bubble::Show(views::Widget* parent,
                     const gfx::Rect& position_relative_to,
                     views::BubbleBorder::ArrowLocation arrow_location,
                     views::BubbleBorder::BubbleAlignment alignment,
                     views::View* contents,
                     BubbleDelegate* delegate) {
  Bubble* bubble = new Bubble;
  bubble->InitBubble(parent, position_relative_to, arrow_location, alignment,
                     contents, delegate);

  // Register the Escape accelerator for closing.
  bubble->RegisterEscapeAccelerator();

  if (delegate)
    delegate->BubbleShown();

  return bubble;
}

#if defined(OS_CHROMEOS)
// static
Bubble* Bubble::ShowFocusless(
    views::Widget* parent,
    const gfx::Rect& position_relative_to,
    views::BubbleBorder::ArrowLocation arrow_location,
    views::BubbleBorder::BubbleAlignment alignment,
    views::View* contents,
    BubbleDelegate* delegate,
    bool show_while_screen_is_locked) {
  Bubble* bubble = new Bubble(views::Widget::InitParams::TYPE_POPUP,
                              show_while_screen_is_locked);
  bubble->InitBubble(parent, position_relative_to, arrow_location, alignment,
                     contents, delegate);
  return bubble;
}
#endif

void Bubble::Close() {
  if (show_status_ != kOpen)
    return;

  show_status_ = kClosing;

  if (fade_away_on_close_)
    FadeOut();
  else
    DoClose(false);
}

void Bubble::AnimationEnded(const ui::Animation* animation) {
  if (static_cast<int>(animation_->GetCurrentValue()) == 0) {
    // When fading out we just need to close the bubble at the end
    DoClose(false);
  } else {
#if defined(OS_WIN) && !defined(USE_AURA)
    // When fading in we need to remove the layered window style flag, since
    // that style prevents some bubble content from working properly.
    SetWindowLong(GWL_EXSTYLE, GetWindowLong(GWL_EXSTYLE) & ~WS_EX_LAYERED);
#endif
  }
}

void Bubble::AnimationProgressed(const ui::Animation* animation) {
  // Set the opacity for the main contents window.
  unsigned char opacity = static_cast<unsigned char>(
      animation_->GetCurrentValue() * 255);
#if defined(OS_WIN) && !defined(USE_AURA)
  SetLayeredWindowAttributes(GetNativeView(), 0,
      static_cast<byte>(opacity), LWA_ALPHA);
  contents_->SchedulePaint();

  // Also fade in/out the bubble border window.
  border_->SetOpacity(opacity);
  border_->border_contents()->SchedulePaint();
#else
  SetOpacity(opacity);
  border_contents_->SchedulePaint();
#endif
}

Bubble::Bubble()
    :
#if defined(USE_AURA)
      views::NativeWidgetAura(new views::Widget),
#elif defined(OS_WIN)
      views::NativeWidgetWin(new views::Widget),
#elif defined(TOUCH_UI)
      views::NativeWidgetViews(new views::Widget),
#elif defined(TOOLKIT_USES_GTK)
      views::NativeWidgetGtk(new views::Widget),
#endif
#if defined(OS_WIN) && !defined(USE_AURA)
      border_(NULL),
#else
      border_contents_(NULL),
#endif
      delegate_(NULL),
      show_status_(kOpen),
      fade_away_on_close_(false),
      close_on_deactivate_(true),
#if defined(TOOLKIT_USES_GTK)
      type_(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS),
#endif
#if defined(OS_CHROMEOS)
      show_while_screen_is_locked_(false),
#endif
      arrow_location_(views::BubbleBorder::NONE),
      contents_(NULL),
      accelerator_registered_(false) {
}

#if defined(OS_CHROMEOS)
Bubble::Bubble(views::Widget::InitParams::Type type,
               bool show_while_screen_is_locked)
#if defined(USE_AURA)
    : views::NativeWidgetAura(new views::Widget),
#elif defined(TOUCH_UI)
    : views::NativeWidgetViews(new views::Widget),
#else
    : views::NativeWidgetGtk(new views::Widget),
#endif
      border_contents_(NULL),
      delegate_(NULL),
      show_status_(kOpen),
      fade_away_on_close_(false),
#if defined(TOOLKIT_USES_GTK)
      type_(type),
#endif
      show_while_screen_is_locked_(show_while_screen_is_locked),
      arrow_location_(views::BubbleBorder::NONE),
      contents_(NULL),
      accelerator_registered_(false) {
}
#endif

Bubble::~Bubble() {
}

void Bubble::InitBubble(views::Widget* parent,
                        const gfx::Rect& position_relative_to,
                        views::BubbleBorder::ArrowLocation arrow_location,
                        views::BubbleBorder::BubbleAlignment alignment,
                        views::View* contents,
                        BubbleDelegate* delegate) {
  delegate_ = delegate;
  position_relative_to_ = position_relative_to;
  arrow_location_ = arrow_location;
  contents_ = contents;
  const bool fade_in = delegate_ && delegate_->FadeInOnShow();

  // Create the main window.
#if defined(USE_AURA)
  views::Widget* parent_window = parent->GetTopLevelWidget();
  if (parent_window)
    parent_window->DisableInactiveRendering();
  views::Widget::InitParams params;
  params.transparent = true;
  params.parent_widget = parent;
  params.native_widget = this;
  GetWidget()->Init(params);
  if (fade_in)
    SetOpacity(0);
#elif defined(OS_WIN)
  views::Widget* parent_window = parent->GetTopLevelWidget();
  if (parent_window)
    parent_window->DisableInactiveRendering();
  set_window_style(WS_POPUP | WS_CLIPCHILDREN);
  int extended_style = WS_EX_TOOLWINDOW;
  // During FadeIn we need to turn on the layered window style to deal with
  // transparency. This flag needs to be reset after fading in is complete.
  if (fade_in)
    extended_style |= WS_EX_LAYERED;
  set_window_ex_style(extended_style);

  DCHECK(!border_);
  border_ = new BorderWidgetWin();

  border_->InitBorderWidgetWin(CreateBorderContents(), parent->GetNativeView());
  border_->border_contents()->SetBackgroundColor(kBackgroundColor);
  border_->border_contents()->SetAlignment(alignment);

  // We make the BorderWidgetWin the owner of the Bubble HWND, so that the
  // latter is displayed on top of the former.
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.parent = border_->GetNativeView();
  params.native_widget = this;
  GetWidget()->Init(params);

  if (fade_in) {
    border_->SetOpacity(0);
    GetWidget()->SetOpacity(0);
  }
  SetWindowText(GetNativeView(), delegate_->GetAccessibleName().c_str());
#elif defined(TOOLKIT_USES_GTK)
  views::Widget::InitParams params(type_);
  params.transparent = true;
  params.parent_widget = parent;
  params.native_widget = this;
  GetWidget()->Init(params);
  if (fade_in)
    SetOpacity(0);
#if defined(OS_CHROMEOS) && defined(TOOLKIT_USES_GTK)
  {
    vector<int> params;
    params.push_back(show_while_screen_is_locked_ ? 1 : 0);
    chromeos::WmIpc::instance()->SetWindowType(
        GetNativeView(),
        chromeos::WM_IPC_WINDOW_CHROME_INFO_BUBBLE,
        &params);
  }
#endif
#endif

  // Create a View to hold the contents of the main window.
  views::View* contents_view = new views::View;
  // We add |contents_view| to ourselves before the AddChildView() call below so
  // that when |contents| gets added, it will already have a widget, and thus
  // any NativeButtons it creates in ViewHierarchyChanged() will be functional
  // (e.g. calling SetChecked() on checkboxes is safe).
  GetWidget()->SetContentsView(contents_view);
  // Adding |contents| as a child has to be done before we call
  // contents->GetPreferredSize() below, since some supplied views don't
  // actually initialize themselves until they're added to a hierarchy.
  contents_view->AddChildView(contents);

  // Calculate and set the bounds for all windows and views.
  gfx::Rect window_bounds;

#if defined(OS_WIN) && !defined(USE_AURA)
  // Initialize and position the border window.
  window_bounds = border_->SizeAndGetBounds(position_relative_to,
                                            arrow_location,
                                            contents->GetPreferredSize());

  // Make |contents| take up the entire contents view.
  contents_view->SetLayoutManager(new views::FillLayout);

  // Paint the background color behind the contents.
  contents_view->set_background(
      views::Background::CreateSolidBackground(kBackgroundColor));
#else
  // Create a view to paint the border and background.
  border_contents_ = CreateBorderContents();
  border_contents_->Init();
  border_contents_->SetBackgroundColor(kBackgroundColor);
  border_contents_->SetAlignment(alignment);
  gfx::Rect contents_bounds;
  border_contents_->SizeAndGetBounds(position_relative_to,
      arrow_location, false, contents->GetPreferredSize(),
      &contents_bounds, &window_bounds);
  // This new view must be added before |contents| so it will paint under it.
  contents_view->AddChildViewAt(border_contents_, 0);

  // |contents_view| has no layout manager, so we have to explicitly position
  // its children.
  border_contents_->SetBoundsRect(
      gfx::Rect(gfx::Point(), window_bounds.size()));
  contents->SetBoundsRect(contents_bounds);
#endif
  GetWidget()->SetBounds(window_bounds);

  // Show the window.
#if defined(USE_AURA)
  GetWidget()->Show();
#elif defined(OS_WIN)
  border_->ShowWindow(SW_SHOW);
  ShowWindow(SW_SHOW);
#elif defined(TOOLKIT_USES_GTK)
  GetWidget()->Show();
#endif

  if (fade_in)
    FadeIn();
}

void Bubble::RegisterEscapeAccelerator() {
  GetWidget()->GetFocusManager()->RegisterAccelerator(
      ui::Accelerator(ui::VKEY_ESCAPE, false, false, false), this);
  accelerator_registered_ = true;
}

void Bubble::UnregisterEscapeAccelerator() {
  DCHECK(accelerator_registered_);
  GetWidget()->GetFocusManager()->UnregisterAccelerator(
      ui::Accelerator(ui::VKEY_ESCAPE, false, false, false), this);
  accelerator_registered_ = false;
}

BorderContents* Bubble::CreateBorderContents() {
  return new BorderContents();
}

void Bubble::SizeToContents() {
  gfx::Rect window_bounds;

#if defined(OS_WIN) && !defined(USE_AURA)
  // Initialize and position the border window.
  window_bounds = border_->SizeAndGetBounds(position_relative_to_,
                                            arrow_location_,
                                            contents_->GetPreferredSize());
#else
  gfx::Rect contents_bounds;
  border_contents_->SizeAndGetBounds(position_relative_to_,
      arrow_location_, false, contents_->GetPreferredSize(),
      &contents_bounds, &window_bounds);
  // |contents_view| has no layout manager, so we have to explicitly position
  // its children.
  border_contents_->SetBoundsRect(
      gfx::Rect(gfx::Point(), window_bounds.size()));
  contents_->SetBoundsRect(contents_bounds);
#endif
  GetWidget()->SetBounds(window_bounds);
}

#if defined(USE_AURA)
void Bubble::OnLostActive() {
  GetWidget()->Close();
}
#elif defined(OS_WIN)
void Bubble::OnActivate(UINT action, BOOL minimized, HWND window) {
  // The popup should close when it is deactivated.
  if (action == WA_INACTIVE) {
    if (close_on_deactivate_)
      GetWidget()->Close();
  } else if (action == WA_ACTIVE) {
    DCHECK(GetWidget()->GetRootView()->has_children());
    GetWidget()->GetRootView()->child_at(0)->RequestFocus();
  }
}
#elif defined(TOUCH_UI)
void Bubble::Deactivate() {
  GetWidget()->Close();
}
#elif defined(TOOLKIT_USES_GTK)
void Bubble::OnActiveChanged() {
  if (!GetWidget()->IsActive())
    GetWidget()->Close();
}
#endif

void Bubble::DoClose(bool closed_by_escape) {
  if (show_status_ == kClosed)
    return;

  if (accelerator_registered_)
    UnregisterEscapeAccelerator();
  if (delegate_)
    delegate_->BubbleClosing(this, closed_by_escape);
  FOR_EACH_OBSERVER(Observer, observer_list_, OnBubbleClosing());
  show_status_ = kClosed;
#if defined(OS_WIN) && !defined(USE_AURA)
  border_->Close();
#endif
#if defined(USE_AURA)
  NativeWidgetAura::Close();
#elif defined(OS_WIN)
  NativeWidgetWin::Close();
#elif defined(TOUCH_UI)
  NativeWidgetViews::Close();
#elif defined(TOOLKIT_USES_GTK)
  NativeWidgetGtk::Close();
#endif
}

void Bubble::FadeIn() {
  Fade(true);  // |fade_in|.
}

void Bubble::FadeOut() {
#if defined(OS_WIN) && !defined(USE_AURA)
  // The contents window cannot have the layered flag on by default, since its
  // content doesn't always work inside a layered window, but when animating it
  // is ok to set that style on the window for the purpose of fading it out.
  SetWindowLong(GWL_EXSTYLE, GetWindowLong(GWL_EXSTYLE) | WS_EX_LAYERED);
  // This must be the very next call, otherwise we can get flicker on close.
  SetLayeredWindowAttributes(GetNativeView(), 0,
      static_cast<byte>(255), LWA_ALPHA);
#elif defined(USE_AURA)
  NOTIMPLEMENTED();
#endif

  Fade(false);  // |fade_in|.
}

void Bubble::Fade(bool fade_in) {
  animation_.reset(new ui::SlideAnimation(this));
  animation_->SetSlideDuration(kHideFadeDurationMS);
  animation_->SetTweenType(ui::Tween::LINEAR);

  animation_->Reset(fade_in ? 0.0 : 1.0);
  if (fade_in)
    animation_->Show();
  else
    animation_->Hide();
}

bool Bubble::AcceleratorPressed(const ui::Accelerator& accelerator) {
  if (!delegate_ || delegate_->CloseOnEscape()) {
    DoClose(true);
    return true;
  }
  return false;
}
