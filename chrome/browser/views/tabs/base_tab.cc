// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/tabs/base_tab.h"

#include <limits>

#include "app/animation_container.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "app/slide_animation.h"
#include "app/throb_animation.h"
#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/views/tabs/tab_controller.h"
#include "chrome/browser/view_ids.h"
#include "chrome/common/chrome_switches.h"
#include "gfx/canvas_skia.h"
#include "gfx/favicon_size.h"
#include "gfx/font.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/controls/button/image_button.h"

#ifdef WIN32
#include "app/win_util.h"
#endif

// How long the pulse throb takes.
static const int kPulseDurationMs = 200;

// How long the hover state takes.
static const int kHoverDurationMs = 90;

static SkBitmap* waiting_animation_frames = NULL;
static SkBitmap* loading_animation_frames = NULL;
static int loading_animation_frame_count = 0;
static int waiting_animation_frame_count = 0;
static int waiting_to_loading_frame_count_ratio = 0;

// Close button images.
static SkBitmap* close_button_n = NULL;
static SkBitmap* close_button_h = NULL;
static SkBitmap* close_button_p = NULL;

static SkBitmap* crashed_fav_icon = NULL;

namespace {

////////////////////////////////////////////////////////////////////////////////
// TabCloseButton
//
//  This is a Button subclass that causes middle clicks to be forwarded to the
//  parent View by explicitly not handling them in OnMousePressed.
class TabCloseButton : public views::ImageButton {
 public:
  explicit TabCloseButton(views::ButtonListener* listener)
      : views::ImageButton(listener) {
  }
  virtual ~TabCloseButton() {}

  virtual bool OnMousePressed(const views::MouseEvent& event) {
    bool handled = ImageButton::OnMousePressed(event);
    // Explicitly mark midle-mouse clicks as non-handled to ensure the tab
    // sees them.
    return event.IsOnlyMiddleMouseButton() ? false : handled;
  }

  // We need to let the parent know about mouse state so that it
  // can highlight itself appropriately. Note that Exit events
  // fire before Enter events, so this works.
  virtual void OnMouseEntered(const views::MouseEvent& event) {
    CustomButton::OnMouseEntered(event);
    GetParent()->OnMouseEntered(event);
  }

  virtual void OnMouseExited(const views::MouseEvent& event) {
    CustomButton::OnMouseExited(event);
    GetParent()->OnMouseExited(event);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TabCloseButton);
};

}  // namespace

// static
int BaseTab::close_button_width_ = 0;
// static
int BaseTab::close_button_height_ = 0;
// static
int BaseTab::loading_animation_size_ = 0;

// static
gfx::Font* BaseTab::font_ = NULL;
// static
int BaseTab::font_height_ = 0;

////////////////////////////////////////////////////////////////////////////////
// FaviconCrashAnimation
//
//  A custom animation subclass to manage the favicon crash animation.
class BaseTab::FavIconCrashAnimation : public LinearAnimation,
                                       public AnimationDelegate {
 public:
  explicit FavIconCrashAnimation(BaseTab* target)
      : ALLOW_THIS_IN_INITIALIZER_LIST(LinearAnimation(1000, 25, this)),
        target_(target) {
  }
  virtual ~FavIconCrashAnimation() {}

  // Animation overrides:
  virtual void AnimateToState(double state) {
    const double kHidingOffset = 27;

    if (state < .5) {
      target_->SetFavIconHidingOffset(
          static_cast<int>(floor(kHidingOffset * 2.0 * state)));
    } else {
      target_->DisplayCrashedFavIcon();
      target_->SetFavIconHidingOffset(
          static_cast<int>(
              floor(kHidingOffset - ((state - .5) * 2.0 * kHidingOffset))));
    }
  }

  // AnimationDelegate overrides:
  virtual void AnimationCanceled(const Animation* animation) {
    target_->SetFavIconHidingOffset(0);
  }

 private:
  BaseTab* target_;

  DISALLOW_COPY_AND_ASSIGN(FavIconCrashAnimation);
};

BaseTab::BaseTab(TabController* controller)
    : controller_(controller),
      closing_(false),
      dragging_(false),
      loading_animation_frame_(0),
      throbber_disabled_(false),
      theme_provider_(NULL),
      fav_icon_hiding_offset_(0),
      should_display_crashed_favicon_(false) {
  BaseTab::InitResources();

  SetID(VIEW_ID_TAB);

  // Add the Close Button.
  TabCloseButton* close_button = new TabCloseButton(this);
  close_button_ = close_button;
  close_button->SetImage(views::CustomButton::BS_NORMAL, close_button_n);
  close_button->SetImage(views::CustomButton::BS_HOT, close_button_h);
  close_button->SetImage(views::CustomButton::BS_PUSHED, close_button_p);
  close_button->SetTooltipText(l10n_util::GetString(IDS_TOOLTIP_CLOSE_TAB));
  close_button->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_CLOSE));
  // Disable animation so that the red danger sign shows up immediately
  // to help avoid mis-clicks.
  close_button->SetAnimationDuration(0);
  AddChildView(close_button);

  SetContextMenuController(this);
}

