// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/tabs/tab_renderer_gtk.h"

#include <algorithm>
#include <utility>

#include "base/utf_string_conversions.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/gtk/bookmarks/bookmark_utils_gtk.h"
#include "chrome/browser/ui/gtk/custom_button.h"
#include "chrome/browser/ui/gtk/gtk_theme_provider.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_service.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/base/animation/throb_animation.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas_skia_paint.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/platform_font_gtk.h"
#include "ui/gfx/skbitmap_operations.h"

namespace {

const int kFontPixelSize = 13;
const int kLeftPadding = 16;
const int kTopPadding = 6;
const int kRightPadding = 15;
const int kBottomPadding = 5;
const int kDropShadowHeight = 2;
const int kFavIconTitleSpacing = 4;
const int kTitleCloseButtonSpacing = 5;
const int kStandardTitleWidth = 175;
const int kDropShadowOffset = 2;
const int kInactiveTabBackgroundOffsetY = 15;

// When a non-mini-tab becomes a mini-tab the width of the tab animates. If
// the width of a mini-tab is >= kMiniTabRendererAsNormalTabWidth then the tab
// is rendered as a normal tab. This is done to avoid having the title
// immediately disappear when transitioning a tab from normal to mini-tab.
const int kMiniTabRendererAsNormalTabWidth =
    browser_defaults::kMiniTabWidth + 30;

// The tab images are designed to overlap the toolbar by 1 pixel. For now we
// don't actually overlap the toolbar, so this is used to know how many pixels
// at the bottom of the tab images are to be ignored.
const int kToolbarOverlap = 1;

// How long the hover state takes.
const int kHoverDurationMs = 90;

// How opaque to make the hover state (out of 1).
const double kHoverOpacity = 0.33;

// Max opacity for the mini-tab title change animation.
const double kMiniTitleChangeThrobOpacity = 0.75;

// Duration for when the title of an inactive mini-tab changes.
const int kMiniTitleChangeThrobDuration = 1000;

const SkScalar kTabCapWidth = 15;
const SkScalar kTabTopCurveWidth = 4;
const SkScalar kTabBottomCurveWidth = 3;

// The vertical and horizontal offset used to position the close button
// in the tab. TODO(jhawkins): Ask pkasting what the Fuzz is about.
const int kCloseButtonVertFuzz = 0;
const int kCloseButtonHorzFuzz = 5;

SkBitmap* crashed_fav_icon = NULL;

// Gets the bounds of |widget| relative to |parent|.
gfx::Rect GetWidgetBoundsRelativeToParent(GtkWidget* parent,
                                          GtkWidget* widget) {
  gfx::Point parent_pos = gtk_util::GetWidgetScreenPosition(parent);
  gfx::Point widget_pos = gtk_util::GetWidgetScreenPosition(widget);
  return gfx::Rect(widget_pos.x() - parent_pos.x(),
                   widget_pos.y() - parent_pos.y(),
                   widget->allocation.width, widget->allocation.height);
}

}  // namespace

TabRendererGtk::LoadingAnimation::Data::Data(
    ui::ThemeProvider* theme_provider) {
  // The loading animation image is a strip of states. Each state must be
  // square, so the height must divide the width evenly.
  loading_animation_frames = theme_provider->GetBitmapNamed(IDR_THROBBER);
  DCHECK(loading_animation_frames);
  DCHECK_EQ(loading_animation_frames->width() %
            loading_animation_frames->height(), 0);
  loading_animation_frame_count =
      loading_animation_frames->width() /
      loading_animation_frames->height();

  waiting_animation_frames =
      theme_provider->GetBitmapNamed(IDR_THROBBER_WAITING);
  DCHECK(waiting_animation_frames);
  DCHECK_EQ(waiting_animation_frames->width() %
            waiting_animation_frames->height(), 0);
  waiting_animation_frame_count =
      waiting_animation_frames->width() /
      waiting_animation_frames->height();

  waiting_to_loading_frame_count_ratio =
      waiting_animation_frame_count /
      loading_animation_frame_count;
  // TODO(beng): eventually remove this when we have a proper themeing system.
  //             themes not supporting IDR_THROBBER_WAITING are causing this
  //             value to be 0 which causes DIV0 crashes. The value of 5
  //             matches the current bitmaps in our source.
  if (waiting_to_loading_frame_count_ratio == 0)
    waiting_to_loading_frame_count_ratio = 5;
}

TabRendererGtk::LoadingAnimation::Data::Data(
    int loading, int waiting, int waiting_to_loading)
    : waiting_animation_frames(NULL),
      loading_animation_frames(NULL),
      loading_animation_frame_count(loading),
      waiting_animation_frame_count(waiting),
      waiting_to_loading_frame_count_ratio(waiting_to_loading) {
}

bool TabRendererGtk::initialized_ = false;
TabRendererGtk::TabImage TabRendererGtk::tab_active_ = {0};
TabRendererGtk::TabImage TabRendererGtk::tab_inactive_ = {0};
TabRendererGtk::TabImage TabRendererGtk::tab_alpha_ = {0};
gfx::Font* TabRendererGtk::title_font_ = NULL;
int TabRendererGtk::title_font_height_ = 0;
int TabRendererGtk::close_button_width_ = 0;
int TabRendererGtk::close_button_height_ = 0;
SkColor TabRendererGtk::selected_title_color_ = SK_ColorBLACK;
SkColor TabRendererGtk::unselected_title_color_ = SkColorSetRGB(64, 64, 64);

