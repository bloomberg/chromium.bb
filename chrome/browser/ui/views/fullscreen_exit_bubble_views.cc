// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/fullscreen_exit_bubble_views.h"

#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/fullscreen/fullscreen_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "content/public/browser/notification_service.h"
#include "grit/generated_resources.h"
#include "grit/ui_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/screen.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "ui/base/l10n/l10n_util_win.h"
#endif

// FullscreenExitView ----------------------------------------------------------

namespace {

// Space between the site info label and the buttons / link.
const int kMiddlePaddingPx = 30;

class ButtonView : public views::View {
 public:
  ButtonView(views::ButtonListener* listener, int between_button_spacing);
  virtual ~ButtonView();

  // Returns an empty size when the view is not visible.
  virtual gfx::Size GetPreferredSize() const OVERRIDE;

  views::LabelButton* accept_button() const { return accept_button_; }
  views::LabelButton* deny_button() const { return deny_button_; }

 private:
  views::LabelButton* accept_button_;
  views::LabelButton* deny_button_;

  DISALLOW_COPY_AND_ASSIGN(ButtonView);
};

ButtonView::ButtonView(views::ButtonListener* listener,
                       int between_button_spacing)
    : accept_button_(NULL),
      deny_button_(NULL) {
  accept_button_ = new views::LabelButton(listener, base::string16());
  accept_button_->SetStyle(views::Button::STYLE_BUTTON);
  accept_button_->SetFocusable(false);
  AddChildView(accept_button_);

  deny_button_ = new views::LabelButton(listener, base::string16());
  deny_button_->SetStyle(views::Button::STYLE_BUTTON);
  deny_button_->SetFocusable(false);
  AddChildView(deny_button_);

  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0,
                                        between_button_spacing));
}

ButtonView::~ButtonView() {
}

gfx::Size ButtonView::GetPreferredSize() const {
  return visible() ? views::View::GetPreferredSize() : gfx::Size();
}

}  // namespace

class FullscreenExitBubbleViews::FullscreenExitView
    : public views::View,
      public views::ButtonListener,
      public views::LinkListener {
 public:
  FullscreenExitView(FullscreenExitBubbleViews* bubble,
                     const base::string16& accelerator,
                     const GURL& url,
                     FullscreenExitBubbleType bubble_type);
  virtual ~FullscreenExitView();

  // views::ButtonListener
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // views::LinkListener
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

  void UpdateContent(const GURL& url, FullscreenExitBubbleType bubble_type);

 private:
  FullscreenExitBubbleViews* bubble_;

  // Clickable hint text for exiting fullscreen mode.
  views::Link* link_;
  // Instruction for exiting mouse lock.
  views::Label* mouse_lock_exit_instruction_;
  // Informational label: 'www.foo.com has gone fullscreen'.
  views::Label* message_label_;
  ButtonView* button_view_;
  const base::string16 browser_fullscreen_exit_accelerator_;

  DISALLOW_COPY_AND_ASSIGN(FullscreenExitView);
};

FullscreenExitBubbleViews::FullscreenExitView::FullscreenExitView(
    FullscreenExitBubbleViews* bubble,
    const base::string16& accelerator,
    const GURL& url,
    FullscreenExitBubbleType bubble_type)
    : bubble_(bubble),
      link_(NULL),
      mouse_lock_exit_instruction_(NULL),
      message_label_(NULL),
      button_view_(NULL),
      browser_fullscreen_exit_accelerator_(accelerator) {
  scoped_ptr<views::BubbleBorder> bubble_border(
      new views::BubbleBorder(views::BubbleBorder::NONE,
                              views::BubbleBorder::BIG_SHADOW,
                              SK_ColorWHITE));
  set_background(new views::BubbleBackground(bubble_border.get()));
  SetBorder(bubble_border.PassAs<views::Border>());
  SetFocusable(false);

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  const gfx::FontList& medium_font_list =
      rb.GetFontList(ui::ResourceBundle::MediumFont);
  message_label_ = new views::Label(base::string16(), medium_font_list);

  mouse_lock_exit_instruction_ =
      new views::Label(bubble_->GetInstructionText(), medium_font_list);
  mouse_lock_exit_instruction_->set_collapse_when_hidden(true);

  link_ = new views::Link();
  link_->set_collapse_when_hidden(true);
  link_->SetFocusable(false);
#if defined(OS_CHROMEOS)
  // On CrOS, the link text doesn't change, since it doesn't show the shortcut.
  link_->SetText(l10n_util::GetStringUTF16(IDS_EXIT_FULLSCREEN_MODE));
#endif
  link_->set_listener(this);
  link_->SetFontList(medium_font_list);
  link_->SetPressedColor(message_label_->enabled_color());
  link_->SetEnabledColor(message_label_->enabled_color());
  link_->SetVisible(false);

  link_->SetBackgroundColor(background()->get_color());
  message_label_->SetBackgroundColor(background()->get_color());
  mouse_lock_exit_instruction_->SetBackgroundColor(background()->get_color());

  button_view_ = new ButtonView(this, kPaddingPx);
  button_view_->accept_button()->SetText(bubble->GetAllowButtonText());

  views::GridLayout* layout = new views::GridLayout(this);
  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER, 0,
                     views::GridLayout::USE_PREF, 0, 0);
  columns->AddPaddingColumn(1, kMiddlePaddingPx);
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER, 0,
                     views::GridLayout::USE_PREF, 0, 0);
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER, 0,
                     views::GridLayout::USE_PREF, 0, 0);
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER, 0,
                     views::GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, 0);
  layout->AddView(message_label_);
  layout->AddView(button_view_);
  layout->AddView(mouse_lock_exit_instruction_);
  layout->AddView(link_);

  gfx::Insets padding(kPaddingPx, kPaddingPx, kPaddingPx, kPaddingPx);
  padding += GetInsets();
  layout->SetInsets(padding);
  SetLayoutManager(layout);

  UpdateContent(url, bubble_type);
}

