// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_TABS_TAB_RENDERER_GTK_H_
#define CHROME_BROWSER_UI_GTK_TABS_TAB_RENDERER_GTK_H_
#pragma once

#include <gtk/gtk.h>
#include <map>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "chrome/browser/ui/gtk/owned_widget_gtk.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/rect.h"

namespace gfx {
class Size;
}  // namespace gfx

class CustomDrawButton;
class GtkThemeProvider;
class TabContents;

namespace ui {
class SlideAnimation;
class ThemeProvider;
class ThrobAnimation;
}

class TabRendererGtk : public ui::AnimationDelegate,
                       public NotificationObserver {
 public:
  // Possible animation states.
  enum AnimationState {
    ANIMATION_NONE,
    ANIMATION_WAITING,
    ANIMATION_LOADING
  };

  class LoadingAnimation : public NotificationObserver {
   public:
    struct Data {
      explicit Data(ui::ThemeProvider* theme_provider);
      Data(int loading, int waiting, int waiting_to_loading);

      SkBitmap* waiting_animation_frames;
      SkBitmap* loading_animation_frames;
      int loading_animation_frame_count;
      int waiting_animation_frame_count;
      int waiting_to_loading_frame_count_ratio;
    };

    explicit LoadingAnimation(ui::ThemeProvider* theme_provider);

    // Used in unit tests to inject specific data.
    explicit LoadingAnimation(const LoadingAnimation::Data& data);

    virtual ~LoadingAnimation();

    // Advance the loading animation to the next frame, or hide the animation if
    // the tab isn't loading. Returns |true| if the icon area needs to be
    // repainted.
    bool ValidateLoadingAnimation(AnimationState animation_state);

    AnimationState animation_state() const { return animation_state_; }
    int animation_frame() const { return animation_frame_; }

    const SkBitmap* waiting_animation_frames() const {
      return data_->waiting_animation_frames;
    }
    const SkBitmap* loading_animation_frames() const {
      return data_->loading_animation_frames;
    }

    // Provide NotificationObserver implementation.
    virtual void Observe(NotificationType type,
                         const NotificationSource& source,
                         const NotificationDetails& details);

   private:
    scoped_ptr<Data> data_;

    // Used to listen for theme change notifications.
    NotificationRegistrar registrar_;

    // Gives us our throbber images.
    ui::ThemeProvider* theme_provider_;

    // Current state of the animation.
    AnimationState animation_state_;

    // The current index into the Animation image strip.
    int animation_frame_;

    DISALLOW_COPY_AND_ASSIGN(LoadingAnimation);
  };

  explicit TabRendererGtk(ui::ThemeProvider* theme_provider);
  virtual ~TabRendererGtk();

  // TabContents. If only the loading state was updated, the loading_only flag
  // should be specified. If other things change, set this flag to false to
  // update everything.
  virtual void UpdateData(TabContents* contents, bool app, bool loading_only);

  // Sets the blocked state of the tab.
  void SetBlocked(bool pinned);
  bool is_blocked() const;

  // Sets the mini-state of the tab.
  void set_mini(bool mini) { data_.mini = mini; }
  bool mini() const { return data_.mini; }

  // Sets the app state of the tab.
  void set_app(bool app) { data_.app = app; }
  bool app() const { return data_.app; }

  // Are we in the process of animating a mini tab state change on this tab?
  void set_animating_mini_change(bool value) {
    data_.animating_mini_change = value;
  }

  // Updates the display to reflect the contents of this TabRenderer's model.
  void UpdateFromModel();

  // Returns true if the Tab is selected, false otherwise.
  virtual bool IsSelected() const;

  // Returns true if the Tab is visible, false otherwise.
  virtual bool IsVisible() const;

  // Sets the visibility of the Tab.
  virtual void SetVisible(bool visible) const;

  // Paints the tab into |canvas|.
  virtual void Paint(gfx::Canvas* canvas);

  // Paints the tab into a SkBitmap.
  virtual SkBitmap PaintBitmap();

  // Paints the tab, and keeps the result server-side. The returned surface must
  // be freed with cairo_surface_destroy().
  virtual cairo_surface_t* PaintToSurface();

  // There is no PaintNow available, so the fastest we can do is schedule a
  // paint with the windowing system.
  virtual void SchedulePaint();

  // Notifies the Tab that the close button has been clicked.
  virtual void CloseButtonClicked();

  // Sets the bounds of the tab.
  virtual void SetBounds(const gfx::Rect& bounds);

  // Provide NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Advance the loading animation to the next frame, or hide the animation if
  // the tab isn't loading.  Returns |true| if the icon area needs to be
  // repainted.
  bool ValidateLoadingAnimation(AnimationState animation_state);

  // Repaint only the area of the tab that contains the favicon.
  void PaintFavIconArea(GdkEventExpose* event);

  // Returns whether the Tab should display a favicon.
  bool ShouldShowIcon() const;

  // Returns the minimum possible size of a single unselected Tab.
  static gfx::Size GetMinimumUnselectedSize();
  // Returns the minimum possible size of a selected Tab. Selected tabs must
  // always show a close button and have a larger minimum size than unselected
  // tabs.
  static gfx::Size GetMinimumSelectedSize();
  // Returns the preferred size of a single Tab, assuming space is
  // available.
  static gfx::Size GetStandardSize();

  // Returns the width for mini-tabs. Mini-tabs always have this width.
  static int GetMiniWidth();

  // Loads the images to be used for the tab background.
  static void LoadTabImages();

  // Sets the colors used for painting text on the tabs.
  static void SetSelectedTitleColor(SkColor color);
  static void SetUnselectedTitleColor(SkColor color);

  static gfx::Font* title_font() { return title_font_; }

  // Returns the bounds of the Tab.
  int x() const { return bounds_.x(); }
  int y() const { return bounds_.y(); }
  int width() const { return bounds_.width(); }
  int height() const { return bounds_.height(); }

  gfx::Rect bounds() const { return bounds_; }

  gfx::Rect favicon_bounds() const { return favicon_bounds_; }

  // Returns the non-mirrored (LTR) bounds of this tab.
  gfx::Rect GetNonMirroredBounds(GtkWidget* parent) const;

  // Returns the requested bounds of the tab.
  gfx::Rect GetRequisition() const;

  GtkWidget* widget() const { return tab_.get(); }

  // Start/stop the mini-tab title animation.
  void StartMiniTabTitleAnimation();
  void StopMiniTabTitleAnimation();

  void set_vertical_offset(int offset) { background_offset_y_ = offset; }

 protected:
  const gfx::Rect& title_bounds() const { return title_bounds_; }
  const gfx::Rect& close_button_bounds() const { return close_button_bounds_; }

  // Returns the title of the Tab.
  string16 GetTitle() const;

  // enter-notify-event handler that signals when the mouse enters the tab.
  CHROMEGTK_CALLBACK_1(TabRendererGtk, gboolean, OnEnterNotifyEvent,
                       GdkEventCrossing*);

  // leave-notify-event handler that signals when the mouse enters the tab.
  CHROMEGTK_CALLBACK_1(TabRendererGtk, gboolean, OnLeaveNotifyEvent,
                       GdkEventCrossing*);

 private:
  class FavIconCrashAnimation;

  // The data structure used to hold cached bitmaps.  We need to manually free
  // the bitmap in CachedBitmap when we remove it from |cached_bitmaps_|.  We
  // handle this when we replace images in the map and in the destructor.
  struct CachedBitmap {
    int bg_offset_x;
    int bg_offset_y;
    SkBitmap* bitmap;
  };
  typedef std::map<std::pair<const SkBitmap*, const SkBitmap*>, CachedBitmap>
      BitmapCache;

  // Model data. We store this here so that we don't need to ask the underlying
  // model, which is tricky since instances of this object can outlive the
  // corresponding objects in the underlying model.
  struct TabData {
    TabData()
        : is_default_favicon(false),
          loading(false),
          crashed(false),
          incognito(false),
          show_icon(true),
          mini(false),
          blocked(false),
          animating_mini_change(false),
          app(false) {
    }

    SkBitmap favicon;
    bool is_default_favicon;
    string16 title;
    bool loading;
    bool crashed;
    bool incognito;
    bool show_icon;
    bool mini;
    bool blocked;
    bool animating_mini_change;
    bool app;
  };

  // TODO(jhawkins): Move into TabResources class.
  struct TabImage {
    SkBitmap* image_l;
    SkBitmap* image_c;
    SkBitmap* image_r;
    int l_width;
    int r_width;
    int y_offset;
  };

  // Overridden from ui::AnimationDelegate:
  virtual void AnimationProgressed(const ui::Animation* animation);
  virtual void AnimationCanceled(const ui::Animation* animation);
  virtual void AnimationEnded(const ui::Animation* animation);

  // Starts/Stops the crash animation.
  void StartCrashAnimation();
  void StopCrashAnimation();

  // Return true if the crash animation is currently running.
  bool IsPerformingCrashAnimation() const;

  // Set the temporary offset for the favicon. This is used during animation.
  void SetFaviconHidingOffset(int offset);

  void DisplayCrashedFavIcon();
  void ResetCrashedFavIcon();

  // Generates the bounds for the interior items of the tab.
  void Layout();

  // Returns the local bounds of the tab.  This returns the rect
  // {0, 0, width(), height()} for now, as we don't yet support borders.
  gfx::Rect GetLocalBounds();

  // Moves the close button widget within the GtkFixed container.
  void MoveCloseButtonWidget();

  // Returns the largest of the favicon, title text, and the close button.
  static int GetContentHeight();

  // A helper method for generating the masked bitmaps used to draw the curved
  // edges of tabs.  We cache the generated bitmaps because they can take a
  // long time to compute.
  SkBitmap* GetMaskedBitmap(const SkBitmap* mask,
                            const SkBitmap* background,
                            int bg_offset_x,
                            int bg_offset_y);
  BitmapCache cached_bitmaps_;

  // Paints the tab, minus the close button.
  void PaintTab(GdkEventExpose* event);

  // Paint various portions of the Tab
  void PaintTitle(gfx::Canvas* canvas);
  void PaintIcon(gfx::Canvas* canvas);
  void PaintTabBackground(gfx::Canvas* canvas);
  void PaintInactiveTabBackground(gfx::Canvas* canvas);
  void PaintActiveTabBackground(gfx::Canvas* canvas);
  void PaintLoadingAnimation(gfx::Canvas* canvas);

  // Returns the number of favicon-size elements that can fit in the tab's
  // current size.
  int IconCapacity() const;


  // Returns whether the Tab should display a close button.
  bool ShouldShowCloseBox() const;

  CustomDrawButton* MakeCloseButton();

  // Gets the throb value for the tab. When a tab is not selected the
  // active background is drawn at |GetThrobValue()|%. This is used for hover
  // and mini-tab title change effects.
  double GetThrobValue();

  // Handles the clicked signal for the close button.
  CHROMEGTK_CALLBACK_0(TabRendererGtk, void, OnCloseButtonClicked);

  // Handles middle clicking the close button.
  CHROMEGTK_CALLBACK_1(TabRendererGtk, gboolean, OnCloseButtonMouseRelease,
                       GdkEventButton*);

  // expose-event handler that redraws the tab.
  CHROMEGTK_CALLBACK_1(TabRendererGtk, gboolean, OnExposeEvent,
                       GdkEventExpose*);

  // size-allocate handler used to update the current bounds of the tab.
  CHROMEGTK_CALLBACK_1(TabRendererGtk, void, OnSizeAllocate, GtkAllocation*);

  // TODO(jhawkins): Move to TabResources.
  static void InitResources();
  static bool initialized_;

  // The bounds of various sections of the display.
  gfx::Rect favicon_bounds_;
  gfx::Rect title_bounds_;
  gfx::Rect close_button_bounds_;

  TabData data_;

  static TabImage tab_active_;
  static TabImage tab_inactive_;
  static TabImage tab_alpha_;

  static gfx::Font* title_font_;
  static int title_font_height_;

  static int close_button_width_;
  static int close_button_height_;

  static SkColor selected_title_color_;
  static SkColor unselected_title_color_;

  // The GtkDrawingArea we draw the tab on.
  OwnedWidgetGtk tab_;

  // Whether we're showing the icon. It is cached so that we can detect when it
  // changes and layout appropriately.
  bool showing_icon_;

  // Whether we are showing the close button. It is cached so that we can
  // detect when it changes and layout appropriately.
  bool showing_close_button_;

  // The offset used to animate the favicon location.
  int fav_icon_hiding_offset_;

  // The animation object used to swap the favicon with the sad tab icon.
  scoped_ptr<FavIconCrashAnimation> crash_animation_;

  // Set when the crashed favicon should be displayed.
  bool should_display_crashed_favicon_;

  // The bounds of this Tab.
  gfx::Rect bounds_;

  // The requested bounds of this tab.  These bounds are relative to the
  // tabstrip.
  gfx::Rect requisition_;

  // Hover animation.
  scoped_ptr<ui::SlideAnimation> hover_animation_;

  // Animation used when the title of an inactive mini-tab changes.
  scoped_ptr<ui::ThrobAnimation> mini_title_animation_;

  // Contains the loading animation state.
  LoadingAnimation loading_animation_;

  // The offset used to paint the tab theme images.
  int background_offset_x_;

  // The vertical offset used to paint the tab theme images. Controlled by the
  // tabstrip and plumbed here to offset the theme image by the size of the
  // alignment in the BrowserTitlebar.
  int background_offset_y_;

  GtkThemeProvider* theme_provider_;

  // The close button.
  scoped_ptr<CustomDrawButton> close_button_;

  // The current color of the close button.
  SkColor close_button_color_;

  // Used to listen for theme change notifications.
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(TabRendererGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_TABS_TAB_RENDERER_GTK_H_