////////////////////////////////////////////////////////////////////////////////
// TabRendererGtk::LoadingAnimation, public:
//
TabRendererGtk::LoadingAnimation::LoadingAnimation(
    ui::ThemeProvider* theme_provider)
    : data_(new Data(theme_provider)),
      theme_provider_(theme_provider),
      animation_state_(ANIMATION_NONE),
      animation_frame_(0) {
  registrar_.Add(this,
                 NotificationType::BROWSER_THEME_CHANGED,
                 NotificationService::AllSources());
}

TabRendererGtk::LoadingAnimation::LoadingAnimation(
    const LoadingAnimation::Data& data)
    : data_(new Data(data)),
      theme_provider_(NULL),
      animation_state_(ANIMATION_NONE),
      animation_frame_(0) {
}

TabRendererGtk::LoadingAnimation::~LoadingAnimation() {}

bool TabRendererGtk::LoadingAnimation::ValidateLoadingAnimation(
    AnimationState animation_state) {
  bool has_changed = false;
  if (animation_state_ != animation_state) {
    // The waiting animation is the reverse of the loading animation, but at a
    // different rate - the following reverses and scales the animation_frame_
    // so that the frame is at an equivalent position when going from one
    // animation to the other.
    if (animation_state_ == ANIMATION_WAITING &&
        animation_state == ANIMATION_LOADING) {
      animation_frame_ = data_->loading_animation_frame_count -
          (animation_frame_ / data_->waiting_to_loading_frame_count_ratio);
    }
    animation_state_ = animation_state;
    has_changed = true;
  }

  if (animation_state_ != ANIMATION_NONE) {
    animation_frame_ = (animation_frame_ + 1) %
                       ((animation_state_ == ANIMATION_WAITING) ?
                         data_->waiting_animation_frame_count :
                         data_->loading_animation_frame_count);
    has_changed = true;
  } else {
    animation_frame_ = 0;
  }
  return has_changed;
}

void TabRendererGtk::LoadingAnimation::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  DCHECK(type == NotificationType::BROWSER_THEME_CHANGED);
  data_.reset(new Data(theme_provider_));
}

////////////////////////////////////////////////////////////////////////////////
// FaviconCrashAnimation
//
//  A custom animation subclass to manage the favicon crash animation.
class TabRendererGtk::FavIconCrashAnimation : public ui::LinearAnimation,
                                              public ui::AnimationDelegate {
 public:
  explicit FavIconCrashAnimation(TabRendererGtk* target)
      : ALLOW_THIS_IN_INITIALIZER_LIST(ui::LinearAnimation(1000, 25, this)),
        target_(target) {
  }
  virtual ~FavIconCrashAnimation() {}

  // ui::Animation overrides:
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

  // ui::AnimationDelegate overrides:
  virtual void AnimationCanceled(const ui::Animation* animation) {
    target_->SetFavIconHidingOffset(0);
  }

 private:
  TabRendererGtk* target_;

  DISALLOW_COPY_AND_ASSIGN(FavIconCrashAnimation);
};

////////////////////////////////////////////////////////////////////////////////
// TabRendererGtk, public:

TabRendererGtk::TabRendererGtk(ui::ThemeProvider* theme_provider)
    : showing_icon_(false),
      showing_close_button_(false),
      fav_icon_hiding_offset_(0),
      should_display_crashed_favicon_(false),
      loading_animation_(theme_provider),
      background_offset_x_(0),
      background_offset_y_(kInactiveTabBackgroundOffsetY),
      close_button_color_(0) {
  InitResources();

  tab_.Own(gtk_fixed_new());
  gtk_widget_set_app_paintable(tab_.get(), TRUE);
  g_signal_connect(tab_.get(), "expose-event",
                   G_CALLBACK(OnExposeEventThunk), this);
  g_signal_connect(tab_.get(), "size-allocate",
                   G_CALLBACK(OnSizeAllocateThunk), this);
  close_button_.reset(MakeCloseButton());
  gtk_widget_show(tab_.get());

  hover_animation_.reset(new ui::SlideAnimation(this));
  hover_animation_->SetSlideDuration(kHoverDurationMs);

  registrar_.Add(this, NotificationType::BROWSER_THEME_CHANGED,
                 NotificationService::AllSources());
}

TabRendererGtk::~TabRendererGtk() {
  tab_.Destroy();
  for (BitmapCache::iterator it = cached_bitmaps_.begin();
       it != cached_bitmaps_.end(); ++it) {
    delete it->second.bitmap;
  }
}