BaseTab::~BaseTab() {
}

void BaseTab::SetData(const TabRendererData& data) {
  TabRendererData old(data_);
  data_ = data;

  if (data_.crashed) {
    if (!should_display_crashed_favicon_ && !IsPerformingCrashAnimation())
      StartCrashAnimation();
  } else {
    if (IsPerformingCrashAnimation())
      StopCrashAnimation();
    ResetCrashedFavIcon();
  }

  // Sets the accessible name for the tab.
  SetAccessibleName(UTF16ToWide(data_.title));

  DataChanged(old);

  Layout();
}

void BaseTab::UpdateLoadingAnimation(TabRendererData::NetworkState state) {
  // If this is an extension app and a command line flag is set,
  // then disable the throbber.
  throbber_disabled_ = data().app &&
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kAppsNoThrob);

  if (throbber_disabled_)
    return;

  if (state == data_.network_state &&
      state == TabRendererData::NETWORK_STATE_NONE) {
    // If the network state is none and hasn't changed, do nothing. Otherwise we
    // need to advance the animation frame.
    return;
  }

  TabRendererData::NetworkState old_state = data_.network_state;
  data_.network_state = state;
  AdvanceLoadingAnimation(old_state, state);
}

void BaseTab::StartPulse() {
  if (!pulse_animation_.get()) {
    pulse_animation_.reset(new ThrobAnimation(this));
    pulse_animation_->SetSlideDuration(kPulseDurationMs);
    if (animation_container_.get())
      pulse_animation_->SetContainer(animation_container_.get());
  }
  pulse_animation_->Reset();
  pulse_animation_->StartThrobbing(std::numeric_limits<int>::max());
}

void BaseTab::StopPulse() {
  if (!pulse_animation_.get())
    return;

  pulse_animation_->Stop();  // Do stop so we get notified.
  pulse_animation_.reset(NULL);
}

bool BaseTab::IsSelected() const {
  return controller() ? controller()->IsTabSelected(this) : true;
}

bool BaseTab::IsCloseable() const {
  return controller() ? controller()->IsTabCloseable(this) : true;
}

void BaseTab::OnMouseEntered(const views::MouseEvent& e) {
  if (!hover_animation_.get()) {
    hover_animation_.reset(new SlideAnimation(this));
    hover_animation_->SetContainer(animation_container_.get());
    hover_animation_->SetSlideDuration(kHoverDurationMs);
  }
  hover_animation_->SetTweenType(Tween::EASE_OUT);
  hover_animation_->Show();
}

void BaseTab::OnMouseExited(const views::MouseEvent& e) {
  hover_animation_->SetTweenType(Tween::EASE_IN);
  hover_animation_->Hide();
}

bool BaseTab::OnMousePressed(const views::MouseEvent& event) {
  if (!controller())
    return false;

  if (event.IsOnlyLeftMouseButton()) {
    // Store whether or not we were selected just now... we only want to be
    // able to drag foreground tabs, so we don't start dragging the tab if
    // it was in the background.
    bool just_selected = !IsSelected();
    if (just_selected)
      controller()->SelectTab(this);
    controller()->MaybeStartDrag(this, event);
  }
  return true;
}

bool BaseTab::OnMouseDragged(const views::MouseEvent& event) {
  if (controller())
    controller()->ContinueDrag(event);
  return true;
}

void BaseTab::OnMouseReleased(const views::MouseEvent& event, bool canceled) {
  if (!controller())
    return;

  // Notify the drag helper that we're done with any potential drag operations.
  // Clean up the drag helper, which is re-created on the next mouse press.
  // In some cases, ending the drag will schedule the tab for destruction; if
  // so, bail immediately, since our members are already dead and we shouldn't
  // do anything else except drop the tab where it is.
  if (controller()->EndDrag(canceled))
    return;

  // Close tab on middle click, but only if the button is released over the tab
  // (normal windows behavior is to discard presses of a UI element where the
  // releases happen off the element).
  if (event.IsMiddleMouseButton()) {
    if (HitTest(event.location())) {
      controller()->CloseTab(this);
    } else if (closing_) {
      // We're animating closed and a middle mouse button was pushed on us but
      // we don't contain the mouse anymore. We assume the user is clicking
      // quicker than the animation and we should close the tab that falls under
      // the mouse.
      BaseTab* closest_tab = controller()->GetTabAt(this, event.location());
      if (closest_tab)
        controller()->CloseTab(closest_tab);
    }
  }
}