FullscreenExitBubbleViews::FullscreenExitView::~FullscreenExitView() {
}

void FullscreenExitBubbleViews::FullscreenExitView::ButtonPressed(
    views::Button* sender,
    const ui::Event& event) {
  if (sender == button_view_->accept_button())
    bubble_->Accept();
  else
    bubble_->Cancel();
}

void FullscreenExitBubbleViews::FullscreenExitView::LinkClicked(
    views::Link* link,
    int event_flags) {
  bubble_->ToggleFullscreen();
}

void FullscreenExitBubbleViews::FullscreenExitView::UpdateContent(
    const GURL& url,
    FullscreenExitBubbleType bubble_type) {
  DCHECK_NE(FEB_TYPE_NONE, bubble_type);

  message_label_->SetText(bubble_->GetCurrentMessageText());
  if (fullscreen_bubble::ShowButtonsForType(bubble_type)) {
    link_->SetVisible(false);
    mouse_lock_exit_instruction_->SetVisible(false);
    button_view_->SetVisible(true);
    button_view_->deny_button()->SetText(bubble_->GetCurrentDenyButtonText());
    button_view_->deny_button()->set_min_size(gfx::Size());
  } else {
    bool link_visible = true;
    base::string16 accelerator;
    if (bubble_type == FEB_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION ||
        bubble_type == FEB_TYPE_BROWSER_EXTENSION_FULLSCREEN_EXIT_INSTRUCTION) {
      accelerator = browser_fullscreen_exit_accelerator_;
    } else if (bubble_type == FEB_TYPE_FULLSCREEN_EXIT_INSTRUCTION) {
      accelerator = l10n_util::GetStringUTF16(IDS_APP_ESC_KEY);
    } else {
      link_visible = false;
    }
#if !defined(OS_CHROMEOS)
    if (link_visible) {
      link_->SetText(
          l10n_util::GetStringUTF16(IDS_EXIT_FULLSCREEN_MODE) +
          base::UTF8ToUTF16(" ") +
          l10n_util::GetStringFUTF16(IDS_EXIT_FULLSCREEN_MODE_ACCELERATOR,
              accelerator));
    }
#endif
    link_->SetVisible(link_visible);
    mouse_lock_exit_instruction_->SetVisible(!link_visible);
    button_view_->SetVisible(false);
  }
}


// FullscreenExitBubbleViews ---------------------------------------------------

