// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_H_

#include <list>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/views/tabs/tab_renderer_data.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/base/layout.h"
#include "ui/gfx/point.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/glow_hover_controller.h"
#include "ui/views/view.h"

class TabController;

namespace gfx {
class Font;
}
namespace ui {
class Animation;
class AnimationContainer;
class LinearAnimation;
class MultiAnimation;
}
namespace views {
class ImageButton;
}

///////////////////////////////////////////////////////////////////////////////
//
//  A View that renders a Tab, either in a TabStrip or in a DraggedTabView.
//
///////////////////////////////////////////////////////////////////////////////
class Tab : public ui::AnimationDelegate,
            public views::ButtonListener,
            public views::ContextMenuController,
            public views::View {
 public:
  // The menu button's class name.
  static const char kViewClassName[];

  explicit Tab(TabController* controller);
  virtual ~Tab();

  TabController* controller() const { return controller_; }

  // Used to set/check whether this Tab is being animated closed.
  void set_closing(bool closing) { closing_ = closing; }
  bool closing() const { return closing_; }

  // See description above field.
  void set_dragging(bool dragging) { dragging_ = dragging; }
  bool dragging() const { return dragging_; }

  // Sets the container all animations run from.
  void set_animation_container(ui::AnimationContainer* container);

  // Set the theme provider - because we get detached, we are frequently
  // outside of a hierarchy with a theme provider at the top. This should be
  // called whenever we're detached or attached to a hierarchy.
  void set_theme_provider(ui::ThemeProvider* provider) {
    theme_provider_ = provider;
  }

  // Returns true if this tab is the active tab.
  bool IsActive() const;

  // Returns true if the tab is selected.
  bool IsSelected() const;

  // Sets the data this tabs displays. Invokes DataChanged.
  void SetData(const TabRendererData& data);
  const TabRendererData& data() const { return data_; }

  // Sets the network state. If the network state changes NetworkStateChanged is
  // invoked.
  void UpdateLoadingAnimation(TabRendererData::NetworkState state);

  // Starts/Stops a pulse animation.
  void StartPulse();
  void StopPulse();

  // Start/stop the mini-tab title animation.
  void StartMiniTabTitleAnimation();
  void StopMiniTabTitleAnimation();

  // Set the background offset used to match the image in the inactive tab
  // to the frame image.
  void set_background_offset(const gfx::Point& offset) {
    background_offset_ = offset;
  }

  // Recomputes the dominant color of the favicon, used in immersive mode.
  // TODO(jamescook): Remove this if UX agrees that we don't want colors in the
  // immersive mode light bar. crbug.com/166929
  void UpdateIconDominantColor();

  views::GlowHoverController* hover_controller() {
    return &hover_controller_;
  }

  // Returns the minimum possible size of a single unselected Tab.
  static gfx::Size GetMinimumUnselectedSize();
  // Returns the minimum possible size of a selected Tab. Selected tabs must
  // always show a close button and have a larger minimum size than unselected
  // tabs.
  static gfx::Size GetMinimumSelectedSize();
  // Returns the preferred size of a single Tab, assuming space is
  // available.
  static gfx::Size GetStandardSize();

  // Returns the width for touch tabs.
  static int GetTouchWidth();

  // Returns the width for mini-tabs. Mini-tabs always have this width.
  static int GetMiniWidth();

  // Returns the height for immersive mode tabs.
  static int GetImmersiveHeight();

 private:
  friend class TabTest;
   // The animation object used to swap the favicon with the sad tab icon.
  class FaviconCrashAnimation;
  class TabCloseButton;

  // Contains a cached image and the values used to generate it.
  struct ImageCacheEntry {
    ImageCacheEntry();
    ~ImageCacheEntry();

    // ID of the resource used.
    int resource_id;

    // Scale factor we're drawing it.
    ui::ScaleFactor scale_factor;

    // The image.
    gfx::ImageSkia image;
  };

  typedef std::list<ImageCacheEntry> ImageCache;

  // Overridden from ui::AnimationDelegate:
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;
  virtual void AnimationCanceled(const ui::Animation* animation) OVERRIDE;
  virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE;

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // Overridden from views::ContextMenuController:
  virtual void ShowContextMenuForView(views::View* source,
                                      const gfx::Point& point) OVERRIDE;

  // Overridden from views::View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void OnThemeChanged() OVERRIDE;
  virtual std::string GetClassName() const OVERRIDE;
  virtual bool HasHitTestMask() const OVERRIDE;
  virtual void GetHitTestMask(gfx::Path* path) const OVERRIDE;
  virtual bool GetTooltipText(const gfx::Point& p,
                              string16* tooltip) const OVERRIDE;
  virtual bool GetTooltipTextOrigin(const gfx::Point& p,
                                    gfx::Point* origin) const OVERRIDE;
  virtual ui::ThemeProvider* GetThemeProvider() const OVERRIDE;
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE;
  virtual bool OnMouseDragged(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseCaptureLost() OVERRIDE;
  virtual void OnMouseEntered(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseMoved(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const ui::MouseEvent& event) OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;

  // Overridden from ui::EventHandler:
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;

  // Returns the bounds of the title and icon.
  const gfx::Rect& GetTitleBounds() const;
  const gfx::Rect& GetIconBounds() const;

  // Invoked from SetData after |data_| has been updated to the new data.
  void DataChanged(const TabRendererData& old);

  // Paint with the normal tab style.
  void PaintTab(gfx::Canvas* canvas);

  // Paint with the "immersive mode" light-bar style.
  void PaintTabImmersive(gfx::Canvas* canvas);

  // Paint various portions of the Tab
  void PaintTabBackground(gfx::Canvas* canvas);
  void PaintInactiveTabBackgroundWithTitleChange(gfx::Canvas* canvas,
                                                 ui::MultiAnimation* animation);
  void PaintInactiveTabBackground(gfx::Canvas* canvas);
  void PaintInactiveTabBackgroundUsingResourceId(gfx::Canvas* canvas,
                                                 int tab_id);
  void PaintActiveTabBackground(gfx::Canvas* canvas);

  // Paints the icon at the specified coordinates, mirrored for RTL if needed.
  void PaintIcon(gfx::Canvas* canvas);
  void PaintTitle(gfx::Canvas* canvas, SkColor title_color);

  // Invoked if data_.network_state changes, or the network_state is not none.
  void AdvanceLoadingAnimation(TabRendererData::NetworkState old_state,
                               TabRendererData::NetworkState state);

  // Returns the number of favicon-size elements that can fit in the tab's
  // current size.
  int IconCapacity() const;

  // Returns whether the Tab should display a favicon.
  bool ShouldShowIcon() const;

  // Returns whether the Tab should display a close button.
  bool ShouldShowCloseBox() const;

  // Gets the throb value for the tab. When a tab is not selected the
  // active background is drawn at |GetThrobValue()|%. This is used for hover,
  // mini tab title change and pulsing.
  double GetThrobValue();

  // Set the temporary offset for the favicon. This is used during the crash
  // animation.
  void SetFaviconHidingOffset(int offset);

  void DisplayCrashedFavicon();
  void ResetCrashedFavicon();

  void StartCrashAnimation();
  void StopCrashAnimation();

  void StartRecordingAnimation();
  void StopRecordingAnimation();

  // Returns true if the crash animation is currently running.
  bool IsPerformingCrashAnimation() const;

  // Schedules repaint task for icon.
  void ScheduleIconPaint();

  // Performs a one-time initialization of static resources such as tab images.
  static void InitTabResources();

  // Returns the minimum possible size of a single unselected Tab, not
  // including considering touch mode.
  static gfx::Size GetBasicMinimumUnselectedSize();

  // Loads the images to be used for the tab background.
  static void LoadTabImages();

  // Returns the cached image for the specified arguments, or an empty image if
  // there isn't one cached.
  static gfx::ImageSkia GetCachedImage(int resource_id,
                                       const gfx::Size& size,
                                       ui::ScaleFactor scale_factor);

  // Caches the specified image.
  static void SetCachedImage(int resource_id,
                             ui::ScaleFactor scale_factor,
                             const gfx::ImageSkia& image);

  // The controller.
  // WARNING: this is null during detached tab dragging.
  TabController* controller_;

  TabRendererData data_;

  // True if the tab is being animated closed.
  bool closing_;

  // True if the tab is being dragged.
  bool dragging_;

  // The offset used to animate the favicon location. This is used when the tab
  // crashes.
  int favicon_hiding_offset_;

  // The current index of the loading animation.
  int loading_animation_frame_;

  bool should_display_crashed_favicon_;

  // The tab and the icon can both be animating. The tab 'throbs' by changing
  // color. The icon can have one of several of animations like crashing,
  // recording, projecting, etc. Note that the icon animation related to network
  // state does not have an animation associated with it.
  scoped_ptr<ui::Animation> tab_animation_;
  scoped_ptr<ui::LinearAnimation> icon_animation_;

  scoped_refptr<ui::AnimationContainer> animation_container_;

  views::ImageButton* close_button_;

  ui::ThemeProvider* theme_provider_;

  views::GlowHoverController hover_controller_;

  // The bounds of various sections of the display.
  gfx::Rect favicon_bounds_;
  gfx::Rect title_bounds_;

  // The offset used to paint the inactive background image.
  gfx::Point background_offset_;

  struct TabImage {
    gfx::ImageSkia* image_l;
    gfx::ImageSkia* image_c;
    gfx::ImageSkia* image_r;
    int l_width;
    int r_width;
    int y_offset;
  };
  static TabImage tab_active_;
  static TabImage tab_inactive_;
  static TabImage tab_alpha_;

  // Whether we're showing the icon. It is cached so that we can detect when it
  // changes and layout appropriately.
  bool showing_icon_;

  // Whether we are showing the close button. It is cached so that we can
  // detect when it changes and layout appropriately.
  bool showing_close_button_;

  // The current color of the close button.
  SkColor close_button_color_;

  // The dominant color of the favicon. Used in immersive mode. White until the
  // color is known so that tab has something visible to draw during page load.
  // TODO(jamescook): Remove this if UX agrees that we don't want colors in the
  // immersive mode light bar. crbug.com/166929
  SkColor icon_dominant_color_;

  static gfx::Font* font_;
  static int font_height_;

  // As the majority of the tabs are inactive, and painting tabs is slowish,
  // we cache a handful of the inactive tab backgrounds here.
  static ImageCache* image_cache_;

  DISALLOW_COPY_AND_ASSIGN(Tab);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_H_
