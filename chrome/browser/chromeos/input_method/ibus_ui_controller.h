// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The header files provides APIs for monitoring and controlling input
// method UI status. The APIs encapsulate the APIs of IBus, the underlying
// input method framework.
// TODO(nona): Remove InputMethodLookupTable and IBusUiController.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_IBUS_UI_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_IBUS_UI_CONTROLLER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "chromeos/dbus/ibus/ibus_panel_service.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace chromeos {
namespace input_method {

// A key for attaching the |ibus_service_panel_| object to |ibus_|.
const char kPanelObjectKey[] = "panel-object";

class InputMethodDescriptor;
typedef std::vector<InputMethodDescriptor> InputMethodDescriptors;

// The struct represents the input method lookup table (list of candidates).
// Used for InputMethodUpdateLookupTableMonitorFunction.
struct InputMethodLookupTable {
  enum Orientation {
    kVertical,
    kHorizontal,
  };

  struct Description {
    std::string title;  // Description title string in UTF-8.
    std::string body;  // Description body string in UTF-8.
  };

  InputMethodLookupTable();

  ~InputMethodLookupTable();

  // Debug print function.
  std::string ToString() const;

  // True if the lookup table is visible.
  bool visible;

  // Zero-origin index of the current cursor position in the all
  // candidates. If the cursor is pointing to the third candidate in the
  // second page when the page size is 10, the value will be 12 as it's
  // 13th candidate.
  int cursor_absolute_index;

  // Page size is the max number of candidates shown in a page. Usually
  // it's about 10, depending on the backend conversion engine.
  int page_size;

  // True if the candidate window should be shown under the first character of
  // composition string.
  bool show_at_composition_head;

  // Candidate strings in UTF-8.
  std::vector<std::string> candidates;

  // The orientation of the candidates in the candidate window.
  Orientation orientation;

  // Label strings in UTF-8 (ex. "1", "2", "3", ...).
  std::vector<std::string> labels;

  // Annotation strings in UTF-8 (ex. "Hankaku Katakana").
  std::vector<std::string> annotations;

  // Description entries. This text is shown in Infolist window.
  std::vector<Description> descriptions;
};

// IBusUiController is used to interact with the IBus daemon.
class IBusUiController : public ibus::IBusPanelCandidateWindowHandlerInterface {
 public:
  class Observer {
   public:
    // Called when the auxiliary text becomes hidden.
    virtual void OnHideAuxiliaryText() = 0;

    // Called when the lookup table becomes hidden.
    virtual void OnHideLookupTable() = 0;

    // Called when the preedit text becomes hidden.
    virtual void OnHidePreeditText() = 0;

    // Called when the cursor location is set.
    virtual void OnSetCursorLocation(const gfx::Rect& cusor_location,
                                     const gfx::Rect& composition_head) = 0;

    // Called when the auxiliary text is updated.
    virtual void OnUpdateAuxiliaryText(const std::string& text,
                                       bool visible) = 0;

    // Called when the lookup table is updated.
    virtual void OnUpdateLookupTable(const InputMethodLookupTable& table) = 0;

    // Called when the preedit text is updated.
    virtual void OnUpdatePreeditText(const std::string& utf8_text,
                                     unsigned int cursor, bool visible) = 0;
  };

  IBusUiController();
  virtual ~IBusUiController();

  // Creates an instance of the class. The constructor is unused.
  static IBusUiController* Create();

  // Adds and removes observers for IBus UI notifications. Clients must be
  // sure to remove the observer before they go away. To capture the
  // initial connection change, you should add an observer before calling
  // Connect().
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Notifies that a candidate is clicked. |CandidateClicked| signal will be
  // sent to the ibus-daemon.
  //
  // - |index| Index in the Lookup table. The semantics is same with
  //   |cursor_absolute_index|.
  // - |button| GdkEventButton::button (1: left button, etc.)
  // - |state|  GdkEventButton::state (key modifier flags)
  void NotifyCandidateClicked(int index, int button, int flags);

  // Notifies that the cursor up button is clicked. |CursorUp| signal will be
  // sent to the ibus-daemon
  void NotifyCursorUp();

  // Notifies that the cursor down button is clicked. |CursorDown| signal
  // will be sent to the ibus-daemon
  void NotifyCursorDown();

  // Notifies that the page up button is clicked. |PageUp| signal will be
  // sent to the ibus-daemon
  void NotifyPageUp();

  // Notifies that the page down button is clicked. |PageDown| signal will be
  // sent to the ibus-daemon
  void NotifyPageDown();

  // Handles cursor location update event. This is originate from
  // SetCursorLocation method call, but we can bypass it on Chrome OS because
  // candidate window is integrated with Chrome.
  void SetCursorLocation(const gfx::Rect& cursor_location,
                         const gfx::Rect& composition_head);

 private:
  // IBusPanelHandlerInterface overrides.
  virtual void UpdateLookupTable(const ibus::IBusLookupTable& table,
                                 bool visible) OVERRIDE;
  virtual void HideLookupTable() OVERRIDE;
  virtual void UpdateAuxiliaryText(const std::string& text,
                                   bool visible) OVERRIDE;
  virtual void HideAuxiliaryText() OVERRIDE;
  virtual void UpdatePreeditText(const std::string& text, uint32 cursor_pos,
                                 bool visible) OVERRIDE;
  virtual void HidePreeditText() OVERRIDE;

  ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(IBusUiController);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_IBUS_UI_CONTROLLER_H_
