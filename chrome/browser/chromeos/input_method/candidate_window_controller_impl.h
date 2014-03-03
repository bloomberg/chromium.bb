// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_CANDIDATE_WINDOW_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_CANDIDATE_WINDOW_CONTROLLER_IMPL_H_

#include "chrome/browser/chromeos/input_method/candidate_window_controller.h"

#include "ash/ime/candidate_window_view.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "ui/base/ime/chromeos/ime_bridge.h"
#include "ui/base/ime/infolist_entry.h"
#include "ui/views/widget/widget_observer.h"

namespace ash {
namespace ime {
class InfolistWindow;
}  // namespace ime
}  // namespace ash

namespace ui {
class CandidateWindow;
}  // namespace ui

namespace views {
class Widget;
}  // namespace views

namespace chromeos {
namespace input_method {

class DelayableWidget;
class ModeIndicatorController;

// The implementation of CandidateWindowController.
// CandidateWindowController controls the CandidateWindow.
class CandidateWindowControllerImpl
    : public CandidateWindowController,
      public ash::ime::CandidateWindowView::Observer,
      public views::WidgetObserver,
      public IMECandidateWindowHandlerInterface {
 public:
  CandidateWindowControllerImpl();
  virtual ~CandidateWindowControllerImpl();

  // CandidateWindowController overrides:
  virtual void AddObserver(
      CandidateWindowController::Observer* observer) OVERRIDE;
  virtual void RemoveObserver(
      CandidateWindowController::Observer* observer) OVERRIDE;
  virtual void Hide() OVERRIDE;

 protected:
  static void ConvertLookupTableToInfolistEntry(
      const ui::CandidateWindow& candidate_window,
      std::vector<ui::InfolistEntry>* infolist_entries,
      bool* has_highlighted);

 private:
  // ash::ime::CandidateWindowView::Observer implementation.
  virtual void OnCandidateCommitted(int index) OVERRIDE;

  // views::WidgetObserver implementation.
  virtual void OnWidgetClosing(views::Widget* widget) OVERRIDE;

  // IMECandidateWindowHandlerInterface implementation.
  virtual void SetCursorBounds(const gfx::Rect& cursor_bounds,
                               const gfx::Rect& composition_head) OVERRIDE;
  virtual void UpdateLookupTable(
      const ui::CandidateWindow& candidate_window,
      bool visible) OVERRIDE;
  virtual void UpdatePreeditText(const base::string16& text,
                                 unsigned int cursor, bool visible) OVERRIDE;
  virtual void FocusStateChanged(bool is_focused) OVERRIDE;

  void InitCandidateWindowView();

  // The candidate window view.
  ash::ime::CandidateWindowView* candidate_window_view_;

  // This is the outer frame of the infolist window view. Owned by the widget.
  ash::ime::InfolistWindow* infolist_window_;

  gfx::Rect cursor_bounds_;
  gfx::Rect composition_head_;

  // This is the controller of the IME mode indicator.
  scoped_ptr<ModeIndicatorController> mode_indicator_controller_;

  // The infolist entries and its focused index which currently shown in
  // Infolist window.
  std::vector<ui::InfolistEntry> latest_infolist_entries_;

  ObserverList<CandidateWindowController::Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(CandidateWindowControllerImpl);
};

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_CANDIDATE_WINDOW_CONTROLLER_IMPL_H_

}  // namespace input_method
}  // namespace chromeos
