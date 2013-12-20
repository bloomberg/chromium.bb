// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_CANDIDATE_WINDOW_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_CANDIDATE_WINDOW_CONTROLLER_IMPL_H_

#include "chrome/browser/chromeos/input_method/candidate_window_controller.h"

#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/input_method/candidate_window_view.h"
#include "chrome/browser/chromeos/input_method/infolist_window.h"
#include "ui/base/ime/chromeos/ibus_bridge.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class Widget;
}  // namespace views

namespace chromeos {
namespace input_method {

class CandidateWindow;
class DelayableWidget;
class ModeIndicatorController;

// The implementation of CandidateWindowController.
// CandidateWindowController controls the CandidateWindow.
class CandidateWindowControllerImpl
    : public CandidateWindowController,
      public CandidateWindowView::Observer,
      public views::WidgetObserver,
      public IBusPanelCandidateWindowHandlerInterface {
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
  // Returns infolist window position. This function handles right or bottom
  // position overflow. Usually, infolist window is clipped with right-top of
  // candidate window, but the position should be changed based on position
  // overflow. If right of infolist window is out of screen, infolist window is
  // shown left-top of candidate window. If bottom of infolist window is out of
  // screen, infolist window is shown with clipping to bottom of screen.
  // Infolist window does not overflow top and left direction.
  static gfx::Point GetInfolistWindowPosition(
      const gfx::Rect& candidate_window_rect,
      const gfx::Rect& screen_rect,
      const gfx::Size& infolist_winodw_size);

  // Converts |candidate_window| to infolist entry models. Sets
  // |has_highlighted| to true if infolist_entries contains highlighted entry.
  // TODO(mukai): move this method (and tests) to the new InfolistEntry model.
  static void ConvertLookupTableToInfolistEntry(
      const CandidateWindow& candidate_window,
      std::vector<InfolistEntry>* infolist_entries,
      bool* has_highlighted);

 private:
  // CandidateWindowView::Observer implementation.
  virtual void OnCandidateCommitted(int index) OVERRIDE;
  virtual void OnCandidateWindowOpened() OVERRIDE;
  virtual void OnCandidateWindowClosed() OVERRIDE;

  // views::WidgetObserver implementation.
  virtual void OnWidgetClosing(views::Widget* widget) OVERRIDE;

  // IBusPanelCandidateWindowHandlerInterface implementation.
  virtual void SetCursorBounds(const gfx::Rect& cursor_bounds,
                               const gfx::Rect& composition_head) OVERRIDE;
  virtual void UpdateAuxiliaryText(const std::string& utf8_text,
                                   bool visible) OVERRIDE;
  virtual void UpdateLookupTable(const CandidateWindow& candidate_window,
                                 bool visible) OVERRIDE;
  virtual void UpdatePreeditText(const std::string& utf8_text,
                                 unsigned int cursor, bool visible) OVERRIDE;
  virtual void FocusStateChanged(bool is_focused) OVERRIDE;

  // Creates the candidate window view.
  void CreateView();

  // Updates infolist bounds, if current bounds is up-to-date, this function
  // does nothing.
  gfx::Rect GetInfolistBounds();

  // The candidate window view.
  CandidateWindowView* candidate_window_view_;

  // This is the outer frame of the candidate window view. The frame will
  // own |candidate_window_|.
  scoped_ptr<views::Widget> frame_;

  // This is the outer frame of the infolist window view. Owned by the widget.
  InfolistWindow* infolist_window_;

  // This is the controller of the IME mode indicator.
  scoped_ptr<ModeIndicatorController> mode_indicator_controller_;

  // The infolist entries and its focused index which currently shown in
  // Infolist window.
  std::vector<InfolistEntry> latest_infolist_entries_;

  ObserverList<CandidateWindowController::Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(CandidateWindowControllerImpl);
};

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_CANDIDATE_WINDOW_CONTROLLER_IMPL_H_

}  // namespace input_method
}  // namespace chromeos