void TabRendererGtk::UpdateData(TabContents* contents,
                                bool app,
                                bool loading_only) {
  DCHECK(contents);
  theme_provider_ = GtkThemeProvider::GetFrom(contents->profile());

  if (!loading_only) {
    data_.title = contents->GetTitle();
    data_.incognito = contents->profile()->IsOffTheRecord();
    data_.crashed = contents->is_crashed();

    SkBitmap* app_icon = contents->GetExtensionAppIcon();
    if (app_icon)
      data_.favicon = *app_icon;
    else
      data_.favicon = contents->GetFavicon();

    data_.app = app;
    // This is kind of a hacky way to determine whether our icon is the default
    // favicon. But the plumbing that would be necessary to do it right would
    // be a good bit of work and would sully code for other platforms which
    // don't care to custom-theme the favicon. Hopefully the default favicon
    // will eventually be chromium-themable and this code will go away.
    data_.is_default_favicon =
        (data_.favicon.pixelRef() ==
        ResourceBundle::GetSharedInstance().GetBitmapNamed(
            IDR_DEFAULT_FAVICON)->pixelRef());
  }

  // Loading state also involves whether we show the favicon, since that's where
  // we display the throbber.
  data_.loading = contents->is_loading();
  data_.show_icon = contents->ShouldDisplayFavIcon();
}

void TabRendererGtk::UpdateFromModel() {
  // Force a layout, since the tab may have grown a favicon.
  Layout();
  SchedulePaint();

  if (data_.crashed) {
    if (!should_display_crashed_favicon_ && !IsPerformingCrashAnimation())
      StartCrashAnimation();
  } else {
    if (IsPerformingCrashAnimation())
      StopCrashAnimation();
    ResetCrashedFavIcon();
  }
}

void TabRendererGtk::SetBlocked(bool blocked) {
  if (data_.blocked == blocked)
    return;
  data_.blocked = blocked;
  // TODO(zelidrag) bug 32399: Make tabs pulse on Linux as well.
}

bool TabRendererGtk::is_blocked() const {
  return data_.blocked;
}

bool TabRendererGtk::IsSelected() const {
  return true;
}

bool TabRendererGtk::IsVisible() const {
  return GTK_WIDGET_FLAGS(tab_.get()) & GTK_VISIBLE;
}

void TabRendererGtk::SetVisible(bool visible) const {
  if (visible) {
    gtk_widget_show(tab_.get());
    if (data_.mini)
      gtk_widget_show(close_button_->widget());
  } else {
    gtk_widget_hide_all(tab_.get());
  }
}

bool TabRendererGtk::ValidateLoadingAnimation(AnimationState animation_state) {
  return loading_animation_.ValidateLoadingAnimation(animation_state);
}

void TabRendererGtk::PaintFavIconArea(GdkEventExpose* event) {
  DCHECK(ShouldShowIcon());

  // The paint area is the favicon bounds, but we're painting into the gdk
  // window belonging to the tabstrip.  So the coordinates are relative to the
  // top left of the tab strip.
  event->area.x = x() + favicon_bounds_.x();
  event->area.y = y() + favicon_bounds_.y();
  event->area.width = favicon_bounds_.width();
  event->area.height = favicon_bounds_.height();
  gfx::CanvasSkiaPaint canvas(event, false);

  // The actual paint methods expect 0, 0 to be the tab top left (see
  // PaintTab).
  canvas.TranslateInt(x(), y());

  // Paint the background behind the favicon.
  int theme_id;
  int offset_y = 0;
  if (IsSelected()) {
    theme_id = IDR_THEME_TOOLBAR;
  } else {
    if (!data_.incognito) {
      theme_id = IDR_THEME_TAB_BACKGROUND;
    } else {
      theme_id = IDR_THEME_TAB_BACKGROUND_INCOGNITO;
    }
    if (!theme_provider_->HasCustomImage(theme_id))
      offset_y = background_offset_y_;
  }
  SkBitmap* tab_bg = theme_provider_->GetBitmapNamed(theme_id);
  canvas.TileImageInt(*tab_bg,
      x() + favicon_bounds_.x(), offset_y + favicon_bounds_.y(),
      favicon_bounds_.x(), favicon_bounds_.y(),
      favicon_bounds_.width(), favicon_bounds_.height());

  if (!IsSelected()) {
    double throb_value = GetThrobValue();
    if (throb_value > 0) {
      SkRect bounds;
      bounds.set(favicon_bounds_.x(), favicon_bounds_.y(),
          favicon_bounds_.right(), favicon_bounds_.bottom());
      canvas.saveLayerAlpha(&bounds, static_cast<int>(throb_value * 0xff),
                            SkCanvas::kARGB_ClipLayer_SaveFlag);
      canvas.drawARGB(0, 255, 255, 255, SkXfermode::kClear_Mode);
      SkBitmap* active_bg = theme_provider_->GetBitmapNamed(IDR_THEME_TOOLBAR);
      canvas.TileImageInt(*active_bg,
          x() + favicon_bounds_.x(), favicon_bounds_.y(),
          favicon_bounds_.x(), favicon_bounds_.y(),
          favicon_bounds_.width(), favicon_bounds_.height());
      canvas.restore();
    }
  }

  // Now paint the icon.
  PaintIcon(&canvas);
}

bool TabRendererGtk::ShouldShowIcon() const {
  if (mini() && height() >= GetMinimumUnselectedSize().height()) {
    return true;
  } else if (!data_.show_icon) {
    return false;
  } else if (IsSelected()) {
    // The selected tab clips favicon before close button.
    return IconCapacity() >= 2;
  }
  // Non-selected tabs clip close button before favicon.
  return IconCapacity() >= 1;
}

