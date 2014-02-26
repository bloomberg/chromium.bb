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
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/display/native_display_observer.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/display/display_constants.h"

// Forward declarations for Xlib and Xrandr.
// This is so unused X definitions don't pollute the namespace.
typedef unsigned long XID;
typedef XID RROutput;
typedef XID RRCrtc;
typedef XID RRMode;

namespace chromeos {

class NativeDisplayDelegate;

// This class interacts directly with the underlying Xrandr API to manipulate
// CTRCs and Outputs.
class CHROMEOS_EXPORT OutputConfigurator
    : public NativeDisplayObserver {
 public:
  typedef uint64_t OutputProtectionClientId;
  static const OutputProtectionClientId kInvalidClientId = 0;

  struct ModeInfo {
    ModeInfo();
    ModeInfo(int width, int height, bool interlaced, float refresh_rate);

    int width;
    int height;
    bool interlaced;
    float refresh_rate;
  };

  typedef std::map<RRMode, ModeInfo> ModeInfoMap;

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
    ~OutputSnapshot();

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

    // Output's physical dimensions.
    uint64 width_mm;
    uint64 height_mm;

    bool is_aspect_preserving_scaling;

    // The type of output.
    ui::OutputType type;

    // Map from mode IDs to details about the corresponding modes.
    ModeInfoMap mode_infos;

    // XInput device ID or 0 if this output isn't a touchscreen.
    int touch_device_id;

    CoordinateTransformation transform;

    // Display id for this output.
    int64 display_id;

    bool has_display_id;

    // This output's index in the array returned by XRandR. Stable even as
    // outputs are connected or disconnected.
    int index;
  };

  class Observer {
   public:
    virtual ~Observer() {}

    // Called after the display mode has been changed. |output| contains the
    // just-applied configuration. Note that the X server is no longer grabbed
    // when this method is called, so the actual configuration could've changed
    // already.
    virtual void OnDisplayModeChanged(
        const std::vector<OutputSnapshot>& outputs) {}

    // Called after a display mode change attempt failed. |failed_new_state| is
    // the new state which the system failed to enter.
    virtual void OnDisplayModeChangeFailed(ui::OutputState failed_new_state) {}
  };

  // Interface for classes that make decisions about which output state
  // should be used.
  class StateController {
   public:
    virtual ~StateController() {}