bool BaseTab::GetTooltipText(const gfx::Point& p, std::wstring* tooltip) {
  if (data_.title.empty())
    return false;

  std::wstring title = UTF16ToWide(data_.title);
  // Only show the tooltip if the title is truncated.
  if (font_->GetStringWidth(title) > title_bounds().width()) {
    *tooltip = title;
    return true;
  }
  return false;
}

bool BaseTab::GetAccessibleRole(AccessibilityTypes::Role* role) {
  DCHECK(role);

  *role = AccessibilityTypes::ROLE_PAGETAB;
  return true;
}

ThemeProvider* BaseTab::GetThemeProvider() {
  ThemeProvider* tp = View::GetThemeProvider();
  return tp ? tp : theme_provider_;
}

void BaseTab::AdvanceLoadingAnimation(TabRendererData::NetworkState old_state,
                                      TabRendererData::NetworkState state) {
  // The waiting animation is the reverse of the loading animation, but at a
  // different rate - the following reverses and scales the animation_frame_
  // so that the frame is at an equivalent position when going from one
  // animation to the other.
  if (state != old_state) {
    loading_animation_frame_ = loading_animation_frame_count -
        (loading_animation_frame_ / waiting_to_loading_frame_count_ratio);
  }

  if (state != TabRendererData::NETWORK_STATE_NONE) {
    loading_animation_frame_ = ++loading_animation_frame_ %
        ((state == TabRendererData::NETWORK_STATE_WAITING) ?
            waiting_animation_frame_count : loading_animation_frame_count);
  } else {
    loading_animation_frame_ = 0;
  }
  SchedulePaint();
}

void BaseTab::PaintIcon(gfx::Canvas* canvas, int x, int y) {
  if (base::i18n::IsRTL()) {
    if (!data().favicon.isNull())
      x = width() - x - data().favicon.width();
    else
      x = width() - x - kFavIconSize;
  }

  int favicon_x = x;
  if (!data().favicon.isNull() && data().favicon.width() != kFavIconSize)
    favicon_x += (data().favicon.width() - kFavIconSize) / 2;

  if (data().network_state != TabRendererData::NETWORK_STATE_NONE) {
    SkBitmap* frames =
        (data().network_state == TabRendererData::NETWORK_STATE_WAITING) ?
        waiting_animation_frames : loading_animation_frames;
    int image_size = frames->height();
    int image_offset = loading_animation_frame_ * image_size;
    int dst_y = (height() - image_size) / 2;
    canvas->DrawBitmapInt(*frames, image_offset, 0, image_size,
                          image_size, favicon_x, dst_y, image_size, image_size,
                          false);
  } else {
    canvas->Save();
    canvas->ClipRectInt(0, 0, width(), height());
    if (should_display_crashed_favicon_) {
      canvas->DrawBitmapInt(*crashed_fav_icon, 0, 0,
                            crashed_fav_icon->width(),
                            crashed_fav_icon->height(),
                            favicon_x,
                            (height() - crashed_fav_icon->height()) / 2 +
                                fav_icon_hiding_offset_,
                            kFavIconSize, kFavIconSize,
                            true);
    } else {
      if (!data().favicon.isNull()) {
        // TODO(pkasting): Use code in tab_icon_view.cc:PaintIcon() (or switch
        // to using that class to render the favicon).
        int size = data().favicon.width();
        canvas->DrawBitmapInt(data().favicon, 0, 0,
                              data().favicon.width(),
                              data().favicon.height(),
                              x, y + fav_icon_hiding_offset_, size, size,
                              true);
      }
    }
    canvas->Restore();
  }
}

void BaseTab::PaintTitle(gfx::Canvas* canvas, SkColor title_color) {
  // Paint the Title.
  string16 title = data().title;
  if (title.empty()) {
    title = data().loading ?
        l10n_util::GetStringUTF16(IDS_TAB_LOADING_TITLE) :
        TabContents::GetDefaultTitle();
  } else {
    Browser::FormatTitleForDisplay(&title);
  }

  canvas->DrawStringInt(UTF16ToWideHack(title), *font_, title_color,
                        title_bounds().x(), title_bounds().y(),
                        title_bounds().width(), title_bounds().height());
}

void BaseTab::AnimationProgressed(const Animation* animation) {
  SchedulePaint();
}

void BaseTab::AnimationCanceled(const Animation* animation) {
  SchedulePaint();
}

void BaseTab::AnimationEnded(const Animation* animation) {
  SchedulePaint();
}