// static
gfx::Size TabRendererGtk::GetMinimumUnselectedSize() {
  InitResources();

  gfx::Size minimum_size;
  minimum_size.set_width(kLeftPadding + kRightPadding);
  // Since we use bitmap images, the real minimum height of the image is
  // defined most accurately by the height of the end cap images.
  minimum_size.set_height(tab_active_.image_l->height() - kToolbarOverlap);
  return minimum_size;
}

// static
gfx::Size TabRendererGtk::GetMinimumSelectedSize() {
  gfx::Size minimum_size = GetMinimumUnselectedSize();
  minimum_size.set_width(kLeftPadding + kFavIconSize + kRightPadding);
  return minimum_size;
}

// static
gfx::Size TabRendererGtk::GetStandardSize() {
  gfx::Size standard_size = GetMinimumUnselectedSize();
  standard_size.Enlarge(kFavIconTitleSpacing + kStandardTitleWidth, 0);
  return standard_size;
}

// static
int TabRendererGtk::GetMiniWidth() {
  return browser_defaults::kMiniTabWidth;
}

// static
int TabRendererGtk::GetContentHeight() {
  // The height of the content of the Tab is the largest of the favicon,
  // the title text and the close button graphic.
  int content_height = std::max(kFavIconSize, title_font_height_);
  return std::max(content_height, close_button_height_);
}

// static
void TabRendererGtk::LoadTabImages() {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  tab_alpha_.image_l = rb.GetBitmapNamed(IDR_TAB_ALPHA_LEFT);
  tab_alpha_.image_r = rb.GetBitmapNamed(IDR_TAB_ALPHA_RIGHT);

  tab_active_.image_l = rb.GetBitmapNamed(IDR_TAB_ACTIVE_LEFT);
  tab_active_.image_c = rb.GetBitmapNamed(IDR_TAB_ACTIVE_CENTER);
  tab_active_.image_r = rb.GetBitmapNamed(IDR_TAB_ACTIVE_RIGHT);
  tab_active_.l_width = tab_active_.image_l->width();
  tab_active_.r_width = tab_active_.image_r->width();

  tab_inactive_.image_l = rb.GetBitmapNamed(IDR_TAB_INACTIVE_LEFT);
  tab_inactive_.image_c = rb.GetBitmapNamed(IDR_TAB_INACTIVE_CENTER);
  tab_inactive_.image_r = rb.GetBitmapNamed(IDR_TAB_INACTIVE_RIGHT);
  tab_inactive_.l_width = tab_inactive_.image_l->width();
  tab_inactive_.r_width = tab_inactive_.image_r->width();

  close_button_width_ = rb.GetBitmapNamed(IDR_TAB_CLOSE)->width();
  close_button_height_ = rb.GetBitmapNamed(IDR_TAB_CLOSE)->height();
}

// static
void TabRendererGtk::SetSelectedTitleColor(SkColor color) {
  selected_title_color_ = color;
}

// static
void TabRendererGtk::SetUnselectedTitleColor(SkColor color) {
  unselected_title_color_ = color;
}

gfx::Rect TabRendererGtk::GetNonMirroredBounds(GtkWidget* parent) const {
  // The tabstrip widget is a windowless widget so the tab widget's allocation
  // is relative to the browser titlebar.  We need the bounds relative to the
  // tabstrip.
  gfx::Rect bounds = GetWidgetBoundsRelativeToParent(parent, widget());
  bounds.set_x(gtk_util::MirroredLeftPointForRect(parent, bounds));
  return bounds;
}

gfx::Rect TabRendererGtk::GetRequisition() const {
  return gfx::Rect(requisition_.x(), requisition_.y(),
                   requisition_.width(), requisition_.height());
}

void TabRendererGtk::StartMiniTabTitleAnimation() {
  if (!mini_title_animation_.get()) {
    mini_title_animation_.reset(new ui::ThrobAnimation(this));
    mini_title_animation_->SetThrobDuration(kMiniTitleChangeThrobDuration);
  }

  if (!mini_title_animation_->is_animating()) {
    mini_title_animation_->StartThrobbing(2);
  } else if (mini_title_animation_->cycles_remaining() <= 2) {
    // The title changed while we're already animating. Add at most one more
    // cycle. This is done in an attempt to smooth out pages that continuously
    // change the title.
    mini_title_animation_->set_cycles_remaining(
        mini_title_animation_->cycles_remaining() + 2);
  }
}

void TabRendererGtk::StopMiniTabTitleAnimation() {
  if (mini_title_animation_.get())
    mini_title_animation_->Stop();
}

void TabRendererGtk::SetBounds(const gfx::Rect& bounds) {
  requisition_ = bounds;
  gtk_widget_set_size_request(tab_.get(), bounds.width(), bounds.height());
}

void TabRendererGtk::Observe(NotificationType type,
                             const NotificationSource& source,
                             const NotificationDetails& details) {
  DCHECK(type == NotificationType::BROWSER_THEME_CHANGED);

  // Clear our cache when we receive a theme change notification because it
  // contains cached bitmaps based off the previous theme.
  for (BitmapCache::iterator it = cached_bitmaps_.begin();
       it != cached_bitmaps_.end(); ++it) {
    delete it->second.bitmap;
  }
  cached_bitmaps_.clear();
}

////////////////////////////////////////////////////////////////////////////////
// TabRendererGtk, protected:

string16 TabRendererGtk::GetTitle() const {
  return data_.title;
}