    // Called when displays are detected.
    virtual ui::OutputState GetStateForDisplayIds(
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

  class TouchscreenDelegate {
   public:
    virtual ~TouchscreenDelegate() {}

    // Searches for touchscreens among input devices,
    // and tries to match them up to screens in |outputs|.
    // |outputs| is an array of detected screens.
    // If a touchscreen with same resolution as an output's native mode
    // is detected, its id will be stored in this output.
    virtual void AssociateTouchscreens(
        std::vector<OutputSnapshot>* outputs) = 0;

    // Configures XInput's Coordinate Transformation Matrix property.
    // |touch_device_id| the ID of the touchscreen device to configure.
    // |ctm| contains the desired transformation parameters.  The offsets
    // in it should be normalized so that 1 corresponds to the X or Y axis
    // size for the corresponding offset.
    virtual void ConfigureCTM(int touch_device_id,
                              const CoordinateTransformation& ctm) = 0;
  };

  // Helper class used by tests.
  class TestApi {
   public:
    TestApi(OutputConfigurator* configurator)
        : configurator_(configurator) {}
    ~TestApi() {}

    const std::vector<OutputSnapshot>& cached_outputs() const {
      return configurator_->cached_outputs_;
    }

    // If |configure_timer_| is started, stops the timer, runs
    // ConfigureOutputs(), and returns true; returns false otherwise.
    bool TriggerConfigureTimeout();

   private:
    OutputConfigurator* configurator_;  // not owned

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

  // Returns a pointer to the ModeInfo struct in |output| corresponding to
  // |mode|, or NULL if the struct isn't present.
  static const ModeInfo* GetModeInfo(const OutputSnapshot& output,
                                     RRMode mode);

  // Returns the mode within |output| that matches the given size with highest
  // refresh rate. Returns None if no matching output was found.
  static RRMode FindOutputModeMatchingSize(const OutputSnapshot& output,
                                           int width,
                                           int height);

  OutputConfigurator();
  virtual ~OutputConfigurator();

  ui::OutputState output_state() const { return output_state_; }
  DisplayPowerState power_state() const { return power_state_; }

  void set_state_controller(StateController* controller) {
    state_controller_ = controller;
  }
  void set_mirroring_controller(SoftwareMirroringController* controller) {
    mirroring_controller_ = controller;
  }

  // Replaces |native_display_delegate_| with |delegate| and sets
  // |configure_display_| to true.  Should be called before Init().
  void SetNativeDisplayDelegateForTesting(
      scoped_ptr<NativeDisplayDelegate> delegate);

  void SetTouchscreenDelegateForTesting(
      scoped_ptr<TouchscreenDelegate> delegate);

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
  bool SetDisplayMode(ui::OutputState new_state);

  // NativeDisplayDelegate::Observer overrides:
  virtual void OnConfigurationChanged() OVERRIDE;

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

  // Registers a client for output protection and requests a client id. Returns
  // 0 if requesting failed.
  OutputProtectionClientId RegisterOutputProtectionClient();

  // Unregisters the client.
  void UnregisterOutputProtectionClient(OutputProtectionClientId client_id);

  // Queries link status and protection status.
  // |link_mask| is the type of connected output links, which is a bitmask of
  // OutputType values. |protection_mask| is the desired protection methods,
  // which is a bitmask of the OutputProtectionMethod values.
  // Returns true on success.
  bool QueryOutputProtectionStatus(
      OutputProtectionClientId client_id,
      int64 display_id,
      uint32_t* link_mask,
      uint32_t* protection_mask);

  // Requests the desired protection methods.
  // |protection_mask| is the desired protection methods, which is a bitmask
  // of the OutputProtectionMethod values.
  // Returns true when the protection request has been made.
  bool EnableOutputProtection(
      OutputProtectionClientId client_id,
      int64 display_id,
      uint32_t desired_protection_mask);

 private:
  // Mapping a display_id to a protection request bitmask.
  typedef std::map<int64, uint32_t> DisplayProtections;
  // Mapping a client to its protection request.
  typedef std::map<OutputProtectionClientId,
                   DisplayProtections> ProtectionRequests;

  // Updates |cached_outputs_| to contain currently-connected outputs. Calls
  // |delegate_->GetOutputs()| and then does additional work, like finding the
  // mirror mode and setting user-preferred modes. Note that the server must be
  // grabbed via |delegate_->GrabServer()| first.
  void UpdateCachedOutputs();

  // Helper method for UpdateCachedOutputs() that initializes the passed-in
  // outputs' |mirror_mode| fields by looking for a mode in |internal_output|
  // and |external_output| having the same resolution. Returns false if a shared
  // mode wasn't found or created.
  //
  // |try_panel_fitting| allows creating a panel-fitting mode for
  // |internal_output| instead of only searching for a matching mode (note that
  // it may lead to a crash if |internal_info| is not capable of panel fitting).
  //
  // |preserve_aspect| limits the search/creation only to the modes having the
  // native aspect ratio of |external_output|.
  bool FindMirrorMode(OutputSnapshot* internal_output,
                      OutputSnapshot* external_output,
                      bool try_panel_fitting,
                      bool preserve_aspect);

  // Configures outputs.
  void ConfigureOutputs();

  // Notifies observers about an attempted state change.
  void NotifyObservers(bool success, ui::OutputState attempted_state);

  // Switches to the state specified in |output_state| and |power_state|.
  // If the hardware mirroring failed and |mirroring_controller_| is set,
  // it switches to |STATE_DUAL_EXTENDED| and calls |SetSoftwareMirroring()|
  // to enable software based mirroring.
  // On success, updates |output_state_|, |power_state_|, and |cached_outputs_|
  // and returns true.
  bool EnterStateOrFallBackToSoftwareMirroring(ui::OutputState output_state,
                                               DisplayPowerState power_state);

  // Switches to the state specified in |output_state| and |power_state|.
  // On success, updates |output_state_|, |power_state_|, and
  // |cached_outputs_| and returns true.
  bool EnterState(ui::OutputState output_state, DisplayPowerState power_state);

  // Returns the output state that should be used with |cached_outputs_| while
  // in |power_state|.
  ui::OutputState ChooseOutputState(DisplayPowerState power_state) const;

  // Computes the relevant transformation for mirror mode.
  // |output| is the output on which mirror mode is being applied.
  // Returns the transformation or identity if computations fail.
  CoordinateTransformation GetMirrorModeCTM(
      const OutputConfigurator::OutputSnapshot& output);

  // Computes the relevant transformation for extended mode.
  // |output| is the output on which extended mode is being applied.
  // |width| and |height| are the width and height of the combined framebuffer.
  // Returns the transformation or identity if computations fail.
  CoordinateTransformation GetExtendedModeCTM(
      const OutputConfigurator::OutputSnapshot& output,
      int framebuffer_width,
      int frame_buffer_height);

  // Returns the ratio between mirrored mode area and native mode area:
  // (mirror_mode_width * mirrow_mode_height) / (native_width * native_height)
  float GetMirroredDisplayAreaRatio(
      const OutputConfigurator::OutputSnapshot& output);

  // Applies output protections according to requests.
  bool ApplyProtections(const DisplayProtections& requests);

  StateController* state_controller_;
  SoftwareMirroringController* mirroring_controller_;
  scoped_ptr<NativeDisplayDelegate> native_display_delegate_;
  scoped_ptr<TouchscreenDelegate> touchscreen_delegate_;

  // Used to enable modes which rely on panel fitting.
  bool is_panel_fitting_enabled_;

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

  // The current display state.
  ui::OutputState output_state_;

  // The current power state.
  DisplayPowerState power_state_;

  // Most-recently-used output configuration. Note that the actual
  // configuration changes asynchronously.
  std::vector<OutputSnapshot> cached_outputs_;

  ObserverList<Observer> observers_;

  // The timer to delay configuring outputs. See also the comments in
  // Dispatch().
  scoped_ptr<base::OneShotTimer<OutputConfigurator> > configure_timer_;

  // Id for next output protection client.
  OutputProtectionClientId next_output_protection_client_id_;

  // Output protection requests of each client.
  ProtectionRequests client_protection_requests_;

  DISALLOW_COPY_AND_ASSIGN(OutputConfigurator);
};

typedef std::vector<OutputConfigurator::OutputSnapshot> OutputSnapshotList;

}  // namespace chromeos

#endif  // CHROMEOS_DISPLAY_OUTPUT_CONFIGURATOR_H_
