// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_MONITOR_OUTPUT_CONFIGURATOR_H_
#define CHROMEOS_MONITOR_OUTPUT_CONFIGURATOR_H_
#pragma once

#include "base/basictypes.h"
#include "base/event_types.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chromeos/chromeos_export.h"

// Forward declarations for Xlib and Xrandr.
// This is so unused X definitions don't pollute the namespace.
typedef unsigned long XID;
typedef XID Window;
typedef XID RROutput;
typedef XID RRCrtc;
typedef XID RRMode;

struct _XRRScreenResources;
typedef _XRRScreenResources XRRScreenResources;

namespace chromeos {

// The information we need to cache from an output to implement operations such
// as power state but also to eliminate duplicate operations within a given
// action (determining which CRTC to use for a given output, for example).
struct CachedOutputDescription {
  RROutput output;
  RRCrtc crtc;
  RRMode mirror_mode;
  RRMode ideal_mode;
  int x;
  int y;
  bool is_connected;
  bool is_powered_on;
  bool is_internal;
  unsigned long mm_width;
  unsigned long mm_height;
};

// Used to describe the state of a multi-monitor configuration.
enum State {
  STATE_INVALID,
  STATE_HEADLESS,
  STATE_SINGLE,
  STATE_DUAL_MIRROR,
  STATE_DUAL_PRIMARY_ONLY,
  STATE_DUAL_SECONDARY_ONLY,
};

// This class interacts directly with the underlying Xrandr API to manipulate
// CTRCs and Outputs.  It will likely grow more state, over time, or expose
// Output info in other ways as more of the Chrome display code grows up around
// it.
class CHROMEOS_EXPORT OutputConfigurator : public MessageLoop::Dispatcher {
 public:
  OutputConfigurator();
  virtual ~OutputConfigurator();

  State output_state() const { return output_state_; }

  // Called when the user hits ctrl-F4 to request a display mode change.
  // This method should only return false if it was called in a single-head or
  // headless mode.
  bool CycleDisplayMode();

  // Called when powerd notifies us that some set of displays should be turned
  // on or off.  This requires enabling or disabling the CRTC associated with
  // the display(s) in question so that the low power state is engaged.
  bool ScreenPowerSet(bool power_on, bool all_displays);

  // Force switching the display mode to |new_state|.  This method is used when
  // the user explicitly changes the display mode in the options UI.  Returns
  // false if it was called in a single-head or headless mode.
  bool SetDisplayMode(State new_state);

  // Called when an RRNotify event is received.  The implementation is
  // interested in the cases of RRNotify events which correspond to output
  // add/remove events.  Note that Output add/remove events are sent in response
  // to our own reconfiguration operations so spurious events are common.
  // Spurious events will have no effect.
  virtual bool Dispatch(const base::NativeEvent& event) OVERRIDE;

 private:
  // Updates |output_count_|, |output_cache_|, |mirror_supported_|,
  // |primary_output_index_|, and |secondary_output_index_| with new data.
  // Returns true if the update succeeded or false if it was skipped since no
  // actual change was observed.
  // Note that |output_state_| is not updated by this call.
  bool TryRecacheOutputs(Display* display, XRRScreenResources* screen);

  // Uses the data stored in |output_cache_| and the given |new_state| to
  // configure the Xrandr interface and then updates |output_state_| to reflect
  // the new state.
  void UpdateCacheAndXrandrToState(Display* display,
                                   XRRScreenResources* screen,
                                   Window window,
                                   State new_state);

  // A helper to re-cache instance variable state and transition into the
  // appropriate default state for the observed displays.
  bool RecacheAndUseDefaultState();

  // Checks the |primary_output_index_|, |secondary_output_index_|, and
  // |mirror_supported_| to see how many displays are currently connected and
  // returns the state which is most appropriate as a default state for those
  // displays.
  State GetDefaultState() const;

  // Called during start-up to determine what the current state of the displays
  // appears to be, by investigating how the outputs compare to the data stored
  // in |output_cache_|.  Returns STATE_INVALID if the current display state
  // doesn't match any supported state.  |output_cache_| must be up-to-date with
  // regards to the state of X or this method may return incorrect results.
  State InferCurrentState(Display* display, XRRScreenResources* screen) const;

  // Scans the |output_cache_| to determine whether or not we are in a
  // "projecting" state and then calls the DBus kSetIsProjectingMethod on powerd
  // with the result.
  void CheckIsProjectingAndNotify();

  // This is detected by the constructor to determine whether or not we should
  // be enabled.  If we aren't running on ChromeOS, we can't assume that the
  // Xrandr X11 extension is supported.
  // If this flag is set to false, any attempts to change the output
  // configuration to immediately fail without changing the state.
  bool is_running_on_chrome_os_;

  // The number of outputs in the output_cache_ array.
  int output_count_;

  // The list of cached output descriptions (|output_count_| elements long).
  scoped_array<CachedOutputDescription> output_cache_;

  // True if |output_cache_| describes a permutation of outputs which support a
  // mirrored device mode.
  bool mirror_supported_;

  // The index of the primary connected output in |output_cache_|.  -1 if there
  // is no primary output.  This implies the machine currently has no outputs.
  int primary_output_index_;

  // The index of the secondary connected output in |output_cache_|.  -1 if
  // there is no secondary output.  This implies the machine currently has one
  // output.
  int secondary_output_index_;

  // The base of the event numbers used to represent XRandr events used in
  // decoding events regarding output add/remove.
  int xrandr_event_base_;

  // The display state as derived from the outputs observed in |output_cache_|.
  // This is used for rotating display modes.
  State output_state_;

  DISALLOW_COPY_AND_ASSIGN(OutputConfigurator);
};

}  // namespace chromeos

#endif  // CHROMEOS_MONITOR_OUTPUT_CONFIGURATOR_H_