///////////////////////////////////////////////////////////////////////////////
// TabRendererGtk, ui::AnimationDelegate implementation:

void TabRendererGtk::AnimationProgressed(const ui::Animation* animation) {
  gtk_widget_queue_draw(tab_.get());
}

void TabRendererGtk::AnimationCanceled(const ui::Animation* animation) {
  AnimationEnded(animation);
}

void TabRendererGtk::AnimationEnded(const ui::Animation* animation) {
  gtk_widget_queue_draw(tab_.get());
}

////////////////////////////////////////////////////////////////////////////////
// TabRendererGtk, private:

void TabRendererGtk::StartCrashAnimation() {
  if (!crash_animation_.get())
    crash_animation_.reset(new FavIconCrashAnimation(this));
  crash_animation_->Stop();
  crash_animation_->Start();
}

void TabRendererGtk::StopCrashAnimation() {
  if (!crash_animation_.get())
    return;
  crash_animation_->Stop();
}

bool TabRendererGtk::IsPerformingCrashAnimation() const {
  return crash_animation_.get() && crash_animation_->is_animating();
}

void TabRendererGtk::SetFavIconHidingOffset(int offset) {
  fav_icon_hiding_offset_ = offset;
  SchedulePaint();
}

void TabRendererGtk::DisplayCrashedFavIcon() {
  should_display_crashed_favicon_ = true;
}

void TabRendererGtk::ResetCrashedFavIcon() {
  should_display_crashed_favicon_ = false;
}

void TabRendererGtk::Paint(gfx::Canvas* canvas) {
  // Don't paint if we're narrower than we can render correctly. (This should
  // only happen during animations).
  if (width() < GetMinimumUnselectedSize().width() && !mini())
    return;

  // See if the model changes whether the icons should be painted.
  const bool show_icon = ShouldShowIcon();
  const bool show_close_button = ShouldShowCloseBox();
  if (show_icon != showing_icon_ ||
      show_close_button != showing_close_button_)
    Layout();

  PaintTabBackground(canvas);

  if (!mini() || width() > kMiniTabRendererAsNormalTabWidth)
    PaintTitle(canvas);

  if (show_icon)
    PaintIcon(canvas);
}

SkBitmap TabRendererGtk::PaintBitmap() {
  gfx::CanvasSkia canvas(width(), height(), false);
  Paint(&canvas);
  return canvas.ExtractBitmap();
}

cairo_surface_t* TabRendererGtk::PaintToSurface() {
  gfx::CanvasSkia canvas(width(), height(), false);
  Paint(&canvas);
  return cairo_surface_reference(cairo_get_target(canvas.beginPlatformPaint()));
}

void TabRendererGtk::SchedulePaint() {
  gtk_widget_queue_draw(tab_.get());
}

gfx::Rect TabRendererGtk::GetLocalBounds() {
  return gfx::Rect(0, 0, bounds_.width(), bounds_.height());
}

void TabRendererGtk::Layout() {
  gfx::Rect local_bounds = GetLocalBounds();
  if (local_bounds.IsEmpty())
    return;
  local_bounds.Inset(kLeftPadding, kTopPadding, kRightPadding, kBottomPadding);

  // Figure out who is tallest.
  int content_height = GetContentHeight();

  // Size the Favicon.
  showing_icon_ = ShouldShowIcon();
  if (showing_icon_) {
    int favicon_top = kTopPadding + (content_height - kFavIconSize) / 2;
    favicon_bounds_.SetRect(local_bounds.x(), favicon_top,
                            kFavIconSize, kFavIconSize);
    if ((mini() || data_.animating_mini_change) &&
        bounds_.width() < kMiniTabRendererAsNormalTabWidth) {
      int mini_delta = kMiniTabRendererAsNormalTabWidth - GetMiniWidth();
      int ideal_delta = bounds_.width() - GetMiniWidth();
      if (ideal_delta < mini_delta) {
        int ideal_x = (GetMiniWidth() - kFavIconSize) / 2;
        int x = favicon_bounds_.x() + static_cast<int>(
            (1 - static_cast<float>(ideal_delta) /
             static_cast<float>(mini_delta)) *
            (ideal_x - favicon_bounds_.x()));
        favicon_bounds_.set_x(x);
      }
    }
  } else {
    favicon_bounds_.SetRect(local_bounds.x(), local_bounds.y(), 0, 0);
  }

  // Size the Close button.
  showing_close_button_ = ShouldShowCloseBox();
  if (showing_close_button_) {
    int close_button_top =
        kTopPadding + kCloseButtonVertFuzz +
        (content_height - close_button_height_) / 2;
    close_button_bounds_.SetRect(local_bounds.width() + kCloseButtonHorzFuzz,
                                 close_button_top, close_button_width_,
                                 close_button_height_);

    // If the close button color has changed, generate a new one.
    if (theme_provider_) {
      SkColor tab_text_color =
        theme_provider_->GetColor(BrowserThemeProvider::COLOR_TAB_TEXT);
      if (!close_button_color_ || tab_text_color != close_button_color_) {
        close_button_color_ = tab_text_color;
        ResourceBundle& rb = ResourceBundle::GetSharedInstance();
        close_button_->SetBackground(close_button_color_,
            rb.GetBitmapNamed(IDR_TAB_CLOSE),
            rb.GetBitmapNamed(IDR_TAB_CLOSE_MASK));
      }
    }
  } else {
    close_button_bounds_.SetRect(0, 0, 0, 0);
  }

  if (!mini() || width() >= kMiniTabRendererAsNormalTabWidth) {
    // Size the Title text to fill the remaining space.
    int title_left = favicon_bounds_.right() + kFavIconTitleSpacing;
    int title_top = kTopPadding;

    // If the user has big fonts, the title will appear rendered too far down
    // on the y-axis if we use the regular top padding, so we need to adjust it
    // so that the text appears centered.
    gfx::Size minimum_size = GetMinimumUnselectedSize();
    int text_height = title_top + title_font_height_ + kBottomPadding;
    if (text_height > minimum_size.height())
      title_top -= (text_height - minimum_size.height()) / 2;

    int title_width;
    if (close_button_bounds_.width() && close_button_bounds_.height()) {
      title_width = std::max(close_button_bounds_.x() -
                             kTitleCloseButtonSpacing - title_left, 0);
    } else {
      title_width = std::max(local_bounds.width() - title_left, 0);
    }
    title_bounds_.SetRect(title_left, title_top, title_width, content_height);
  }

  favicon_bounds_.set_x(
      gtk_util::MirroredLeftPointForRect(tab_.get(), favicon_bounds_));
  close_button_bounds_.set_x(
      gtk_util::MirroredLeftPointForRect(tab_.get(), close_button_bounds_));
  title_bounds_.set_x(
      gtk_util::MirroredLeftPointForRect(tab_.get(), title_bounds_));

  MoveCloseButtonWidget();
}

