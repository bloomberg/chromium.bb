// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bubble/bubble.h"

#include <vector>

#include "chrome/browser/ui/views/bubble/border_contents.h"
#include "content/common/notification_service.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/gfx/color_utils.h"
#include "views/layout/fill_layout.h"
#include "views/widget/root_view.h"
#include "views/widget/widget.h"
#include "views/window/client_view.h"
#include "views/window/window.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/wm_ipc.h"
#include "third_party/cros/chromeos_wm_ipc_enums.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/ui/views/bubble/border_widget_win.h"
#endif

using std::vector;

// How long the fade should last for.
static const int kHideFadeDurationMS = 200;

// Background color of the bubble.
#if defined(OS_WIN)
const SkColor Bubble::kBackgroundColor =
    color_utils::GetSysSkColor(COLOR_WINDOW);
#else
// TODO(beng): source from theme provider.
const SkColor Bubble::kBackgroundColor = SK_ColorWHITE;
#endif

// BubbleDelegate ---------------------------------------------------------

std::wstring BubbleDelegate::accessible_name() {
  return L"";
}

// Bubble -----------------------------------------------------------------

// static
Bubble* Bubble::Show(views::Widget* parent,
                     const gfx::Rect& position_relative_to,
                     BubbleBorder::ArrowLocation arrow_location,
                     views::View* contents,
                     BubbleDelegate* delegate) {
  Bubble* bubble = new Bubble;
  bubble->InitBubble(parent, position_relative_to, arrow_location,
                     contents, delegate);
  return bubble;
}

#if defined(OS_CHROMEOS)
// static
Bubble* Bubble::ShowFocusless(
    views::Widget* parent,
    const gfx::Rect& position_relative_to,
    BubbleBorder::ArrowLocation arrow_location,
    views::View* contents,
    BubbleDelegate* delegate,
    bool show_while_screen_is_locked) {
  Bubble* bubble = new Bubble(views::WidgetGtk::TYPE_POPUP,
                              show_while_screen_is_locked);
  bubble->InitBubble(parent, position_relative_to, arrow_location,
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
#if defined(OS_WIN)
    // When fading in we need to remove the layered window style flag, since
    // that style prevents some bubble content from working properly.
    SetWindowLong(GWL_EXSTYLE, GetWindowLong(GWL_EXSTYLE) & ~WS_EX_LAYERED);
#endif
  }
}

void Bubble::AnimationProgressed(const ui::Animation* animation) {
#if defined(OS_WIN)
  // Set the opacity for the main contents window.
  unsigned char opacity = static_cast<unsigned char>(
      animation_->GetCurrentValue() * 255);
  SetLayeredWindowAttributes(GetNativeView(), 0,
      static_cast<byte>(opacity), LWA_ALPHA);
  contents_->SchedulePaint();

  // Also fade in/out the bubble border window.
  border_->SetOpacity(opacity);
  border_->border_contents()->SchedulePaint();
#else
  NOTIMPLEMENTED();
#endif
}

Bubble::Bubble()
    :
#if defined(OS_LINUX)
      WidgetGtk(TYPE_WINDOW),
      border_contents_(NULL),
#elif defined(OS_WIN)
      border_(NULL),
#endif
      delegate_(NULL),
      show_status_(kOpen),
      fade_away_on_close_(false),
#if defined(OS_CHROMEOS)
      show_while_screen_is_locked_(false),
#endif
      arrow_location_(BubbleBorder::NONE),
      contents_(NULL) {
}

#if defined(OS_CHROMEOS)
Bubble::Bubble(views::WidgetGtk::Type type, bool show_while_screen_is_locked)
    : WidgetGtk(type),
      border_contents_(NULL),
      delegate_(NULL),
      show_status_(kOpen),
      fade_away_on_close_(false),
      show_while_screen_is_locked_(show_while_screen_is_locked),
      arrow_location_(BubbleBorder::NONE),
      contents_(NULL) {
}
#endif

Bubble::~Bubble() {
}

