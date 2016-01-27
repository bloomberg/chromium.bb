// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/exclusive_access_bubble_views.h"

#include <utility>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/views/exclusive_access_bubble_views_context.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/notification_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/screen.h"
#include "ui/native_theme/native_theme.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "ui/base/l10n/l10n_util_win.h"
#endif

// ExclusiveAccessView ---------------------------------------------------------

namespace {

// Space between the site info label and the buttons / link.
const int kMiddlePaddingPx = 30;

const int kOuterPaddingHorizPx = 24;
const int kOuterPaddingVertPx = 8;

// Partially-transparent background color. Only used with
// IsSimplifiedFullscreenUIEnabled.
const SkColor kBackgroundColor = SkColorSetARGB(0xcc, 0x28, 0x2c, 0x32);

class ButtonView : public views::View {
 public:
  ButtonView(views::ButtonListener* listener, int between_button_spacing);
  ~ButtonView() override;

  views::LabelButton* accept_button() const { return accept_button_; }
  views::LabelButton* deny_button() const { return deny_button_; }

 private:
  views::LabelButton* accept_button_;
  views::LabelButton* deny_button_;
  DISALLOW_COPY_AND_ASSIGN(ButtonView);
};

ButtonView::ButtonView(views::ButtonListener* listener,
                       int between_button_spacing)
    : accept_button_(nullptr), deny_button_(nullptr) {
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

// Class containing the exit instruction text. Contains fancy styling on the
// keyboard key (not just a simple label).
class InstructionView : public views::View {
 public:
  // Creates an InstructionView with specific text. |text| may contain a single
  // segment delimited by a pair of pipes ('|'); this segment will be displayed
  // as a keyboard key. e.g., "Press |Esc| to exit" will have "Esc" rendered as
  // a key.
  InstructionView(const base::string16& text,
                  const gfx::FontList& font_list,
                  SkColor foreground_color,
                  SkColor background_color);

  void SetText(const base::string16& text);

 private:
  views::Label* before_key_;
  views::Label* key_name_label_;
  views::View* key_name_;
  views::Label* after_key_;

  DISALLOW_COPY_AND_ASSIGN(InstructionView);
};

InstructionView::InstructionView(const base::string16& text,
                                 const gfx::FontList& font_list,
                                 SkColor foreground_color,
                                 SkColor background_color) {
  // Spacing around the escape key name.
  const int kKeyNameMarginHorizPx = 7;
  const int kKeyNameBorderPx = 1;
  const int kKeyNameCornerRadius = 2;
  const int kKeyNamePaddingPx = 5;

  // The |between_child_spacing| is the horizontal margin of the key name.
  views::BoxLayout* layout = new views::BoxLayout(views::BoxLayout::kHorizontal,
                                                  0, 0, kKeyNameMarginHorizPx);
  SetLayoutManager(layout);

  before_key_ = new views::Label(base::string16(), font_list);
  before_key_->SetEnabledColor(foreground_color);
  before_key_->SetBackgroundColor(background_color);
  AddChildView(before_key_);

  key_name_label_ = new views::Label(base::string16(), font_list);
  key_name_label_->SetEnabledColor(foreground_color);
  key_name_label_->SetBackgroundColor(background_color);

  key_name_ = new views::View;
  views::BoxLayout* key_name_layout = new views::BoxLayout(
      views::BoxLayout::kHorizontal, kKeyNamePaddingPx, kKeyNamePaddingPx, 0);
  key_name_->SetLayoutManager(key_name_layout);
  key_name_->AddChildView(key_name_label_);
  // The key name has a border around it.
  scoped_ptr<views::Border> border(views::Border::CreateRoundedRectBorder(
      kKeyNameBorderPx, kKeyNameCornerRadius, foreground_color));
  key_name_->SetBorder(std::move(border));
  AddChildView(key_name_);

  after_key_ = new views::Label(base::string16(), font_list);
  after_key_->SetEnabledColor(foreground_color);
  after_key_->SetBackgroundColor(background_color);
  AddChildView(after_key_);

  SetText(text);
}

void InstructionView::SetText(const base::string16& text) {
  // Parse |text|, looking for pipe-delimited segment.
  std::vector<base::string16> segments =
      base::SplitString(text, base::ASCIIToUTF16("|"), base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_ALL);
  // Expect 1 or 3 pieces (either no pipe-delimited segments, or one).
  DCHECK(segments.size() <= 1 || segments.size() == 3);

  before_key_->SetText(segments.size() ? segments[0] : base::string16());

  if (segments.size() < 3) {
    key_name_->SetVisible(false);
    after_key_->SetVisible(false);
    return;
  }

  before_key_->SetText(segments[0]);
  key_name_label_->SetText(segments[1]);
  key_name_->SetVisible(true);
  after_key_->SetVisible(true);
  after_key_->SetText(segments[2]);
}

}  // namespace

class ExclusiveAccessBubbleViews::ExclusiveAccessView
    : public views::View,
      public views::ButtonListener,
      public views::LinkListener {
 public:
  ExclusiveAccessView(ExclusiveAccessBubbleViews* bubble,
                      const base::string16& accelerator,
                      const GURL& url,
                      ExclusiveAccessBubbleType bubble_type);
  ~ExclusiveAccessView() override;

  // views::ButtonListener
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::LinkListener
  void LinkClicked(views::Link* source, int event_flags) override;

  void UpdateContent(const GURL& url, ExclusiveAccessBubbleType bubble_type);

 private:
  ExclusiveAccessBubbleViews* bubble_;

  // Clickable hint text for exiting fullscreen mode. (Non-simplified mode
  // only.)
  views::Link* link_;
  // Informational label: 'www.foo.com has gone fullscreen'. (Non-simplified
  // mode only.)
  views::Label* message_label_;
  // Clickable buttons to exit fullscreen. (Non-simplified mode only.)
  ButtonView* button_view_;
  // Instruction for exiting fullscreen / mouse lock. Only present if there is
  // no link or button (always present in simplified mode).
  InstructionView* exit_instruction_;
  const base::string16 browser_fullscreen_exit_accelerator_;

  DISALLOW_COPY_AND_ASSIGN(ExclusiveAccessView);
};

ExclusiveAccessBubbleViews::ExclusiveAccessView::ExclusiveAccessView(
    ExclusiveAccessBubbleViews* bubble,
    const base::string16& accelerator,
    const GURL& url,
    ExclusiveAccessBubbleType bubble_type)
    : bubble_(bubble),
      link_(nullptr),
      message_label_(nullptr),
      button_view_(nullptr),
      exit_instruction_(nullptr),
      browser_fullscreen_exit_accelerator_(accelerator) {
  views::BubbleBorder::Shadow shadow_type = views::BubbleBorder::BIG_SHADOW;
#if defined(OS_LINUX)
  // Use a smaller shadow on Linux (including ChromeOS) as the shadow assets can
  // overlap each other in a fullscreen notification bubble.
  // See http://crbug.com/462983.
  shadow_type = views::BubbleBorder::SMALL_SHADOW;
#endif
  if (ExclusiveAccessManager::IsSimplifiedFullscreenUIEnabled())
    shadow_type = views::BubbleBorder::NO_ASSETS;

  // TODO(estade): don't use this static instance. See http://crbug.com/558162
  ui::NativeTheme* theme = ui::NativeTheme::GetInstanceForWeb();
  SkColor background_color =
      ExclusiveAccessManager::IsSimplifiedFullscreenUIEnabled()
          ? kBackgroundColor
          : theme->GetSystemColor(ui::NativeTheme::kColorId_BubbleBackground);
  SkColor foreground_color =
      ExclusiveAccessManager::IsSimplifiedFullscreenUIEnabled()
          ? SK_ColorWHITE
          : theme->GetSystemColor(ui::NativeTheme::kColorId_LabelEnabledColor);

  scoped_ptr<views::BubbleBorder> bubble_border(new views::BubbleBorder(
      views::BubbleBorder::NONE, shadow_type, background_color));
  set_background(new views::BubbleBackground(bubble_border.get()));
  SetBorder(std::move(bubble_border));
  SetFocusable(false);

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  ui::ResourceBundle::FontStyle font_style =
      ExclusiveAccessManager::IsSimplifiedFullscreenUIEnabled()
          ? ui::ResourceBundle::SmallFont
          : ui::ResourceBundle::MediumFont;
  const gfx::FontList& font_list = rb.GetFontList(font_style);

  if (!ExclusiveAccessManager::IsSimplifiedFullscreenUIEnabled()) {
    message_label_ = new views::Label(base::string16(), font_list);
    message_label_->SetEnabledColor(foreground_color);
    message_label_->SetBackgroundColor(background_color);
  }

  exit_instruction_ = new InstructionView(base::string16(), font_list,
                                          foreground_color, background_color);

  link_ = new views::Link();
  link_->SetFocusable(false);
#if defined(OS_CHROMEOS)
  // On CrOS, the link text doesn't change, since it doesn't show the shortcut.
  link_->SetText(l10n_util::GetStringUTF16(IDS_EXIT_FULLSCREEN_MODE));
#endif
  link_->set_listener(this);
  link_->SetFontList(font_list);
  link_->SetPressedColor(foreground_color);
  link_->SetEnabledColor(foreground_color);
  link_->SetBackgroundColor(background_color);
  link_->SetVisible(false);

  button_view_ = new ButtonView(this, kPaddingPx);

  int outer_padding_horiz = kOuterPaddingHorizPx;
  int outer_padding_vert = kOuterPaddingVertPx;
  if (!ExclusiveAccessManager::IsSimplifiedFullscreenUIEnabled()) {
    DCHECK(message_label_);
    AddChildView(message_label_);

    outer_padding_horiz = kPaddingPx;
    outer_padding_vert = kPaddingPx;
  }
  AddChildView(button_view_);
  AddChildView(exit_instruction_);
  AddChildView(link_);

  views::BoxLayout* layout =
      new views::BoxLayout(views::BoxLayout::kHorizontal, outer_padding_horiz,
                           outer_padding_vert, kMiddlePaddingPx);
  SetLayoutManager(layout);

  UpdateContent(url, bubble_type);
}

ExclusiveAccessBubbleViews::ExclusiveAccessView::~ExclusiveAccessView() {
}

void ExclusiveAccessBubbleViews::ExclusiveAccessView::ButtonPressed(
    views::Button* sender,
    const ui::Event& event) {
  if (sender == button_view_->accept_button())
    bubble_->Accept();
  else
    bubble_->Cancel();
}

void ExclusiveAccessBubbleViews::ExclusiveAccessView::LinkClicked(
    views::Link* link,
    int event_flags) {
  bubble_->ExitExclusiveAccess();
}

void ExclusiveAccessBubbleViews::ExclusiveAccessView::UpdateContent(
    const GURL& url,
    ExclusiveAccessBubbleType bubble_type) {
  DCHECK_NE(EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE, bubble_type);

  if (!ExclusiveAccessManager::IsSimplifiedFullscreenUIEnabled()) {
    DCHECK(message_label_);
    message_label_->SetText(bubble_->GetCurrentMessageText());
  }

  if (exclusive_access_bubble::ShowButtonsForType(bubble_type)) {
    link_->SetVisible(false);
    exit_instruction_->SetVisible(false);
    button_view_->SetVisible(true);
    button_view_->deny_button()->SetText(bubble_->GetCurrentDenyButtonText());
    button_view_->deny_button()->SetMinSize(gfx::Size());
    button_view_->accept_button()->SetText(
        bubble_->GetCurrentAllowButtonText());
    button_view_->accept_button()->SetMinSize(gfx::Size());
  } else {
    bool link_visible =
        !ExclusiveAccessManager::IsSimplifiedFullscreenUIEnabled();
    base::string16 accelerator;
    if (bubble_type ==
            EXCLUSIVE_ACCESS_BUBBLE_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION ||
        bubble_type ==
            EXCLUSIVE_ACCESS_BUBBLE_TYPE_EXTENSION_FULLSCREEN_EXIT_INSTRUCTION) {
      accelerator = browser_fullscreen_exit_accelerator_;
    } else {
      accelerator = l10n_util::GetStringUTF16(IDS_APP_ESC_KEY);
      if (bubble_type !=
          EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION) {
        link_visible = false;
      }
    }
#if !defined(OS_CHROMEOS)
    if (link_visible) {
      link_->SetText(l10n_util::GetStringUTF16(IDS_EXIT_FULLSCREEN_MODE) +
                     base::UTF8ToUTF16(" ") +
                     l10n_util::GetStringFUTF16(
                         IDS_EXIT_FULLSCREEN_MODE_ACCELERATOR, accelerator));
    }
#endif
    link_->SetVisible(link_visible);
    exit_instruction_->SetText(bubble_->GetInstructionText(accelerator));
    exit_instruction_->SetVisible(!link_visible);
    button_view_->SetVisible(false);
  }
}

// ExclusiveAccessBubbleViews --------------------------------------------------

ExclusiveAccessBubbleViews::ExclusiveAccessBubbleViews(
    ExclusiveAccessBubbleViewsContext* context,
    const GURL& url,
    ExclusiveAccessBubbleType bubble_type)
    : ExclusiveAccessBubble(context->GetExclusiveAccessManager(),
                            url,
                            bubble_type),
      bubble_view_context_(context),
      popup_(nullptr),
      animation_(new gfx::SlideAnimation(this)),
      animated_attribute_(ANIMATED_ATTRIBUTE_BOUNDS) {
  // With the simplified fullscreen UI flag, initially hide the bubble;
  // otherwise, initially show it.
  double initial_value =
      ExclusiveAccessManager::IsSimplifiedFullscreenUIEnabled() ? 0 : 1;
  animation_->Reset(initial_value);

  // Create the contents view.
  ui::Accelerator accelerator(ui::VKEY_UNKNOWN, ui::EF_NONE);
  bool got_accelerator =
      bubble_view_context_->GetBubbleAssociatedWidget()->GetAccelerator(
          IDC_FULLSCREEN, &accelerator);
  DCHECK(got_accelerator);
  view_ = new ExclusiveAccessView(this, accelerator.GetShortcutText(), url,
                                  bubble_type_);

  // TODO(yzshen): Change to use the new views bubble, BubbleDelegateView.
  // TODO(pkotwicz): When this becomes a views bubble, make sure that this
  // bubble is ignored by ImmersiveModeControllerAsh::BubbleManager.
  // Initialize the popup.
  popup_ = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent =
      bubble_view_context_->GetBubbleAssociatedWidget()->GetNativeView();
  // The simplified UI just shows a notice; clicks should go through to the
  // underlying window.
  params.accept_events =
      !ExclusiveAccessManager::IsSimplifiedFullscreenUIEnabled();
  popup_->Init(params);
  popup_->SetContentsView(view_);
  gfx::Size size = GetPopupRect(true).size();
  // Bounds are in screen coordinates.
  popup_->SetBounds(GetPopupRect(false));
  // We set layout manager to nullptr to prevent the widget from sizing its
  // contents to the same size as itself. This prevents the widget contents from
  // shrinking while we animate the height of the popup to give the impression
  // that it is sliding off the top of the screen.
  popup_->GetRootView()->SetLayoutManager(nullptr);
  view_->SetBounds(0, 0, size.width(), size.height());
  if (!ExclusiveAccessManager::IsSimplifiedFullscreenUIEnabled())
    popup_->ShowInactive();  // This does not activate the popup.

  popup_->AddObserver(this);

  registrar_.Add(this, chrome::NOTIFICATION_FULLSCREEN_CHANGED,
                 content::Source<FullscreenController>(
                     bubble_view_context_->GetExclusiveAccessManager()
                         ->fullscreen_controller()));

  UpdateForImmersiveState();
}

ExclusiveAccessBubbleViews::~ExclusiveAccessBubbleViews() {
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

void ExclusiveAccessBubbleViews::UpdateContent(
    const GURL& url,
    ExclusiveAccessBubbleType bubble_type) {
  DCHECK_NE(EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE, bubble_type);
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

void ExclusiveAccessBubbleViews::RepositionIfVisible() {
  if (popup_->IsVisible())
    UpdateBounds();
}

views::View* ExclusiveAccessBubbleViews::GetView() {
  return view_;
}

void ExclusiveAccessBubbleViews::UpdateMouseWatcher() {
  bool should_watch_mouse = false;
  if (popup_->IsVisible())
    should_watch_mouse =
        !exclusive_access_bubble::ShowButtonsForType(bubble_type_);
  else
    should_watch_mouse = CanMouseTriggerSlideIn();

  if (should_watch_mouse == IsWatchingMouse())
    return;

  if (should_watch_mouse)
    StartWatchingMouse();
  else
    StopWatchingMouse();
}

void ExclusiveAccessBubbleViews::UpdateForImmersiveState() {
  AnimatedAttribute expected_animated_attribute =
      ExclusiveAccessManager::IsSimplifiedFullscreenUIEnabled() ||
              bubble_view_context_->IsImmersiveModeEnabled()
          ? ANIMATED_ATTRIBUTE_OPACITY
          : ANIMATED_ATTRIBUTE_BOUNDS;
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

void ExclusiveAccessBubbleViews::UpdateBounds() {
  gfx::Rect popup_rect(GetPopupRect(false));
  if (!popup_rect.IsEmpty()) {
    popup_->SetBounds(popup_rect);
    view_->SetY(popup_rect.height() - view_->height());
  }
}

views::View* ExclusiveAccessBubbleViews::GetBrowserRootView() const {
  return bubble_view_context_->GetBubbleAssociatedWidget()->GetRootView();
}

void ExclusiveAccessBubbleViews::AnimationProgressed(
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

void ExclusiveAccessBubbleViews::AnimationEnded(
    const gfx::Animation* animation) {
  AnimationProgressed(animation);
}

gfx::Rect ExclusiveAccessBubbleViews::GetPopupRect(
    bool ignore_animation_state) const {
  gfx::Size size(view_->GetPreferredSize());
  gfx::Rect widget_bounds = bubble_view_context_->GetBubbleAssociatedWidget()
                                ->GetClientAreaBoundsInScreen();
  int x = widget_bounds.x() + (widget_bounds.width() - size.width()) / 2;

  int top_container_bottom = widget_bounds.y();
  if (bubble_view_context_->IsImmersiveModeEnabled()) {
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
        bubble_view_context_->GetTopContainerBoundsInScreen().bottom();
  }
  // |desired_top| is the top of the bubble area including the shadow.
  int popup_top = ExclusiveAccessManager::IsSimplifiedFullscreenUIEnabled()
                      ? kSimplifiedPopupTopPx
                      : kPopupTopPx;
  int desired_top = popup_top - view_->border()->GetInsets().top();
  int y = top_container_bottom + desired_top;

  if (!ignore_animation_state &&
      animated_attribute_ == ANIMATED_ATTRIBUTE_BOUNDS) {
    int total_height = size.height() + desired_top;
    int popup_bottom = animation_->CurrentValueBetween(total_height, 0);
    int y_offset = std::min(popup_bottom, desired_top);
    size.set_height(size.height() - popup_bottom + y_offset);
    y -= y_offset;
  }
  return gfx::Rect(gfx::Point(x, y), size);
}

gfx::Point ExclusiveAccessBubbleViews::GetCursorScreenPoint() {
  gfx::Point cursor_pos = gfx::Screen::GetScreen()->GetCursorScreenPoint();
  views::View::ConvertPointFromScreen(GetBrowserRootView(), &cursor_pos);
  return cursor_pos;
}

bool ExclusiveAccessBubbleViews::WindowContainsPoint(gfx::Point pos) {
  return GetBrowserRootView()->HitTestPoint(pos);
}

bool ExclusiveAccessBubbleViews::IsWindowActive() {
  return bubble_view_context_->GetBubbleAssociatedWidget()->IsActive();
}

void ExclusiveAccessBubbleViews::Hide() {
  animation_->SetSlideDuration(kSlideOutDurationMs);
  animation_->Hide();
}

void ExclusiveAccessBubbleViews::Show() {
  animation_->SetSlideDuration(kSlideInDurationMs);
  animation_->Show();
}

bool ExclusiveAccessBubbleViews::IsAnimating() {
  return animation_->is_animating();
}

bool ExclusiveAccessBubbleViews::CanMouseTriggerSlideIn() const {
  return !bubble_view_context_->IsImmersiveModeEnabled();
}

void ExclusiveAccessBubbleViews::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_FULLSCREEN_CHANGED, type);
  UpdateForImmersiveState();
}

void ExclusiveAccessBubbleViews::OnWidgetVisibilityChanged(
    views::Widget* widget,
    bool visible) {
  UpdateMouseWatcher();
}