void TabRendererGtk::MoveCloseButtonWidget() {
  if (!close_button_bounds_.IsEmpty()) {
    gtk_fixed_move(GTK_FIXED(tab_.get()), close_button_->widget(),
                   close_button_bounds_.x(), close_button_bounds_.y());
    gtk_widget_show(close_button_->widget());
  } else {
    gtk_widget_hide(close_button_->widget());
  }
}

SkBitmap* TabRendererGtk::GetMaskedBitmap(const SkBitmap* mask,
    const SkBitmap* background, int bg_offset_x, int bg_offset_y) {
  // We store a bitmap for each mask + background pair (4 total bitmaps).  We
  // replace the cached image if the tab has moved relative to the background.
  BitmapCache::iterator it = cached_bitmaps_.find(std::make_pair(mask,
                                                                 background));
  if (it != cached_bitmaps_.end()) {
    if (it->second.bg_offset_x == bg_offset_x &&
        it->second.bg_offset_y == bg_offset_y) {
      return it->second.bitmap;
    }
    // The background offset changed so we should re-render with the new
    // offsets.
    delete it->second.bitmap;
  }
  SkBitmap image = SkBitmapOperations::CreateTiledBitmap(
      *background, bg_offset_x, bg_offset_y, mask->width(),
      height() + kToolbarOverlap);
  CachedBitmap bitmap = {
    bg_offset_x,
    bg_offset_y,
    new SkBitmap(SkBitmapOperations::CreateMaskedBitmap(image, *mask))
  };
  cached_bitmaps_[std::make_pair(mask, background)] = bitmap;
  return bitmap.bitmap;
}

void TabRendererGtk::PaintTab(GdkEventExpose* event) {
  gfx::CanvasSkiaPaint canvas(event, false);
  if (canvas.is_empty())
    return;

  // The tab is rendered into a windowless widget whose offset is at the
  // coordinate event->area.  Translate by these offsets so we can render at
  // (0,0) to match Windows' rendering metrics.
  canvas.TranslateInt(event->area.x, event->area.y);

  // Save the original x offset so we can position background images properly.
  background_offset_x_ = event->area.x;

  Paint(&canvas);
}

void TabRendererGtk::PaintTitle(gfx::Canvas* canvas) {
  // Paint the Title.
  string16 title = data_.title;
  if (title.empty()) {
    title = data_.loading ?
        l10n_util::GetStringUTF16(IDS_TAB_LOADING_TITLE) :
        TabContentsWrapper::GetDefaultTitle();
  } else {
    Browser::FormatTitleForDisplay(&title);
  }

  SkColor title_color = IsSelected() ? selected_title_color_
                                     : unselected_title_color_;
  canvas->DrawStringInt(title, *title_font_, title_color,
                        title_bounds_.x(), title_bounds_.y(),
                        title_bounds_.width(), title_bounds_.height());
}

