// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DISPLAY_OUTPUT_CONFIGURATOR_H_
#define CHROMEOS_DISPLAY_OUTPUT_CONFIGURATOR_H_

#include <vector>

#include "base/basictypes.h"
#include "base/event_types.h"
#include "base/observer_list.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/timer.h"
#include "chromeos/chromeos_export.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

// Forward declarations for Xlib and Xrandr.
// This is so unused X definitions don't pollute the namespace.
typedef unsigned long XID;
typedef XID Window;
typedef XID RROutput;
typedef XID RRCrtc;
typedef XID RRMode;

struct _XDisplay;
typedef struct _XDisplay Display;
struct _XRROutputInfo;
typedef _XRROutputInfo XRROutputInfo;
struct _XRRScreenResources;
typedef _XRRScreenResources XRRScreenResources;

namespace chromeos {

struct OutputSnapshot;

// Used to describe the state of a multi-display configuration.
enum OutputState {
  STATE_INVALID,
  STATE_HEADLESS,
  STATE_SINGLE,
  STATE_DUAL_MIRROR,
  STATE_DUAL_EXTENDED,
  STATE_DUAL_UNKNOWN,
};

// Information that is necessary to construct display id
// in |OutputConfigurator::Delegate|.
// TODO(oshima): Move xrandr related functions to here
// from ui/base/x and replace this with display id list.
struct OutputInfo {
  RROutput output;
  int output_index;
};

// This class interacts directly with the underlying Xrandr API to manipulate
// CTRCs and Outputs.  It will likely grow more state, over time, or expose
// Output info in other ways as more of the Chrome display code grows up around
// it.
class CHROMEOS_EXPORT OutputConfigurator : public MessageLoop::Dispatcher {
 public:
  class Observer {
   public:
    // Called when the change of the display mode finished.  It will usually
    // start the fading in the displays.
    virtual void OnDisplayModeChanged() {}

    // Called when the change of the display mode is issued but failed.
    // |failed_new_state| is the new state which the system failed to enter.
    virtual void OnDisplayModeChangeFailed(OutputState failed_new_state) {}
  };

  class Delegate {
   public:
    // Called when displays are detected.
    virtual OutputState GetStateForOutputs(
        const std::vector<OutputInfo>& outputs) const = 0;
  };

  // Flags that can be passed to SetDisplayPower().
  static const int kSetDisplayPowerNoFlags                     = 0;
  // Configure displays even if the passed-in state matches |power_state_|.
  static const int kSetDisplayPowerForceProbe                  = 1 << 0;
  // Do not change the state if multiple displays are connected or if the
  // only connected display is external.
  static const int kSetDisplayPowerOnlyIfSingleInternalDisplay = 1 << 1;

  OutputConfigurator();
  virtual ~OutputConfigurator();

  int connected_output_count() const { return connected_output_count_; }

  OutputState output_state() const { return output_state_; }

  void set_display_power_state(DisplayPowerState power_state) {
    power_state_ = power_state;
  }
  DisplayPowerState display_power_state() const { return power_state_; }

  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

  // Initialization, must be called right after constructor.
  // |is_panel_fitting_enabled| indicates hardware panel fitting support.
  // If |background_color_argb| is non zero and there are multiple displays,
  // OutputConfigurator sets the background color of X's RootWindow to this
  // color.
  void Init(bool is_panel_fitting_enabled, uint32 background_color_argb);

  // Detects displays first time from unknown state.
  void Start();

  // Stop handling display configuration events/requests.
  void Stop();

  // Called when powerd notifies us that some set of displays should be turned
  // on or off.  This requires enabling or disabling the CRTC associated with
  // the display(s) in question so that the low power state is engaged.
  // |flags| contains bitwise-or-ed kSetDisplayPower* values.
  bool SetDisplayPower(DisplayPowerState power_state, int flags);

  // Force switching the display mode to |new_state|. Returns false if
  // it was called in a single-head or headless mode.
  bool SetDisplayMode(OutputState new_state);

  // Called when an RRNotify event is received.  The implementation is
  // interested in the cases of RRNotify events which correspond to output
  // add/remove events.  Note that Output add/remove events are sent in response
  // to our own reconfiguration operations so spurious events are common.
  // Spurious events will have no effect.
  virtual bool Dispatch(const base::NativeEvent& event) OVERRIDE;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Tells if the output specified by |name| is for internal display.
  static bool IsInternalOutputName(const std::string& name);

  // Sets all the displays into pre-suspend mode; usually this means
  // configure them for their resume state. This allows faster resume on
  // machines where display configuration is slow.
  void SuspendDisplays();

  // Reprobes displays to handle changes made while the system was
  // suspended.
  void ResumeDisplays();

