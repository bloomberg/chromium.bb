// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_BASE_TAB_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_BASE_TAB_H_
#pragma once

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/ui/views/tabs/tab_renderer_data.h"
#include "ui/base/animation/animation_container.h"
#include "ui/base/animation/animation_delegate.h"
#include "views/controls/button/button.h"
#include "views/view.h"

class BaseTab;
class TabController;

namespace gfx {
class Font;
}

namespace ui {
class AnimationContainer;
class SlideAnimation;
class ThrobAnimation;
}

namespace views {
class ImageButton;
}

// Base class for tab renderers.
class BaseTab : public ui::AnimationDelegate,
                public views::ButtonListener,
                public views::ContextMenuController,
                public views::View {
 public:
  explicit BaseTab(TabController* controller);
  virtual ~BaseTab();

  // Sets the data this tabs displays. Invokes DataChanged for subclasses to
  // update themselves appropriately.
  void SetData(const TabRendererData& data);
  const TabRendererData& data() const { return data_; }

  // Sets the network state. If the network state changes NetworkStateChanged is
  // invoked.
  virtual void UpdateLoadingAnimation(TabRendererData::NetworkState state);

  // Starts/Stops a pulse animation.
  void StartPulse();
  void StopPulse();

  // Used to set/check whether this Tab is being animated closed.
  void set_closing(bool closing) { closing_ = closing; }
  bool closing() const { return closing_; }

  // See description above field.
  void set_dragging(bool dragging) { dragging_ = dragging; }
  bool dragging() const { return dragging_; }

  // Sets the container all animations run from.
  void set_animation_container(ui::AnimationContainer* container) {
    animation_container_ = container;
  }
  ui::AnimationContainer* animation_container() const {
    return animation_container_.get();
  }

  // Set the theme provider - because we get detached, we are frequently
  // outside of a hierarchy with a theme provider at the top. This should be
  // called whenever we're detached or attached to a hierarchy.
  void set_theme_provider(ui::ThemeProvider* provider) {
    theme_provider_ = provider;
  }

  // Returns true if the tab is selected.
  virtual bool IsSelected() const;

  // Returns true if the tab is closeable.
  bool IsCloseable() const;

  // views::View overrides:
  virtual void OnMouseEntered(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const views::MouseEvent& event) OVERRIDE;
  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE;
  virtual bool OnMouseDragged(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const views::MouseEvent& event,
                               bool canceled) OVERRIDE;
  virtual bool GetTooltipText(const gfx::Point& p,
                              std::wstring* tooltip) OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;
  virtual ThemeProvider* GetThemeProvider() const OVERRIDE;

 protected:
  // Invoked from SetData after |data_| has been updated to the new data.
  virtual void DataChanged(const TabRendererData& old) {}

  // Invoked if data_.network_state changes, or the network_state is not none.
  virtual void AdvanceLoadingAnimation(TabRendererData::NetworkState old_state,
                                       TabRendererData::NetworkState state);

  TabController* controller() const { return controller_; }

  // Returns the pulse animation. The pulse animation is non-null if StartPulse
  // has been invoked.
  ui::ThrobAnimation* pulse_animation() const { return pulse_animation_.get(); }

  // Returns the hover animation. This may return null.
  const ui::SlideAnimation* hover_animation() const {
    return hover_animation_.get();
  }

  views::ImageButton* close_button() const { return close_button_; }

  // Paints the icon at the specified coordinates, mirrored for RTL if needed.
  void PaintIcon(gfx::Canvas* canvas);
  void PaintTitle(gfx::Canvas* canvas, SkColor title_color);

  // Overridden from AnimationDelegate:
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;
  virtual void AnimationCanceled(const ui::Animation* animation) OVERRIDE;
  virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE;

  // views::ButtonListener overrides:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  // views::ContextMenuController overrides:
  virtual void ShowContextMenuForView(views::View* source,
                                      const gfx::Point& p,
                                      bool is_mouse_gesture) OVERRIDE;

  // Returns the bounds of the title and icon.
  virtual const gfx::Rect& GetTitleBounds() const = 0;
  virtual const gfx::Rect& GetIconBounds() const = 0;

  virtual int loading_animation_frame() const;
  virtual bool should_display_crashed_favicon() const;
  virtual int fav_icon_hiding_offset() const;

  static gfx::Font* font() { return font_; }
  static int font_height() { return font_height_; }

 private:
  // The animation object used to swap the favicon with the sad tab icon.
  class FavIconCrashAnimation;

  // Set the temporary offset for the favicon. This is used during the crash
  // animation.
  void SetFaviconHidingOffset(int offset);

  void DisplayCrashedFavIcon();
  void ResetCrashedFavIcon();

  // Starts/Stops the crash animation.
  void StartCrashAnimation();
  void StopCrashAnimation();

  // Returns true if the crash animation is currently running.
  bool IsPerformingCrashAnimation() const;

  // Schedules repaint task for icon.
  void ScheduleIconPaint();

  static void InitResources();

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
  int fav_icon_hiding_offset_;

  // The current index of the loading animation.
  int loading_animation_frame_;

  bool should_display_crashed_favicon_;

  // Pulse animation.
  scoped_ptr<ui::ThrobAnimation> pulse_animation_;

  // Hover animation.
  scoped_ptr<ui::SlideAnimation> hover_animation_;

  // Crash animation.
  scoped_ptr<FavIconCrashAnimation> crash_animation_;

  scoped_refptr<ui::AnimationContainer> animation_container_;

  views::ImageButton* close_button_;

  // Whether to disable throbber animations. Only true if this is an app tab
  // renderer and a command line flag has been passed in to disable the
  // animations.
  bool throbber_disabled_;

  ui::ThemeProvider* theme_provider_;

  static gfx::Font* font_;
  static int font_height_;

  DISALLOW_COPY_AND_ASSIGN(BaseTab);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_BASE_TAB_H_