void TabRendererGtk::PaintIcon(gfx::Canvas* canvas) {
  if (loading_animation_.animation_state() != ANIMATION_NONE) {
    PaintLoadingAnimation(canvas);
  } else {
    canvas->Save();
    canvas->ClipRectInt(0, 0, width(), height() - kFavIconTitleSpacing);
    if (should_display_crashed_favicon_) {
      canvas->DrawBitmapInt(*crashed_fav_icon, 0, 0,
                            crashed_fav_icon->width(),
                            crashed_fav_icon->height(),
                            favicon_bounds_.x(),
                            favicon_bounds_.y() + fav_icon_hiding_offset_,
                            kFavIconSize, kFavIconSize,
                            true);
    } else {
      if (!data_.favicon.isNull()) {
        if (data_.is_default_favicon && theme_provider_->UseGtkTheme()) {
          GdkPixbuf* favicon = GtkThemeProvider::GetDefaultFavicon(true);
          canvas->AsCanvasSkia()->DrawGdkPixbuf(
              favicon, favicon_bounds_.x(),
              favicon_bounds_.y() + fav_icon_hiding_offset_);
        } else {
          // If the favicon is an app icon, it is allowed to be drawn slightly
          // larger than the standard favicon.
          int favIconHeightOffset = data_.app ? -2 : 0;
          int favIconWidthDelta = data_.app ?
              data_.favicon.width() - kFavIconSize : 0;
          int favIconHeightDelta = data_.app ?
              data_.favicon.height() - kFavIconSize : 0;

          // TODO(pkasting): Use code in tab_icon_view.cc:PaintIcon() (or switch
          // to using that class to render the favicon).
          canvas->DrawBitmapInt(data_.favicon, 0, 0,
                                data_.favicon.width(),
                                data_.favicon.height(),
                                favicon_bounds_.x() - favIconWidthDelta/2,
                                favicon_bounds_.y() + favIconHeightOffset
                                    - favIconHeightDelta/2
                                    + fav_icon_hiding_offset_,
                                kFavIconSize + favIconWidthDelta,
                                kFavIconSize + favIconHeightDelta,
                                true);
        }
      }
    }
    canvas->Restore();
  }
}

void TabRendererGtk::PaintTabBackground(gfx::Canvas* canvas) {
  if (IsSelected()) {
    PaintActiveTabBackground(canvas);
  } else {
    PaintInactiveTabBackground(canvas);

    double throb_value = GetThrobValue();
    if (throb_value > 0) {
      canvas->SaveLayerAlpha(static_cast<int>(throb_value * 0xff),
                             gfx::Rect(width(), height()));
      canvas->AsCanvasSkia()->drawARGB(0, 255, 255, 255,
                                       SkXfermode::kClear_Mode);
      PaintActiveTabBackground(canvas);
      canvas->Restore();
    }
  }
}

void TabRendererGtk::PaintInactiveTabBackground(gfx::Canvas* canvas) {

  // The tab image needs to be lined up with the background image
  // so that it feels partially transparent.
  int offset_x = background_offset_x_;

  int tab_id = data_.incognito ?
      IDR_THEME_TAB_BACKGROUND_INCOGNITO : IDR_THEME_TAB_BACKGROUND;

  SkBitmap* tab_bg = theme_provider_->GetBitmapNamed(tab_id);

  // If the theme is providing a custom background image, then its top edge
  // should be at the top of the tab. Otherwise, we assume that the background
  // image is a composited foreground + frame image.
  int offset_y = theme_provider_->HasCustomImage(tab_id) ?
      0 : background_offset_y_;

  // Draw left edge.
  SkBitmap* theme_l = GetMaskedBitmap(tab_alpha_.image_l, tab_bg, offset_x,
                                      offset_y);
  canvas->DrawBitmapInt(*theme_l, 0, 0);

  // Draw right edge.
  SkBitmap* theme_r = GetMaskedBitmap(tab_alpha_.image_r, tab_bg,
      offset_x + width() - tab_active_.r_width, offset_y);

  canvas->DrawBitmapInt(*theme_r, width() - theme_r->width(), 0);

  // Draw center.
  canvas->TileImageInt(*tab_bg,
      offset_x + tab_active_.l_width, kDropShadowOffset + offset_y,
      tab_active_.l_width, 2,
      width() - tab_active_.l_width - tab_active_.r_width, height() - 2);

  canvas->DrawBitmapInt(*tab_inactive_.image_l, 0, 0);
  canvas->TileImageInt(*tab_inactive_.image_c, tab_inactive_.l_width, 0,
      width() - tab_inactive_.l_width - tab_inactive_.r_width, height());
  canvas->DrawBitmapInt(*tab_inactive_.image_r,
      width() - tab_inactive_.r_width, 0);
}

void TabRendererGtk::PaintActiveTabBackground(gfx::Canvas* canvas) {
  int offset_x = background_offset_x_;

  SkBitmap* tab_bg = theme_provider_->GetBitmapNamed(IDR_THEME_TOOLBAR);

  // Draw left edge.
  SkBitmap* theme_l = GetMaskedBitmap(tab_alpha_.image_l, tab_bg, offset_x, 0);
  canvas->DrawBitmapInt(*theme_l, 0, 0);

  // Draw right edge.
  SkBitmap* theme_r = GetMaskedBitmap(tab_alpha_.image_r, tab_bg,
      offset_x + width() - tab_active_.r_width, 0);
  canvas->DrawBitmapInt(*theme_r, width() - tab_active_.r_width, 0);

  // Draw center.
  canvas->TileImageInt(*tab_bg,
      offset_x + tab_active_.l_width, kDropShadowHeight,
      tab_active_.l_width, kDropShadowHeight,
      width() - tab_active_.l_width - tab_active_.r_width,
      height() - kDropShadowHeight);

  canvas->DrawBitmapInt(*tab_active_.image_l, 0, 0);
  canvas->TileImageInt(*tab_active_.image_c, tab_active_.l_width, 0,
      width() - tab_active_.l_width - tab_active_.r_width, height());
  canvas->DrawBitmapInt(*tab_active_.image_r, width() - tab_active_.r_width, 0);
}