FullscreenExitBubbleViews::FullscreenExitBubbleViews(
    BrowserView* browser_view,
    const GURL& url,
    FullscreenExitBubbleType bubble_type)
    : FullscreenExitBubble(browser_view->browser(), url, bubble_type),
      browser_view_(browser_view),
      popup_(NULL),
      animation_(new gfx::SlideAnimation(this)),
      animated_attribute_(ANIMATED_ATTRIBUTE_BOUNDS) {
  animation_->Reset(1);

  // Create the contents view.
  ui::Accelerator accelerator(ui::VKEY_UNKNOWN, ui::EF_NONE);
  bool got_accelerator = browser_view_->GetWidget()->GetAccelerator(
      IDC_FULLSCREEN, &accelerator);
  DCHECK(got_accelerator);
  view_ = new FullscreenExitView(
      this, accelerator.GetShortcutText(), url, bubble_type_);

  // TODO(yzshen): Change to use the new views bubble, BubbleDelegateView.
  // TODO(pkotwicz): When this becomes a views bubble, make sure that this
  // bubble is ignored by ImmersiveModeControllerAsh::BubbleManager.
  // Initialize the popup.
  popup_ = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = browser_view_->GetWidget()->GetNativeView();
  params.bounds = GetPopupRect(false);
  popup_->Init(params);
  gfx::Size size = GetPopupRect(true).size();
  popup_->SetContentsView(view_);
  // We set layout manager to NULL to prevent the widget from sizing its
  // contents to the same size as itself. This prevents the widget contents from
  // shrinking while we animate the height of the popup to give the impression
  // that it is sliding off the top of the screen.
  popup_->GetRootView()->SetLayoutManager(NULL);
  view_->SetBounds(0, 0, size.width(), size.height());
  popup_->ShowInactive();  // This does not activate the popup.

  popup_->AddObserver(this);

  registrar_.Add(
      this,
      chrome::NOTIFICATION_FULLSCREEN_CHANGED,
      content::Source<FullscreenController>(
          browser_view_->browser()->fullscreen_controller()));

  UpdateForImmersiveState();
}

FullscreenExitBubbleViews::~FullscreenExitBubbleViews() {
  popup_->RemoveObserver(this);

  // This is tricky.  We may be in an ATL message handler stack, in which case
  // the popup cannot be deleted yet.  We also can't set the popup's ownership
  // model to NATIVE_WIDGET_OWNS_WIDGET because if the user closed the last tab
  // while in fullscreen mode, Windows has already destroyed the popup HWND by
  // the time we get here, and thus either the popup will already have been
  // deleted (if we set this in our constructor) or the popup will never get
  // another OnFinalMessage() call (if not, as currently).  So instead, we tell
  // the popup to synchronously hide, and then asynchronously close and delete
  // itself.
  popup_->Close();
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, popup_);
}

void FullscreenExitBubbleViews::UpdateContent(
    const GURL& url,
    FullscreenExitBubbleType bubble_type) {
  DCHECK_NE(FEB_TYPE_NONE, bubble_type);
  if (bubble_type_ == bubble_type && url_ == url)
    return;

  url_ = url;
  bubble_type_ = bubble_type;
  view_->UpdateContent(url_, bubble_type_);

  gfx::Size size = GetPopupRect(true).size();
  view_->SetSize(size);
  popup_->SetBounds(GetPopupRect(false));
  Show();

  // Stop watching the mouse even if UpdateMouseWatcher() will start watching
  // it again so that the popup with the new content is visible for at least
  // |kInitialDelayMs|.
  StopWatchingMouse();

  UpdateMouseWatcher();
}

void FullscreenExitBubbleViews::RepositionIfVisible() {
  if (popup_->IsVisible())
    UpdateBounds();
}

void FullscreenExitBubbleViews::UpdateMouseWatcher() {
  bool should_watch_mouse = false;
  if (popup_->IsVisible())
    should_watch_mouse = !fullscreen_bubble::ShowButtonsForType(bubble_type_);
  else
    should_watch_mouse = CanMouseTriggerSlideIn();

  if (should_watch_mouse == IsWatchingMouse())
    return;

  if (should_watch_mouse)
    StartWatchingMouse();
  else
    StopWatchingMouse();
}

void FullscreenExitBubbleViews::UpdateForImmersiveState() {
  AnimatedAttribute expected_animated_attribute =
      browser_view_->immersive_mode_controller()->IsEnabled() ?
          ANIMATED_ATTRIBUTE_OPACITY : ANIMATED_ATTRIBUTE_BOUNDS;
  if (animated_attribute_ != expected_animated_attribute) {
    // If an animation is currently in progress, skip to the end because
    // switching the animated attribute midway through the animation looks
    // weird.
    animation_->End();

    animated_attribute_ = expected_animated_attribute;

    // We may have finished hiding |popup_|. However, the bounds animation
    // assumes |popup_| has the opacity when it is fully shown and the opacity
    // animation assumes |popup_| has the bounds when |popup_| is fully shown.
    if (animated_attribute_ == ANIMATED_ATTRIBUTE_BOUNDS)
      popup_->SetOpacity(255);
    else
      UpdateBounds();
  }

  UpdateMouseWatcher();
}