 private:
  // Configure outputs.
  void ConfigureOutputs();

  // Fires OnDisplayModeChanged() event to the observers.
  void NotifyOnDisplayChanged();

  // Returns a vector filled with properties of the first two connected outputs
  // found on |display| and |screen|.
  std::vector<OutputSnapshot> GetDualOutputs(Display* display,
                                             XRRScreenResources* screen);

  // Looks for a mode on internal and external outputs having same resolution.
  // |display| and |screen| parameters are needed for some XRandR calls.
  // |internal_info| and |external_info| are used to search for the modes.
  // |internal_output_id| is used to create a new mode, if applicable.
  // |try_creating|=true will enable creating panel-fitting mode
  // on the |internal_info| output instead of
  // only searching for a matching mode. Note: it may lead to a crash,
  // if |internal_info| is not capable of panel fitting.
  // |preserve_aspect|=true will limit the search / creation
  // only to the modes having the native aspect ratio of |external_info|.
  // |internal_mirror_mode| and |external_mirror_mode| are the out-parameters
  // for the modes on the two outputs which will have the same resolution.
  // Returns false if no mode appropriate for mirroring has been found/created.
  bool FindOrCreateMirrorMode(Display* display,
                              XRRScreenResources* screen,
                              XRROutputInfo* internal_info,
                              XRROutputInfo* external_info,
                              RROutput internal_output_id,
                              bool try_creating,
                              bool preserve_aspect,
                              RRMode* internal_mirror_mode,
                              RRMode* external_mirror_mode);

  // Searches for touchscreens among input devices,
  // and tries to match them up to screens in |outputs|.
  // |display| and |screen| are used to make X calls.
  // |outputs| is an array of detected screens.
  // If a touchscreen with same resolution as an output's native mode
  // is detected, its id will be stored in this output.
  void GetTouchscreens(Display* display,
                       XRRScreenResources* screen,
                       std::vector<OutputSnapshot>& outputs);

  // Configures X to the state specified in |output_state| and
  // |power_state|.  |display|, |screen| and |window| are used to change X
  // configuration.  |outputs| contains information on the currently
  // configured state, as well as how to apply the new state.
  bool EnterState(Display* display,
                  XRRScreenResources* screen,
                  Window window,
                  OutputState output_state,
                  DisplayPowerState power_state,
                  const std::vector<OutputSnapshot>& outputs);

  // Outputs UMA metrics of previous state (the state that is being left).
  // Updates |mirror_mode_preserved_aspect_| and |last_enter_state_time_|.
  void RecordPreviousStateUMA();

  // Returns next state.
  OutputState GetNextState(Display* display,
                           XRRScreenResources* screen,
                           OutputState current_state,
                           const std::vector<OutputSnapshot>& outputs) const;


  // Tells if the output specified by |output_info| is for internal display.
  static bool IsInternalOutput(const XRROutputInfo* output_info);

  // Returns output's native mode, None if not found.
  static RRMode GetOutputNativeMode(const XRROutputInfo* output_info);

  Delegate* delegate_;

  // This is detected by the constructor to determine whether or not we should
  // be enabled.  If we aren't running on ChromeOS, we can't assume that the
  // Xrandr X11 extension is supported.
  // If this flag is set to false, any attempts to change the output
  // configuration to immediately fail without changing the state.
  bool configure_display_;

  // This is set externally in Init,
  // and is used to enable modes which rely on panel fitting.
  bool is_panel_fitting_enabled_;

  // The number of outputs that are connected.
  int connected_output_count_;

  // The base of the event numbers used to represent XRandr events used in
  // decoding events regarding output add/remove.
  int xrandr_event_base_;

  // The display state as derived from the outputs observed in |output_cache_|.
  // This is used for rotating display modes.
  OutputState output_state_;

  // The current power state as set via SetDisplayPower().
  DisplayPowerState power_state_;

  ObserverList<Observer> observers_;

  // The timer to delay configuring outputs. See also the comments in
  // |Dispatch()|.
  scoped_ptr<base::OneShotTimer<OutputConfigurator> > configure_timer_;

  // Next 3 members are used for UMA of time spent in various states.
  // Indicates that current OutputSnapshot has aspect preserving mirror mode.
  bool mirror_mode_will_preserve_aspect_;

  // Indicates that last entered mirror mode preserved aspect.
  bool mirror_mode_preserved_aspect_;

  // Indicates the time at which |output_state_| was entered.
  base::TimeTicks last_enter_state_time_;

  DISALLOW_COPY_AND_ASSIGN(OutputConfigurator);
};

}  // namespace chromeos

#endif  // CHROMEOS_DISPLAY_OUTPUT_CONFIGURATOR_H_