void Bubble::InitBubble(views::Widget* parent,
                        const gfx::Rect& position_relative_to,
                        BubbleBorder::ArrowLocation arrow_location,
                        views::View* contents,
                        BubbleDelegate* delegate) {
  delegate_ = delegate;
  position_relative_to_ = position_relative_to;
  arrow_location_ = arrow_location;
  contents_ = contents;

  // Create the main window.
#if defined(OS_WIN)
  views::Window* parent_window = parent->GetWindow();
  if (parent_window)
    parent_window->DisableInactiveRendering();
  set_window_style(WS_POPUP | WS_CLIPCHILDREN);
  int extended_style = WS_EX_TOOLWINDOW;
  // During FadeIn we need to turn on the layered window style to deal with
  // transparency. This flag needs to be reset after fading in is complete.
  bool fade_in = delegate_ && delegate_->FadeInOnShow();
  if (fade_in)
    extended_style |= WS_EX_LAYERED;
  set_window_ex_style(extended_style);

  DCHECK(!border_);
  border_ = new BorderWidgetWin();

  if (fade_in) {
    border_->SetOpacity(0);
    SetOpacity(0);
  }

  border_->Init(CreateBorderContents(), parent->GetNativeView());
  border_->border_contents()->SetBackgroundColor(kBackgroundColor);

  // We make the BorderWidgetWin the owner of the Bubble HWND, so that the
  // latter is displayed on top of the former.
  WidgetWin::Init(border_->GetNativeView(), gfx::Rect());

  SetWindowText(GetNativeView(), delegate_->accessible_name().c_str());
#elif defined(OS_LINUX)
  MakeTransparent();
  make_transient_to_parent();
  WidgetGtk::InitWithWidget(parent, gfx::Rect());
#if defined(OS_CHROMEOS)
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
  SetContentsView(contents_view);
  // Adding |contents| as a child has to be done before we call
  // contents->GetPreferredSize() below, since some supplied views don't
  // actually initialize themselves until they're added to a hierarchy.
  contents_view->AddChildView(contents);

  // Calculate and set the bounds for all windows and views.
  gfx::Rect window_bounds;

#if defined(OS_WIN)
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
  SetBounds(window_bounds);

  // Register the Escape accelerator for closing.
  GetFocusManager()->RegisterAccelerator(
      views::Accelerator(ui::VKEY_ESCAPE, false, false, false), this);

  // Done creating the bubble.
  NotificationService::current()->Notify(NotificationType::INFO_BUBBLE_CREATED,
                                         Source<Bubble>(this),
                                         NotificationService::NoDetails());

  // Show the window.
#if defined(OS_WIN)
  border_->ShowWindow(SW_SHOW);
  ShowWindow(SW_SHOW);
  if (fade_in)
    FadeIn();
#elif defined(OS_LINUX)
  views::WidgetGtk::Show();
#endif
}

BorderContents* Bubble::CreateBorderContents() {
  return new BorderContents();
}

void Bubble::SizeToContents() {
  gfx::Rect window_bounds;

#if defined(OS_WIN)
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
  SetBounds(window_bounds);
}

#if defined(OS_WIN)
void Bubble::OnActivate(UINT action, BOOL minimized, HWND window) {
  // The popup should close when it is deactivated.
  if (action == WA_INACTIVE) {
    Close();
  } else if (action == WA_ACTIVE) {
    DCHECK(GetRootView()->has_children());
    GetRootView()->GetChildViewAt(0)->RequestFocus();
  }
}
#elif defined(OS_LINUX)
void Bubble::IsActiveChanged() {
  if (!IsActive())
    Close();
}
#endif

void Bubble::DoClose(bool closed_by_escape) {
  if (show_status_ == kClosed)
    return;

  GetFocusManager()->UnregisterAccelerator(
      views::Accelerator(ui::VKEY_ESCAPE, false, false, false), this);
  if (delegate_)
    delegate_->BubbleClosing(this, closed_by_escape);
  show_status_ = kClosed;
#if defined(OS_WIN)
  border_->Close();
  WidgetWin::Close();
#elif defined(OS_LINUX)
  WidgetGtk::Close();
#endif
}

void Bubble::FadeIn() {
  Fade(true);  // |fade_in|.
}

void Bubble::FadeOut() {
#if defined(OS_WIN)
  // The contents window cannot have the layered flag on by default, since its
  // content doesn't always work inside a layered window, but when animating it
  // is ok to set that style on the window for the purpose of fading it out.
  SetWindowLong(GWL_EXSTYLE, GetWindowLong(GWL_EXSTYLE) | WS_EX_LAYERED);
  // This must be the very next call, otherwise we can get flicker on close.
  SetLayeredWindowAttributes(GetNativeView(), 0,
      static_cast<byte>(255), LWA_ALPHA);
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

bool Bubble::AcceleratorPressed(const views::Accelerator& accelerator) {
  if (!delegate_ || delegate_->CloseOnEscape()) {
    DoClose(true);
    return true;
  }
  return false;
}