void FullscreenExitBubbleViews::UpdateBounds() {
  gfx::Rect popup_rect(GetPopupRect(false));
  if (!popup_rect.IsEmpty()) {
    popup_->SetBounds(popup_rect);
    view_->SetY(popup_rect.height() - view_->height());
  }
}

views::View* FullscreenExitBubbleViews::GetBrowserRootView() const {
  return browser_view_->GetWidget()->GetRootView();
}

void FullscreenExitBubbleViews::AnimationProgressed(
    const gfx::Animation* animation) {
  if (animated_attribute_ == ANIMATED_ATTRIBUTE_OPACITY) {
    int opacity = animation_->CurrentValueBetween(0, 255);
    if (opacity == 0) {
      popup_->Hide();
    } else {
      popup_->Show();
      popup_->SetOpacity(opacity);
    }
  } else {
    if (GetPopupRect(false).IsEmpty()) {
      popup_->Hide();
    } else {
      UpdateBounds();
      popup_->Show();
    }
  }
}

void FullscreenExitBubbleViews::AnimationEnded(
    const gfx::Animation* animation) {
  AnimationProgressed(animation);
}

gfx::Rect FullscreenExitBubbleViews::GetPopupRect(
    bool ignore_animation_state) const {
  gfx::Size size(view_->GetPreferredSize());
  // NOTE: don't use the bounds of the root_view_. On linux GTK changing window
  // size is async. Instead we use the size of the screen.
  gfx::Screen* screen =
      gfx::Screen::GetScreenFor(browser_view_->GetWidget()->GetNativeView());
  gfx::Rect screen_bounds = screen->GetDisplayNearestWindow(
      browser_view_->GetWidget()->GetNativeView()).bounds();
  int x = screen_bounds.x() + (screen_bounds.width() - size.width()) / 2;

  int top_container_bottom = screen_bounds.y();
  if (browser_view_->immersive_mode_controller()->IsEnabled()) {
    // Skip querying the top container height in non-immersive fullscreen
    // because:
    // - The top container height is always zero in non-immersive fullscreen.
    // - Querying the top container height may return the height before entering
    //   fullscreen because layout is disabled while entering fullscreen.
    // A visual glitch due to the delayed layout is avoided in immersive
    // fullscreen because entering fullscreen starts with the top container
    // revealed. When revealed, the top container has the same height as before
    // entering fullscreen.
    top_container_bottom =
        browser_view_->top_container()->GetBoundsInScreen().bottom();
  }
  int y = top_container_bottom + kPopupTopPx;

  if (!ignore_animation_state &&
      animated_attribute_ == ANIMATED_ATTRIBUTE_BOUNDS) {
    int total_height = size.height() + kPopupTopPx;
    int popup_bottom = animation_->CurrentValueBetween(total_height, 0);
    int y_offset = std::min(popup_bottom, kPopupTopPx);
    size.set_height(size.height() - popup_bottom + y_offset);
    y -= y_offset;
  }
  return gfx::Rect(gfx::Point(x, y), size);
}

gfx::Point FullscreenExitBubbleViews::GetCursorScreenPoint() {
  gfx::Point cursor_pos = gfx::Screen::GetScreenFor(
      browser_view_->GetWidget()->GetNativeView())->GetCursorScreenPoint();
  views::View::ConvertPointFromScreen(GetBrowserRootView(), &cursor_pos);
  return cursor_pos;
}

bool FullscreenExitBubbleViews::WindowContainsPoint(gfx::Point pos) {
  return GetBrowserRootView()->HitTestPoint(pos);
}

bool FullscreenExitBubbleViews::IsWindowActive() {
  return browser_view_->GetWidget()->IsActive();
}

void FullscreenExitBubbleViews::Hide() {
  animation_->SetSlideDuration(kSlideOutDurationMs);
  animation_->Hide();
}

void FullscreenExitBubbleViews::Show() {
  animation_->SetSlideDuration(kSlideInDurationMs);
  animation_->Show();
}

bool FullscreenExitBubbleViews::IsAnimating() {
  return animation_->is_animating();
}

bool FullscreenExitBubbleViews::CanMouseTriggerSlideIn() const {
  return !browser_view_->immersive_mode_controller()->IsEnabled();
}

void FullscreenExitBubbleViews::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_FULLSCREEN_CHANGED, type);
  UpdateForImmersiveState();
}

void FullscreenExitBubbleViews::OnWidgetVisibilityChanged(
    views::Widget* widget,
    bool visible) {
  UpdateMouseWatcher();
}