void TabRendererGtk::PaintLoadingAnimation(gfx::Canvas* canvas) {
  const SkBitmap* frames =
      (loading_animation_.animation_state() == ANIMATION_WAITING) ?
      loading_animation_.waiting_animation_frames() :
      loading_animation_.loading_animation_frames();
  const int image_size = frames->height();
  const int image_offset = loading_animation_.animation_frame() * image_size;
  DCHECK(image_size == favicon_bounds_.height());
  DCHECK(image_size == favicon_bounds_.width());

  // NOTE: the clipping is a work around for 69528, it shouldn't be necessary.
  canvas->Save();
  canvas->ClipRectInt(
      favicon_bounds_.x(), favicon_bounds_.y(), image_size, image_size);
  canvas->DrawBitmapInt(*frames, image_offset, 0, image_size, image_size,
      favicon_bounds_.x(), favicon_bounds_.y(), image_size, image_size,
      false);
  canvas->Restore();
}

int TabRendererGtk::IconCapacity() const {
  if (height() < GetMinimumUnselectedSize().height())
    return 0;
  return (width() - kLeftPadding - kRightPadding) / kFavIconSize;
}

bool TabRendererGtk::ShouldShowCloseBox() const {
  // The selected tab never clips close button.
  return !mini() && (IsSelected() || IconCapacity() >= 3);
}

CustomDrawButton* TabRendererGtk::MakeCloseButton() {
  CustomDrawButton* button = new CustomDrawButton(IDR_TAB_CLOSE,
      IDR_TAB_CLOSE_P, IDR_TAB_CLOSE_H, IDR_TAB_CLOSE);

  gtk_widget_set_tooltip_text(button->widget(),
      l10n_util::GetStringUTF8(IDS_TOOLTIP_CLOSE_TAB).c_str());

  g_signal_connect(button->widget(), "clicked",
                   G_CALLBACK(OnCloseButtonClickedThunk), this);
  g_signal_connect(button->widget(), "button-release-event",
                   G_CALLBACK(OnCloseButtonMouseReleaseThunk), this);
  g_signal_connect(button->widget(), "enter-notify-event",
                   G_CALLBACK(OnEnterNotifyEventThunk), this);
  g_signal_connect(button->widget(), "leave-notify-event",
                   G_CALLBACK(OnLeaveNotifyEventThunk), this);
  GTK_WIDGET_UNSET_FLAGS(button->widget(), GTK_CAN_FOCUS);
  gtk_fixed_put(GTK_FIXED(tab_.get()), button->widget(), 0, 0);

  return button;
}

double TabRendererGtk::GetThrobValue() {
  if (mini_title_animation_.get() && mini_title_animation_->is_animating()) {
    return mini_title_animation_->GetCurrentValue() *
        kMiniTitleChangeThrobOpacity;
  }
  return hover_animation_.get() ?
      kHoverOpacity * hover_animation_->GetCurrentValue() : 0;
}

void TabRendererGtk::CloseButtonClicked() {
  // Nothing to do.
}

void TabRendererGtk::OnCloseButtonClicked(GtkWidget* widget) {
  CloseButtonClicked();
}

gboolean TabRendererGtk::OnCloseButtonMouseRelease(GtkWidget* widget,
                                                   GdkEventButton* event) {
  if (event->button == 2) {
    CloseButtonClicked();
    return TRUE;
  }

  return FALSE;
}

gboolean TabRendererGtk::OnExposeEvent(GtkWidget* widget,
                                       GdkEventExpose* event) {
  PaintTab(event);
  gtk_container_propagate_expose(GTK_CONTAINER(tab_.get()),
                                 close_button_->widget(), event);
  return TRUE;
}

void TabRendererGtk::OnSizeAllocate(GtkWidget* widget,
                                    GtkAllocation* allocation) {
  gfx::Rect bounds = gfx::Rect(allocation->x, allocation->y,
                               allocation->width, allocation->height);

  // Nothing to do if the bounds are the same.  If we don't catch this, we'll
  // get an infinite loop of size-allocate signals.
  if (bounds_ == bounds)
    return;

  bounds_ = bounds;
  Layout();
}

gboolean TabRendererGtk::OnEnterNotifyEvent(GtkWidget* widget,
                                            GdkEventCrossing* event) {
  hover_animation_->SetTweenType(ui::Tween::EASE_OUT);
  hover_animation_->Show();
  return FALSE;
}

gboolean TabRendererGtk::OnLeaveNotifyEvent(GtkWidget* widget,
                                            GdkEventCrossing* event) {
  hover_animation_->SetTweenType(ui::Tween::EASE_IN);
  hover_animation_->Hide();
  return FALSE;
}

// static
void TabRendererGtk::InitResources() {
  if (initialized_)
    return;

  LoadTabImages();

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  const gfx::Font& base_font = rb.GetFont(ResourceBundle::BaseFont);
  title_font_ = new gfx::Font(base_font.GetFontName(), kFontPixelSize);
  title_font_height_ = title_font_->GetHeight();

  crashed_fav_icon = rb.GetBitmapNamed(IDR_SAD_FAVICON);

  initialized_ = true;
}
