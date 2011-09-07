// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The header files provides APIs for monitoring and controlling input
// method UI status. The APIs encapsulate the APIs of IBus, the underlying
// input method framework.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_IBUS_UI_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_IBUS_UI_CONTROLLER_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "third_party/mozc/session/commands.pb.h"

namespace chromeos {
namespace input_method {

// A key for attaching the |ibus_service_panel_| object to |ibus_|.
const char kPanelObjectKey[] = "panel-object";

// The struct represents the input method lookup table (list of candidates).
// Used for InputMethodUpdateLookupTableMonitorFunction.
struct InputMethodLookupTable {
  enum Orientation {
    kVertical,
    kHorizontal,
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

  // Candidate strings in UTF-8.
  std::vector<std::string> candidates;

  // The orientation of the candidates in the candidate window.
  Orientation orientation;

  // Label strings in UTF-8 (ex. "1", "2", "3", ...).
  std::vector<std::string> labels;

  // Annotation strings in UTF-8 (ex. "Hankaku Katakana").
  std::vector<std::string> annotations;

  // Mozc candidates.
  mozc::commands::Candidates mozc_candidates;
};

// IBusUiController is used to interact with the IBus daemon.
class IBusUiController {
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
    virtual void OnSetCursorLocation(int x, int y, int width, int height) = 0;

    // Called when the auxiliary text is updated.
    virtual void OnUpdateAuxiliaryText(const std::string& text,
                                       bool visible) = 0;

    // Called when the lookup table is updated.
    virtual void OnUpdateLookupTable(const InputMethodLookupTable& table) = 0;

    // Called when the preedit text is updated.
    virtual void OnUpdatePreeditText(const std::string& utf8_text,
                                     unsigned int cursor, bool visible) = 0;

    // Called when the connection is changed. |connected| is true if the
    // connection is established, and false if the connection is closed.
    virtual void OnConnectionChange(bool connected) = 0;
  };

  // Creates an instance of the class. The constructor is unused.
  static IBusUiController* Create();

  virtual ~IBusUiController();

  // Connects to the IBus daemon.
  virtual void Connect() = 0;

  // Adds and removes observers for IBus UI notifications. Clients must be
  // sure to remove the observer before they go away. To capture the
  // initial connection change, you should add an observer before calling
  // Connect().
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Notifies that a candidate is clicked. |CandidateClicked| signal will be
  // sent to the ibus-daemon.
  //
  // - |index| Index in the Lookup table. The semantics is same with
  //   |cursor_absolute_index|.
  // - |button| GdkEventButton::button (1: left button, etc.)
  // - |state|  GdkEventButton::state (key modifier flags)
  virtual void NotifyCandidateClicked(int index, int button, int flags) = 0;

  // Notifies that the cursor up button is clicked. |CursorUp| signal will be
  // sent to the ibus-daemon
  virtual void NotifyCursorUp() = 0;

  // Notifies that the cursor down button is clicked. |CursorDown| signal
  // will be sent to the ibus-daemon
  virtual void NotifyCursorDown() = 0;

  // Notifies that the page up button is clicked. |PageUp| signal will be
  // sent to the ibus-daemon
  virtual void NotifyPageUp() = 0;

  // Notifies that the page down button is clicked. |PageDown| signal will be
  // sent to the ibus-daemon
  virtual void NotifyPageDown() = 0;
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_IBUS_UI_CONTROLLER_H_
