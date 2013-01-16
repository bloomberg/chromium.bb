// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_CANDIDATE_WINDOW_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_CANDIDATE_WINDOW_CONTROLLER_IMPL_H_

#include "chrome/browser/chromeos/input_method/candidate_window_controller.h"

#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/input_method/candidate_window_view.h"
#include "chrome/browser/chromeos/input_method/ibus_controller.h"
#include "chrome/browser/chromeos/input_method/infolist_window_view.h"

namespace views {
class Widget;
}  // namespace views

namespace chromeos {
// TODO(nona): remove ibus namespace
namespace ibus {
class IBusLookupTable;
}  // namespace ibus
namespace input_method {

class DelayableWidget;

// The implementation of CandidateWindowController.
// CandidateWindowController controls the CandidateWindow.
class CandidateWindowControllerImpl : public CandidateWindowController,
                                      public CandidateWindowView::Observer,
                                      public IBusUiController::Observer,
                                      public IBusController::Observer {
 public:
  CandidateWindowControllerImpl();
  virtual ~CandidateWindowControllerImpl();

  // CandidateWindowController overrides:
  virtual bool Init(IBusController* controller) OVERRIDE;
  virtual void Shutdown(IBusController* controller) OVERRIDE;
  virtual void AddObserver(
      CandidateWindowController::Observer* observer) OVERRIDE;
  virtual void RemoveObserver(
      CandidateWindowController::Observer* observer) OVERRIDE;

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

  // Converts |lookup_table| to infolist entries. |focused_index| become
  // InfolistWindowView::InvalidFocusIndex if there is no selected entries.
  static void ConvertLookupTableToInfolistEntry(
      const ibus::IBusLookupTable& lookup_table,
      std::vector<InfolistWindowView::Entry>* infolist_entries,
      size_t* focused_index);

  // Returns true if given |new_entries| is different from |old_entries| and
  // should update current window.
  static bool ShouldUpdateInfolist(
      const std::vector<InfolistWindowView::Entry>& old_entries,
      size_t old_focused_index,
      const std::vector<InfolistWindowView::Entry>& new_entries,
      size_t new_focused_index);

 private:
  // CandidateWindowView::Observer implementation.
  virtual void OnCandidateCommitted(int index, int button, int flags) OVERRIDE;
  virtual void OnCandidateWindowOpened() OVERRIDE;
  virtual void OnCandidateWindowClosed() OVERRIDE;

  // Creates the candidate window view.
  void CreateView();

  // IBusUiController::Observer overrides.
  virtual void OnHideAuxiliaryText() OVERRIDE;
  virtual void OnHideLookupTable() OVERRIDE;
  virtual void OnHidePreeditText() OVERRIDE;
  virtual void OnSetCursorLocation(const gfx::Rect& cursor_position,
                                   const gfx::Rect& composition_head) OVERRIDE;
  virtual void OnUpdateAuxiliaryText(const std::string& utf8_text,
                                     bool visible) OVERRIDE;
  virtual void OnUpdateLookupTable(
      const ibus::IBusLookupTable& lookup_table,
      bool visible) OVERRIDE;
  virtual void OnUpdatePreeditText(const std::string& utf8_text,
                                   unsigned int cursor, bool visible) OVERRIDE;

  // IBusController::Observer override
  virtual void PropertyChanged() OVERRIDE;
  virtual void OnConnected() OVERRIDE;
  virtual void OnDisconnected() OVERRIDE;

  // Updates infolist bounds, if current bounds is up-to-date, this function
  // does nothing.
  void UpdateInfolistBounds();

  // The controller is used for communicating with the IBus daemon.
  scoped_ptr<IBusUiController> ibus_ui_controller_;

  // The candidate window view.
  CandidateWindowView* candidate_window_;

  // This is the outer frame of the candidate window view. The frame will
  // own |candidate_window_|.
  scoped_ptr<views::Widget> frame_;

  // This is the outer frame of the infolist window view. The frame will
  // own |infolist_window_|.
  scoped_ptr<DelayableWidget> infolist_window_;

  // The infolist entries and its focused index which currently shown in
  // Infolist window.
  std::vector<InfolistWindowView::Entry> latest_infolist_entries_;
  size_t latest_infolist_focused_index_;

  ObserverList<CandidateWindowController::Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(CandidateWindowControllerImpl);
};

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_CANDIDATE_WINDOW_CONTROLLER_IMPL_H_

}  // namespace input_method
}  // namespace chromeos
