// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The header files provides APIs for monitoring and controlling input
// method UI status. The APIs encapsulate the APIs of IBus, the underlying
// input method framework.

#ifndef CHROMEOS_INPUT_METHOD_UI_H_
#define CHROMEOS_INPUT_METHOD_UI_H_

#include <base/basictypes.h>

#include <sstream>
#include <string>
#include <vector>

namespace chromeos {

// A key for attaching the |ibus_service_panel_| object to |ibus_|.
const char kPanelObjectKey[] = "panel-object";

// The struct represents the input method lookup table (list of candidates).
// Used for InputMethodUpdateLookupTableMonitorFunction.
struct InputMethodLookupTable {
  enum Orientation {
    kVertical,
    kHorizontal,
  };

  InputMethodLookupTable()
      : visible(false),
        cursor_absolute_index(0),
        page_size(0),
        orientation(kHorizontal) {
  }

  // Returns a string representation of the class. Used for debugging.
  // The function has to be defined here rather than in the .cc file.  If
  // it's defined in the .cc file, the code will be part of libcros.so,
  // which cannot be accessed from clients directly. libcros.so is loaded
  // by dlopen() so all functions are unbound unless explicitly bound by
  // dlsym().
  std::string ToString() const {
    std::stringstream stream;
    stream << "visible: " << visible << "\n";
    stream << "cursor_absolute_index: " << cursor_absolute_index << "\n";
    stream << "page_size: " << page_size << "\n";
    stream << "orientation: " << orientation << "\n";
    stream << "candidates:";
    for (size_t i = 0; i < candidates.size(); ++i) {
      stream << " [" << candidates[i] << "]";
    }
    stream << "\nlabels:";
    for (size_t i = 0; i < labels.size(); ++i) {
      stream << " [" << labels[i] << "]";
    }
    return stream.str();
  }

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
};

// Callback function type for handling IBus's |HideAuxiliaryText| signal.
typedef void (*InputMethodHideAuxiliaryTextMonitorFunction)(
    void* input_method_library);

// Callback function type for handling IBus's |HideLookupTable| signal.
typedef void (*InputMethodHideLookupTableMonitorFunction)(
    void* input_method_library);

// Callback function type for handling IBus's |SetCandidateText| signal.
typedef void (*InputMethodSetCursorLocationMonitorFunction)(
    void* input_method_library,
    int x, int y, int width, int height);

// Callback function type for handling IBus's |UpdateAuxiliaryText| signal.
typedef void (*InputMethodUpdateAuxiliaryTextMonitorFunction)(
    void* input_method_library,
    const std::string& text,
    bool visible);

// Callback function type for handling IBus's |UpdateLookupTable| signal.
typedef void (*InputMethodUpdateLookupTableMonitorFunction)(
    void* input_method_library,
    const InputMethodLookupTable& table);

// A monitor function which is called when ibus connects or disconnects.
typedef void(*InputMethodConnectionChangeMonitorFunction)(
    void* input_method_library, bool connected);

// A set of function pointers used for monitoring the input method UI status.
struct InputMethodUiStatusMonitorFunctions {
  InputMethodUiStatusMonitorFunctions()
      : hide_auxiliary_text(NULL),
        hide_lookup_table(NULL),
        set_cursor_location(NULL),
        update_auxiliary_text(NULL),
        update_lookup_table(NULL) {
  }

  InputMethodHideAuxiliaryTextMonitorFunction hide_auxiliary_text;
  InputMethodHideLookupTableMonitorFunction hide_lookup_table;
  InputMethodSetCursorLocationMonitorFunction set_cursor_location;
  InputMethodUpdateAuxiliaryTextMonitorFunction update_auxiliary_text;
  InputMethodUpdateLookupTableMonitorFunction update_lookup_table;
};

// Establishes IBus connection to the ibus-daemon.
//
// Returns an InputMethodUiStatusConnection object that is used for
// maintaining and monitoring an IBus connection. The implementation
// details of InputMethodUiStatusConnection is not exposed.
//
// Function pointers in |monitor_functions| are registered in the returned
// InputMethodUiStatusConnection object. These functions will be called,
// unless the pointers are NULL, when certain signals are received from
// ibus-daemon.
//
// The client can pass a pointer to an abitrary object as
// |input_method_library|. The pointer passed as |input_method_library|
// will be passed to the registered callback functions as the first
// parameter.
class InputMethodUiStatusConnection;
extern InputMethodUiStatusConnection* (*MonitorInputMethodUiStatus)(
    const InputMethodUiStatusMonitorFunctions& monitor_functions,
    void* input_method_library);

// Disconnects the input method UI status connection, as well as the
// underlying IBus connection.
extern void (*DisconnectInputMethodUiStatus)(
    InputMethodUiStatusConnection* connection);

// Notifies that a candidate is clicked. |CandidateClicked| signal will be
// sent to the ibus-daemon.
//
// - |index| Index in the Lookup table. The semantics is same with
//   |cursor_absolute_index|.
// - |button| GdkEventButton::button (1: left button, etc.)
// - |state|  GdkEventButton::state (key modifier flags)
extern void (*NotifyCandidateClicked)(
    InputMethodUiStatusConnection* connection,
    int index, int button, int flags);

// Notifies that the cursor up button is clicked. |CursorUp| signal will be
// sent to the ibus-daemon
extern void (*NotifyCursorUp)(
    InputMethodUiStatusConnection* connection);

// Notifies that the cursor down button is clicked. |CursorDown| signal will be
// sent to the ibus-daemon
extern void (*NotifyCursorDown)(
    InputMethodUiStatusConnection* connection);

// Notifies that the page up button is clicked. |PageUp| signal will be
// sent to the ibus-daemon
extern void (*NotifyPageUp)(
    InputMethodUiStatusConnection* connection);

// Notifies that the page down button is clicked. |PageDown| signal will be
// sent to the ibus-daemon
extern void (*NotifyPageDown)(
    InputMethodUiStatusConnection* connection);

// Set a notification function for changes to an ibus connection.
extern void (*MonitorInputMethodConnection)(
    InputMethodUiStatusConnection* connection,
    InputMethodConnectionChangeMonitorFunction connection_change_handler);

}  // namespace chromeos

#endif  // CHROMEOS_INPUT_METHOD_UI_H_
