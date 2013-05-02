// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TOUCH_TOUCH_OBSERVER_HUD_H_
#define ASH_TOUCH_TOUCH_OBSERVER_HUD_H_

#include <map>

#include "ash/ash_export.h"
#include "ash/display/display_controller.h"
#include "ash/shell.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "ui/base/events/event_handler.h"
#include "ui/gfx/display_observer.h"
#include "ui/gfx/point.h"
#include "ui/views/widget/widget_observer.h"

#if defined(OS_CHROMEOS)
#include "chromeos/display/output_configurator.h"
#endif  // defined(OS_CHROMEOS)

namespace aura {
class Window;
}

namespace gfx {
class Display;
}

namespace views {
class Label;
class View;
class Widget;
}

namespace ash {
namespace internal {

class TouchHudCanvas;
class TouchLog;
class TouchPointView;

// An event filter which handles system level gesture events. Objects of this
// class manage their own lifetime.
class ASH_EXPORT TouchObserverHUD
    : public ui::EventHandler,
      public views::WidgetObserver,
      public gfx::DisplayObserver,
#if defined(OS_CHROMEOS)
      public chromeos::OutputConfigurator::Observer,
#endif  // defined(OS_CHROMEOS)
      public DisplayController::Observer {
 public:
  enum Mode {
    FULLSCREEN,
    REDUCED_SCALE,
    PROJECTION,
    INVISIBLE,
  };

  explicit TouchObserverHUD(aura::RootWindow* initial_root);

  // Returns the log of touch events as a dictionary mapping id of each display
  // to its touch log.
  static scoped_ptr<DictionaryValue> GetAllAsDictionary();

  // Changes the display mode (e.g. scale, visibility). Calling this repeatedly
  // cycles between a fixed number of display modes.
  void ChangeToNextMode();

  // Removes all existing touch points from the screen (only if the HUD is
  // visible).
  void Clear();

  // Returns log of touch events as a list value. Each item in the list is a
  // trace of one touch point.
  scoped_ptr<ListValue> GetLogAsList() const;

  Mode mode() const { return mode_; }

 private:
  friend class TouchHudTest;

  virtual ~TouchObserverHUD();

  void SetMode(Mode mode);

  void UpdateTouchPointLabel(int index);

  // Overriden from ui::EventHandler:
  virtual void OnTouchEvent(ui::TouchEvent* event) OVERRIDE;

  // Overridden from views::WidgetObserver:
  virtual void OnWidgetDestroying(views::Widget* widget) OVERRIDE;

  // Overridden from gfx::DisplayObserver:
  virtual void OnDisplayBoundsChanged(const gfx::Display& display) OVERRIDE;
  virtual void OnDisplayAdded(const gfx::Display& new_display) OVERRIDE;
  virtual void OnDisplayRemoved(const gfx::Display& old_display) OVERRIDE;

#if defined(OS_CHROMEOS)
  // Overriden from chromeos::OutputConfigurator::Observer:
  virtual void OnDisplayModeChanged() OVERRIDE;
#endif  // defined(OS_CHROMEOS)

  // Overriden form DisplayController::Observer:
  virtual void OnDisplayConfigurationChanging() OVERRIDE;
  virtual void OnDisplayConfigurationChanged() OVERRIDE;

  static const int kMaxTouchPoints = 32;

  const int64 display_id_;
  aura::RootWindow* root_window_;

  Mode mode_;

  scoped_ptr<TouchLog> touch_log_;

  views::Widget* widget_;
  TouchHudCanvas* canvas_;
  std::map<int, TouchPointView*> points_;
  views::View* label_container_;
  views::Label* touch_labels_[kMaxTouchPoints];

  DISALLOW_COPY_AND_ASSIGN(TouchObserverHUD);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_TOUCH_TOUCH_OBSERVER_HUD_H_