void BaseTab::ButtonPressed(views::Button* sender, const views::Event& event) {
  DCHECK(sender == close_button_);
  controller()->CloseTab(this);
}

void BaseTab::ShowContextMenu(views::View* source,
                              const gfx::Point& p,
                              bool is_mouse_gesture) {
  if (controller())
    controller()->ShowContextMenu(this, p);
}

void BaseTab::ThemeChanged() {
  views::View::ThemeChanged();
  LoadThemeImages();
}

void BaseTab::SetFavIconHidingOffset(int offset) {
  fav_icon_hiding_offset_ = offset;
  SchedulePaint();
}

void BaseTab::DisplayCrashedFavIcon() {
  should_display_crashed_favicon_ = true;
}

void BaseTab::ResetCrashedFavIcon() {
  should_display_crashed_favicon_ = false;
}

void BaseTab::StartCrashAnimation() {
  if (!crash_animation_.get())
    crash_animation_.reset(new FavIconCrashAnimation(this));
  crash_animation_->Stop();
  crash_animation_->Start();
}

void BaseTab::StopCrashAnimation() {
  if (!crash_animation_.get())
    return;
  crash_animation_->Stop();
}

bool BaseTab::IsPerformingCrashAnimation() const {
  return crash_animation_.get() && crash_animation_->is_animating();
}

// static
void BaseTab::InitResources() {
  static bool initialized = false;
  if (initialized)
    return;

  initialized = true;

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  crashed_fav_icon = rb.GetBitmapNamed(IDR_SAD_FAVICON);

  close_button_n = rb.GetBitmapNamed(IDR_TAB_CLOSE);
  close_button_h = rb.GetBitmapNamed(IDR_TAB_CLOSE_H);
  close_button_p = rb.GetBitmapNamed(IDR_TAB_CLOSE_P);

  close_button_width_ = close_button_n->width();
  close_button_height_ = close_button_n->height();

  // The loading animation image is a strip of states. Each state must be
  // square, so the height must divide the width evenly.
  loading_animation_frames = rb.GetBitmapNamed(IDR_THROBBER);
  loading_animation_size_ = loading_animation_frames->height();
  DCHECK(loading_animation_frames);
  DCHECK(loading_animation_frames->width() %
         loading_animation_frames->height() == 0);
  loading_animation_frame_count =
      loading_animation_frames->width() / loading_animation_frames->height();

  // We get a DIV0 further down when the throbber is replaced by an image which
  // is taller than wide. In this case we cannot deduce an animation sequence
  // from it since we assume that each animation frame has the width of the
  // image's height.
  if (loading_animation_frame_count == 0) {
#ifdef WIN32
    // TODO(idanan): Remove this when we have a way to handle theme errors.
    // See: http://code.google.com/p/chromium/issues/detail?id=12531 For now,
    // this is Windows-specific because some users have downloaded a DLL from
    // outside of Google to override the theme.
    std::wstring text = l10n_util::GetString(IDS_RESOURCE_ERROR);
    std::wstring caption = l10n_util::GetString(IDS_RESOURCE_ERROR_CAPTION);
    UINT flags = MB_OK | MB_ICONWARNING | MB_TOPMOST;
    win_util::MessageBox(NULL, text, caption, flags);
#endif
    CHECK(loading_animation_frame_count) <<
        "Invalid throbber size. Width = " <<
        loading_animation_frames->width() << ", height = " <<
        loading_animation_frames->height();
  }

  waiting_animation_frames = rb.GetBitmapNamed(IDR_THROBBER_WAITING);
  DCHECK(waiting_animation_frames);
  DCHECK(waiting_animation_frames->width() %
         waiting_animation_frames->height() == 0);
  waiting_animation_frame_count =
      waiting_animation_frames->width() / waiting_animation_frames->height();

  waiting_to_loading_frame_count_ratio =
      waiting_animation_frame_count / loading_animation_frame_count;
  // TODO(beng): eventually remove this when we have a proper themeing system.
  //             themes not supporting IDR_THROBBER_WAITING are causing this
  //             value to be 0 which causes DIV0 crashes. The value of 5
  //             matches the current bitmaps in our source.
  if (waiting_to_loading_frame_count_ratio == 0)
    waiting_to_loading_frame_count_ratio = 5;

  font_ = new gfx::Font(rb.GetFont(ResourceBundle::BaseFont));
  font_height_ = font_->height();
}

// static
void BaseTab::LoadThemeImages() {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  loading_animation_frames = rb.GetBitmapNamed(IDR_THROBBER);
  waiting_animation_frames = rb.GetBitmapNamed(IDR_THROBBER_WAITING);
  loading_animation_size_ = loading_animation_frames->height();
}
