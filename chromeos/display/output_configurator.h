// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DISPLAY_OUTPUT_CONFIGURATOR_H_
#define CHROMEOS_DISPLAY_OUTPUT_CONFIGURATOR_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/event_types.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "chromeos/chromeos_export.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

// Forward declarations for Xlib and Xrandr.
// This is so unused X definitions don't pollute the namespace.
typedef unsigned long XID;
typedef XID RROutput;
typedef XID RRCrtc;
typedef XID RRMode;

namespace chromeos {

// Used to describe the state of a multi-display configuration.
enum OutputState {
  STATE_INVALID,
  STATE_HEADLESS,
  STATE_SINGLE,
  STATE_DUAL_MIRROR,
  STATE_DUAL_EXTENDED,
};

// This class interacts directly with the underlying Xrandr API to manipulate
// CTRCs and Outputs.
class CHROMEOS_EXPORT OutputConfigurator
    : public base::MessageLoop::Dispatcher,
      public base::MessagePumpObserver {
 public:
  struct CoordinateTransformation {
    // Initialized to the identity transformation.
    CoordinateTransformation();

    float x_scale;
    float x_offset;
    float y_scale;
    float y_offset;
  };

  // Information about an output's current state.
  struct OutputSnapshot {
    OutputSnapshot();

    RROutput output;

    // CRTC that should be used for this output. Not necessarily the CRTC
    // that XRandR reports is currently being used.
    RRCrtc crtc;

    // Mode currently being used by the output.
    RRMode current_mode;

    // "Best" mode supported by the output.
    RRMode native_mode;

    // Mode used when displaying the same desktop on multiple outputs.
    RRMode mirror_mode;

    // User-selected mode for the output.
    RRMode selected_mode;

    // Output's origin on the framebuffer.
    int x;
    int y;

    bool is_internal;
    bool is_aspect_preserving_scaling;

    // XInput device ID or 0 if this output isn't a touchscreen.
    int touch_device_id;

    CoordinateTransformation transform;

    // Display id for this output.
    int64 display_id;

    bool has_display_id;
  };

  class Observer {
   public:
    virtual ~Observer() {}

    // Called when the change of the display mode finished.  It will usually
    // start the fading in the displays.
    virtual void OnDisplayModeChanged() {}

    // Called when the change of the display mode is issued but failed.
    // |failed_new_state| is the new state which the system failed to enter.
    virtual void OnDisplayModeChangeFailed(OutputState failed_new_state) {}
  };

  // Interface for classes that make decisions about which output state
  // should be used.
  class StateController {
   public:
    virtual ~StateController() {}

    // Called when displays are detected.
    virtual OutputState GetStateForDisplayIds(
        const std::vector<int64>& display_ids) const = 0;

    // Queries the resolution (|width|x|height|) in pixels
    // to select output mode for the given display id.
    virtual bool GetResolutionForDisplayId(int64 display_id,
                                           int* width,
                                           int* height) const = 0;
  };

  // Interface for classes that implement software based mirroring.
  class SoftwareMirroringController {
   public:
    virtual ~SoftwareMirroringController() {}

    // Called when the hardware mirroring failed.
    virtual void SetSoftwareMirroring(bool enabled) = 0;
  };

  // Interface for classes that perform actions on behalf of OutputController.
  class Delegate {
   public:
    virtual ~Delegate() {}

    virtual void SetPanelFittingEnabled(bool enabled) = 0;

    // Initializes the XRandR extension, saving the base event ID to
    // |event_base|.
    virtual void InitXRandRExtension(int* event_base) = 0;

    // Tells XRandR to update its configuration in response to |event|, an
    // RRScreenChangeNotify event.
    virtual void UpdateXRandRConfiguration(const base::NativeEvent& event) = 0;

    // Grabs the X server and refreshes XRandR-related resources.  While
    // the server is grabbed, other clients are blocked.  Must be balanced
    // by a call to UngrabServer().
    virtual void GrabServer() = 0;

    // Ungrabs the server and frees XRandR-related resources.
    virtual void UngrabServer() = 0;

    // Flushes all pending requests and waits for replies.
    virtual void SyncWithServer() = 0;

    // Sets the window's background color to |color_argb|.
    virtual void SetBackgroundColor(uint32 color_argb) = 0;

    // Enables DPMS and forces it to the "on" state.
    virtual void ForceDPMSOn() = 0;

    // Returns information about the current outputs.
    // This method may block for 60 milliseconds or more.
    virtual std::vector<OutputSnapshot> GetOutputs(
        const StateController* state_controller) = 0;

    // Gets details corresponding to |mode|.  Parameters may be NULL.
    // Returns true on success.
    virtual bool GetModeDetails(RRMode mode,
                                int* width,
                                int* height,
                                bool* interlaced) = 0;

    // Calls XRRSetCrtcConfig() with the given options but some of our default
    // output count and rotation arguments. Returns true on success.
    virtual bool ConfigureCrtc(RRCrtc crtc,
                               RRMode mode,
                               RROutput output,
                               int x,
                               int y) = 0;

    // Called to set the frame buffer (underlying XRR "screen") size.  Has
    // a side-effect of disabling all CRTCs.
    virtual void CreateFrameBuffer(
        int width,
        int height,
        const std::vector<OutputConfigurator::OutputSnapshot>& outputs) = 0;

    // Configures XInput's Coordinate Transformation Matrix property.
    // |touch_device_id| the ID of the touchscreen device to configure.
    // |ctm| contains the desired transformation parameters.  The offsets
    // in it should be normalized so that 1 corresponds to the X or Y axis
    // size for the corresponding offset.
    virtual void ConfigureCTM(int touch_device_id,
                              const CoordinateTransformation& ctm) = 0;

    // Sends a D-Bus message to the power manager telling it that the
    // machine is or is not projecting.
    virtual void SendProjectingStateToPowerManager(bool projecting) = 0;
  };

  // Helper class used by tests.
  class TestApi {
   public:
    TestApi(OutputConfigurator* configurator, int xrandr_event_base)
        : configurator_(configurator),
          xrandr_event_base_(xrandr_event_base) {}
    ~TestApi() {}

    // Dispatches RRScreenChangeNotify and RRNotify_OutputChange events to
    // |configurator_| and runs ConfigureOutputs().  Returns false if
    // |configure_timer_| wasn't started.
    bool SendOutputChangeEvents(bool connected);

   private:
    OutputConfigurator* configurator_;  // not owned

    int xrandr_event_base_;

    DISALLOW_COPY_AND_ASSIGN(TestApi);
  };

  // Flags that can be passed to SetDisplayPower().
  static const int kSetDisplayPowerNoFlags                     = 0;
  // Configure displays even if the passed-in state matches |power_state_|.
  static const int kSetDisplayPowerForceProbe                  = 1 << 0;
  // Do not change the state if multiple displays are connected or if the
  // only connected display is external.
  static const int kSetDisplayPowerOnlyIfSingleInternalDisplay = 1 << 1;

  // Gap between screens so cursor at bottom of active display doesn't
  // partially appear on top of inactive display. Higher numbers guard
  // against larger cursors, but also waste more memory.
  // For simplicity, this is hard-coded to avoid the complexity of always
  // determining the DPI of the screen and rationalizing which screen we
  // need to use for the DPI calculation.
  // See crbug.com/130188 for initial discussion.
  static const int kVerticalGap = 60;

  OutputConfigurator();
  virtual ~OutputConfigurator();

  OutputState output_state() const { return output_state_; }
  DisplayPowerState power_state() const { return power_state_; }

  void set_state_controller(StateController* controller) {
    state_controller_ = controller;
  }
  void set_mirroring_controller(SoftwareMirroringController* controller) {
    mirroring_controller_ = controller;
  }

  // Replaces |delegate_| with |delegate| and sets |configure_display_| to
  // true.  Should be called before Init().
  void SetDelegateForTesting(scoped_ptr<Delegate> delegate);

  // Sets the initial value of |power_state_|.  Must be called before Start().
  void SetInitialDisplayPower(DisplayPowerState power_state);

  // Initialization, must be called right after constructor.
  // |is_panel_fitting_enabled| indicates hardware panel fitting support.
  void Init(bool is_panel_fitting_enabled);

  // Does initial configuration of displays during startup.
  // If |background_color_argb| is non zero and there are multiple displays,
  // OutputConfigurator sets the background color of X's RootWindow to this
  // color.
  void Start(uint32 background_color_argb);

  // Stop handling display configuration events/requests.
  void Stop();

  // Called when powerd notifies us that some set of displays should be turned
  // on or off.  This requires enabling or disabling the CRTC associated with
  // the display(s) in question so that the low power state is engaged.
  // |flags| contains bitwise-or-ed kSetDisplayPower* values.
  bool SetDisplayPower(DisplayPowerState power_state, int flags);

  // Force switching the display mode to |new_state|. Returns false if
  // switching failed (possibly because |new_state| is invalid for the
  // current set of connected outputs).
  bool SetDisplayMode(OutputState new_state);

  // Called when an RRNotify event is received.  The implementation is
  // interested in the cases of RRNotify events which correspond to output
  // add/remove events.  Note that Output add/remove events are sent in response
  // to our own reconfiguration operations so spurious events are common.
  // Spurious events will have no effect.
  virtual bool Dispatch(const base::NativeEvent& event) OVERRIDE;

  // Overridden from base::MessagePumpObserver:
  virtual base::EventStatus WillProcessEvent(
      const base::NativeEvent& event) OVERRIDE;
  virtual void DidProcessEvent(
      const base::NativeEvent& event) OVERRIDE;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Sets all the displays into pre-suspend mode; usually this means
  // configure them for their resume state. This allows faster resume on
  // machines where display configuration is slow.
  void SuspendDisplays();

  // Reprobes displays to handle changes made while the system was
  // suspended.
  void ResumeDisplays();

  const std::map<int, float>& GetMirroredDisplayAreaRatioMap() {
    return mirrored_display_area_ratio_map_;
  }

  // Configure outputs with |kConfigureDelayMs| delay,
  // so that time-consuming ConfigureOutputs() won't be called multiple times.
  void ScheduleConfigureOutputs();

 private:
  // Configure outputs.
  void ConfigureOutputs();

  // Fires OnDisplayModeChanged() event to the observers.
  void NotifyOnDisplayChanged();

  // Switches to the state specified in |output_state| and |power_state|.
  // If the hardware mirroring failed and |mirroring_controller_| is set,
  // it switches to |STATE_DUAL_EXTENDED| and calls |SetSoftwareMirroring()|
  // to enable software based mirroring.
  // On success, updates |output_state_|, |power_state_|, and |cached_outputs_|
  // and returns true.
  bool EnterStateOrFallBackToSoftwareMirroring(
      OutputState output_state,
      DisplayPowerState power_state,
      const std::vector<OutputSnapshot>& outputs);

  // Switches to the state specified in |output_state| and |power_state|.
  // On success, updates |output_state_|, |power_state_|, and
  // |cached_outputs_| and returns true.
  bool EnterState(OutputState output_state,
                  DisplayPowerState power_state,
                  const std::vector<OutputSnapshot>& outputs);

  // Returns the output state that should be used with |outputs| connected
  // while in |power_state|.
  OutputState GetOutputState(const std::vector<OutputSnapshot>& outputs,
                             DisplayPowerState power_state) const;

  // Computes the relevant transformation for mirror mode.
  // |output| is the output on which mirror mode is being applied.
  // Returns the transformation or identity if computations fail.
  CoordinateTransformation GetMirrorModeCTM(
      const OutputConfigurator::OutputSnapshot* output);

  // Returns the ratio between mirrored mode area and native mode area:
  // (mirror_mode_width * mirrow_mode_height) / (native_width * native_height)
  float GetMirroredDisplayAreaRatio(
      const OutputConfigurator::OutputSnapshot* output);

  StateController* state_controller_;
  SoftwareMirroringController* mirroring_controller_;
  scoped_ptr<Delegate> delegate_;

  // Key of the map is the touch display's id, and the value of the map is the
  // touch display's area ratio in mirror mode defined as :
  // mirror_mode_area / native_mode_area.
  // This is used for scaling touch event's radius when the touch display is in
  // mirror mode :
  // new_touch_radius = sqrt(area_ratio) * old_touch_radius
  std::map<int, float> mirrored_display_area_ratio_map_;

  // This is detected by the constructor to determine whether or not we should
  // be enabled.  If we aren't running on ChromeOS, we can't assume that the
  // Xrandr X11 extension is supported.
  // If this flag is set to false, any attempts to change the output
  // configuration to immediately fail without changing the state.
  bool configure_display_;

  // The base of the event numbers used to represent XRandr events used in
  // decoding events regarding output add/remove.
  int xrandr_event_base_;

  // The current display state.
  OutputState output_state_;

  // The current power state.
  DisplayPowerState power_state_;

  // Most-recently-used output configuration. Note that the actual
  // configuration changes asynchronously.
  std::vector<OutputSnapshot> cached_outputs_;

  ObserverList<Observer> observers_;

  // The timer to delay configuring outputs. See also the comments in
  // Dispatch().
  scoped_ptr<base::OneShotTimer<OutputConfigurator> > configure_timer_;

  DISALLOW_COPY_AND_ASSIGN(OutputConfigurator);
};

typedef std::vector<OutputConfigurator::OutputSnapshot> OutputSnapshotList;

}  // namespace chromeos

#endif  // CHROMEOS_DISPLAY_OUTPUT_CONFIGURATOR_H_
