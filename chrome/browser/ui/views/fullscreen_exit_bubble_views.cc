// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/fullscreen_exit_bubble_views.h"

#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "grit/ui_strings.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/screen.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

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
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  views::NativeTextButton* accept_button() const { return accept_button_; }
  views::NativeTextButton* deny_button() const { return deny_button_; }

 private:
  views::NativeTextButton* accept_button_;
  views::NativeTextButton* deny_button_;
};

ButtonView::ButtonView(views::ButtonListener* listener,
                       int between_button_spacing) : accept_button_(NULL),
                                                     deny_button_(NULL) {
  accept_button_ = new views::NativeTextButton(listener);
  accept_button_->set_focusable(false);
  AddChildView(accept_button_);

  deny_button_ = new views::NativeTextButton(listener);
  deny_button_->set_focusable(false);
  AddChildView(deny_button_);

  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0,
                                        between_button_spacing));
}

ButtonView::~ButtonView() {
}

gfx::Size ButtonView::GetPreferredSize() {
  return visible() ? views::View::GetPreferredSize() : gfx::Size();
}

}  // namespace

class FullscreenExitBubbleViews::FullscreenExitView
    : public views::View,
      public views::ButtonListener,
      public views::LinkListener {
 public:
  FullscreenExitView(FullscreenExitBubbleViews* bubble,
                     const string16& accelerator,
                     const GURL& url,
                     FullscreenExitBubbleType bubble_type);
  virtual ~FullscreenExitView();

  // views::ButtonListener
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

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
  const string16 browser_fullscreen_exit_accelerator_;

  DISALLOW_COPY_AND_ASSIGN(FullscreenExitView);
};

FullscreenExitBubbleViews::FullscreenExitView::FullscreenExitView(
    FullscreenExitBubbleViews* bubble,
    const string16& accelerator,
    const GURL& url,
    FullscreenExitBubbleType bubble_type)
    : bubble_(bubble),
      link_(NULL),
      mouse_lock_exit_instruction_(NULL),
      message_label_(NULL),
      button_view_(NULL),
      browser_fullscreen_exit_accelerator_(accelerator) {
  views::BubbleBorder* bubble_border =
      new views::BubbleBorder(views::BubbleBorder::NONE,
                              views::BubbleBorder::SHADOW);
  set_background(new views::BubbleBackground(bubble_border));
  set_border(bubble_border);
  set_focusable(false);

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  message_label_ = new views::Label();
  message_label_->SetFont(rb.GetFont(ui::ResourceBundle::MediumFont));

  mouse_lock_exit_instruction_ = new views::Label();
  mouse_lock_exit_instruction_->set_collapse_when_hidden(true);
  mouse_lock_exit_instruction_->SetText(bubble_->GetInstructionText());
  mouse_lock_exit_instruction_->SetFont(
      rb.GetFont(ui::ResourceBundle::MediumFont));

  link_ = new views::Link();
  link_->set_collapse_when_hidden(true);
  link_->set_focusable(false);
#if defined(OS_CHROMEOS)
  // On CrOS, the link text doesn't change, since it doesn't show the shortcut.
  link_->SetText(l10n_util::GetStringUTF16(IDS_EXIT_FULLSCREEN_MODE));
#endif
  link_->set_listener(this);
  link_->SetFont(rb.GetFont(ui::ResourceBundle::MediumFont));
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
    const views::Event& event) {
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
    button_view_->deny_button()->ClearMaxTextSize();
  } else {
    bool link_visible = true;
    string16 accelerator;
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
          UTF8ToUTF16(" ") +
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
    views::Widget* frame,
    Browser* browser,
    const GURL& url,
    FullscreenExitBubbleType bubble_type)
    : FullscreenExitBubble(browser, url, bubble_type),
      root_view_(frame->GetRootView()),
      popup_(NULL),
      size_animation_(new ui::SlideAnimation(this)) {
  size_animation_->Reset(1);

  // Create the contents view.
  ui::Accelerator accelerator(ui::VKEY_UNKNOWN, ui::EF_NONE);
  bool got_accelerator = frame->GetAccelerator(IDC_FULLSCREEN, &accelerator);
  DCHECK(got_accelerator);
  view_ = new FullscreenExitView(
      this, accelerator.GetShortcutText(), url, bubble_type_);

  // TODO(yzshen): Change to use the new views bubble, BubbleDelegateView.
  // Initialize the popup.
  popup_ = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.transparent = true;
  params.can_activate = false;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = frame->GetNativeView();
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
  popup_->Show();  // This does not activate the popup.

  StartWatchingMouseIfNecessary();
}

FullscreenExitBubbleViews::~FullscreenExitBubbleViews() {
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
  MessageLoop::current()->DeleteSoon(FROM_HERE, popup_);
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

  StopWatchingMouse();
  StartWatchingMouseIfNecessary();
}

void FullscreenExitBubbleViews::AnimationProgressed(
    const ui::Animation* animation) {
  gfx::Rect popup_rect(GetPopupRect(false));
  if (popup_rect.IsEmpty()) {
    popup_->Hide();
  } else {
    popup_->SetBounds(popup_rect);
    view_->SetY(popup_rect.height() - view_->height());
    popup_->Show();
  }
}

void FullscreenExitBubbleViews::AnimationEnded(
    const ui::Animation* animation) {
  AnimationProgressed(animation);
}

gfx::Rect FullscreenExitBubbleViews::GetPopupRect(
    bool ignore_animation_state) const {
  gfx::Size size(view_->GetPreferredSize());
  // NOTE: don't use the bounds of the root_view_. On linux changing window
  // size is async. Instead we use the size of the screen.
  gfx::Rect screen_bounds = gfx::Screen::GetMonitorNearestWindow(
      root_view_->GetWidget()->GetNativeView()).bounds();
  gfx::Point origin(screen_bounds.x() +
                    (screen_bounds.width() - size.width()) / 2,
                    kPopupTopPx + screen_bounds.y());
  if (!ignore_animation_state) {
    int total_height = size.height() + kPopupTopPx;
    int popup_bottom = size_animation_->CurrentValueBetween(
        static_cast<double>(total_height), 0.0f);
    int y_offset = std::min(popup_bottom, kPopupTopPx);
    size.set_height(size.height() - popup_bottom + y_offset);
    origin.set_y(origin.y() - y_offset);
  }
  return gfx::Rect(origin, size);
}

gfx::Point FullscreenExitBubbleViews::GetCursorScreenPoint() {
  gfx::Point cursor_pos = gfx::Screen::GetCursorScreenPoint();
  views::View::ConvertPointToView(NULL, root_view_, &cursor_pos);
  return cursor_pos;
}

bool FullscreenExitBubbleViews::WindowContainsPoint(gfx::Point pos) {
  return root_view_->HitTest(pos);
}

bool FullscreenExitBubbleViews::IsWindowActive() {
  return root_view_->GetWidget()->IsActive();
}

void FullscreenExitBubbleViews::Hide() {
  size_animation_->SetSlideDuration(kSlideOutDurationMs);
  size_animation_->Hide();
}

void FullscreenExitBubbleViews::Show() {
  size_animation_->SetSlideDuration(kSlideInDurationMs);
  size_animation_->Show();
}

bool FullscreenExitBubbleViews::IsAnimating() {
  return size_animation_->GetCurrentValue() != 0;
}

void FullscreenExitBubbleViews::StartWatchingMouseIfNecessary() {
  if (!fullscreen_bubble::ShowButtonsForType(bubble_type_))
    StartWatchingMouse();
}
