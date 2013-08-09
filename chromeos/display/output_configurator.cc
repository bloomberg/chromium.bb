// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/display/output_configurator.h"

#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/XInput2.h>

#include "base/bind.h"
#include "base/chromeos/chromeos_version.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chromeos/display/output_util.h"
#include "chromeos/display/real_output_configurator_delegate.h"

namespace chromeos {

namespace {

// The delay to perform configuration after RRNotify.  See the comment
// in |Dispatch()|.
const int64 kConfigureDelayMs = 500;

// Returns a string describing |state|.
std::string DisplayPowerStateToString(DisplayPowerState state) {
  switch (state) {
    case DISPLAY_POWER_ALL_ON:
      return "ALL_ON";
    case DISPLAY_POWER_ALL_OFF:
      return "ALL_OFF";
    case DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON:
      return "INTERNAL_OFF_EXTERNAL_ON";
    case DISPLAY_POWER_INTERNAL_ON_EXTERNAL_OFF:
      return "INTERNAL_ON_EXTERNAL_OFF";
    default:
      return "unknown (" + base::IntToString(state) + ")";
  }
}

// Returns a string describing |state|.
std::string OutputStateToString(OutputState state) {
  switch (state) {
    case STATE_INVALID:
      return "INVALID";
    case STATE_HEADLESS:
      return "HEADLESS";
    case STATE_SINGLE:
      return "SINGLE";
    case STATE_DUAL_MIRROR:
      return "DUAL_MIRROR";
    case STATE_DUAL_EXTENDED:
      return "DUAL_EXTENDED";
  }
  NOTREACHED() << "Unknown state " << state;
  return "INVALID";
}

// Returns the number of outputs in |outputs| that should be turned on, per
// |state|.  If |output_power| is non-NULL, it is updated to contain the
// on/off state of each corresponding entry in |outputs|.
int GetOutputPower(
    const std::vector<OutputConfigurator::OutputSnapshot>& outputs,
    DisplayPowerState state,
    std::vector<bool>* output_power) {
  int num_on_outputs = 0;
  if (output_power)
    output_power->resize(outputs.size());

  for (size_t i = 0; i < outputs.size(); ++i) {
    bool internal = outputs[i].is_internal;
    bool on = state == DISPLAY_POWER_ALL_ON ||
        (state == DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON && !internal) ||
        (state == DISPLAY_POWER_INTERNAL_ON_EXTERNAL_OFF && internal);
    if (output_power)
      (*output_power)[i] = on;
    if (on)
      num_on_outputs++;
  }
  return num_on_outputs;
}

// Determine if there is an "internal" output and how many outputs are
// connected.
bool IsProjecting(
    const std::vector<OutputConfigurator::OutputSnapshot>& outputs) {
  bool has_internal_output = false;
  int connected_output_count = outputs.size();
  for (size_t i = 0; i < outputs.size(); ++i)
    has_internal_output |= outputs[i].is_internal;

  // "Projecting" is defined as having more than 1 output connected while at
  // least one of them is an internal output.
  return has_internal_output && (connected_output_count > 1);
}

}  // namespace

OutputConfigurator::CoordinateTransformation::CoordinateTransformation()
    : x_scale(1.0),
      x_offset(0.0),
      y_scale(1.0),
      y_offset(0.0) {}

OutputConfigurator::OutputSnapshot::OutputSnapshot()
    : output(None),
      crtc(None),
      current_mode(None),
      native_mode(None),
      mirror_mode(None),
      selected_mode(None),
      x(0),
      y(0),
      is_internal(false),
      is_aspect_preserving_scaling(false),
      touch_device_id(0),
      display_id(0),
      has_display_id(false) {}

bool OutputConfigurator::TestApi::SendOutputChangeEvents(bool connected) {
  XRRScreenChangeNotifyEvent screen_event;
  memset(&screen_event, 0, sizeof(screen_event));
  screen_event.type = xrandr_event_base_ + RRScreenChangeNotify;
  configurator_->Dispatch(
      reinterpret_cast<const base::NativeEvent>(&screen_event));

  XRROutputChangeNotifyEvent notify_event;
  memset(&notify_event, 0, sizeof(notify_event));
  notify_event.type = xrandr_event_base_ + RRNotify;
  notify_event.subtype = RRNotify_OutputChange;
  notify_event.connection = connected ? RR_Connected : RR_Disconnected;
  configurator_->Dispatch(
      reinterpret_cast<const base::NativeEvent>(&notify_event));

  if (!configurator_->configure_timer_->IsRunning()) {
    LOG(ERROR) << "ConfigureOutputs() timer not running";
    return false;
  }

  configurator_->ConfigureOutputs();
  return true;
}

OutputConfigurator::OutputConfigurator()
    : state_controller_(NULL),
      mirroring_controller_(NULL),
      configure_display_(base::chromeos::IsRunningOnChromeOS()),
      xrandr_event_base_(0),
      output_state_(STATE_INVALID),
      power_state_(DISPLAY_POWER_ALL_ON) {
}

OutputConfigurator::~OutputConfigurator() {}

void OutputConfigurator::SetDelegateForTesting(scoped_ptr<Delegate> delegate) {
  delegate_ = delegate.Pass();
  configure_display_ = true;
}

void OutputConfigurator::SetInitialDisplayPower(DisplayPowerState power_state) {
  DCHECK_EQ(output_state_, STATE_INVALID);
  power_state_ = power_state;
}

void OutputConfigurator::Init(bool is_panel_fitting_enabled) {
  if (!configure_display_)
    return;

  if (!delegate_)
    delegate_.reset(new RealOutputConfiguratorDelegate());
  delegate_->SetPanelFittingEnabled(is_panel_fitting_enabled);
}

void OutputConfigurator::Start(uint32 background_color_argb) {
  if (!configure_display_)
    return;

  delegate_->GrabServer();
  delegate_->InitXRandRExtension(&xrandr_event_base_);

  std::vector<OutputSnapshot> outputs =
      delegate_->GetOutputs(state_controller_);
  if (outputs.size() > 1 && background_color_argb)
    delegate_->SetBackgroundColor(background_color_argb);
  EnterStateOrFallBackToSoftwareMirroring(
      GetOutputState(outputs, power_state_), power_state_, outputs);

  // Force the DPMS on chrome startup as the driver doesn't always detect
  // that all displays are on when signing out.
  delegate_->ForceDPMSOn();
  delegate_->UngrabServer();
  delegate_->SendProjectingStateToPowerManager(IsProjecting(outputs));
  NotifyOnDisplayChanged();
}

void OutputConfigurator::Stop() {
  configure_display_ = false;
}

bool OutputConfigurator::SetDisplayPower(DisplayPowerState power_state,
                                         int flags) {
  if (!configure_display_)
    return false;

  VLOG(1) << "SetDisplayPower: power_state="
          << DisplayPowerStateToString(power_state) << " flags=" << flags;
  if (power_state == power_state_ && !(flags & kSetDisplayPowerForceProbe))
    return true;

  delegate_->GrabServer();
  std::vector<OutputSnapshot> outputs =
      delegate_->GetOutputs(state_controller_);

  bool only_if_single_internal_display =
      flags & kSetDisplayPowerOnlyIfSingleInternalDisplay;
  bool single_internal_display = outputs.size() == 1 && outputs[0].is_internal;
  if ((single_internal_display || !only_if_single_internal_display) &&
      EnterStateOrFallBackToSoftwareMirroring(
          GetOutputState(outputs, power_state), power_state, outputs)) {
    if (power_state != DISPLAY_POWER_ALL_OFF)  {
      // Force the DPMS on since the driver doesn't always detect that it
      // should turn on. This is needed when coming back from idle suspend.
      delegate_->ForceDPMSOn();
    }
  }

  delegate_->UngrabServer();
  return true;
}

bool OutputConfigurator::SetDisplayMode(OutputState new_state) {
  if (!configure_display_)
    return false;

  VLOG(1) << "SetDisplayMode: state=" << OutputStateToString(new_state);
  if (output_state_ == new_state) {
    // Cancel software mirroring if the state is moving from
    // STATE_DUAL_EXTENDED to STATE_DUAL_EXTENDED.
    if (mirroring_controller_ && new_state == STATE_DUAL_EXTENDED)
      mirroring_controller_->SetSoftwareMirroring(false);
    NotifyOnDisplayChanged();
    return true;
  }

  delegate_->GrabServer();
  std::vector<OutputSnapshot> outputs =
      delegate_->GetOutputs(state_controller_);
  bool success = EnterStateOrFallBackToSoftwareMirroring(
      new_state, power_state_, outputs);
  delegate_->UngrabServer();

  if (success) {
    NotifyOnDisplayChanged();
  } else {
    FOR_EACH_OBSERVER(
        Observer, observers_, OnDisplayModeChangeFailed(new_state));
  }
  return success;
}

bool OutputConfigurator::Dispatch(const base::NativeEvent& event) {
  if (!configure_display_)
    return true;

  if (event->type - xrandr_event_base_ == RRScreenChangeNotify) {
    delegate_->UpdateXRandRConfiguration(event);
    return true;
  }

  if (event->type - xrandr_event_base_ != RRNotify)
    return true;

  XEvent* xevent = static_cast<XEvent*>(event);
  XRRNotifyEvent* notify_event =
      reinterpret_cast<XRRNotifyEvent*>(xevent);
  if (notify_event->subtype == RRNotify_OutputChange) {
    XRROutputChangeNotifyEvent* output_change_event =
        reinterpret_cast<XRROutputChangeNotifyEvent*>(xevent);
    if ((output_change_event->connection == RR_Connected) ||
        (output_change_event->connection == RR_Disconnected)) {
      // Connecting/Disconnecting display may generate multiple
      // RRNotify. Defer configuring outputs to avoid
      // grabbing X and configuring displays multiple times.
      ScheduleConfigureOutputs();
    }
  }

  return true;
}

base::EventStatus OutputConfigurator::WillProcessEvent(
    const base::NativeEvent& event) {
  // XI_HierarchyChanged events are special. There is no window associated with
  // these events. So process them directly from here.
  if (configure_display_ && event->type == GenericEvent &&
      event->xgeneric.evtype == XI_HierarchyChanged) {
    // Defer configuring outputs to not stall event processing.
    // This also takes care of same event being received twice.
    ScheduleConfigureOutputs();
  }

  return base::EVENT_CONTINUE;
}

void OutputConfigurator::DidProcessEvent(const base::NativeEvent& event) {
}

void OutputConfigurator::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void OutputConfigurator::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void OutputConfigurator::SuspendDisplays() {
  // If the display is off due to user inactivity and there's only a single
  // internal display connected, switch to the all-on state before
  // suspending.  This shouldn't be very noticeable to the user since the
  // backlight is off at this point, and doing this lets us resume directly
  // into the "on" state, which greatly reduces resume times.
  if (power_state_ == DISPLAY_POWER_ALL_OFF) {
    SetDisplayPower(DISPLAY_POWER_ALL_ON,
                    kSetDisplayPowerOnlyIfSingleInternalDisplay);

    // We need to make sure that the monitor configuration we just did actually
    // completes before we return, because otherwise the X message could be
    // racing with the HandleSuspendReadiness message.
    delegate_->SyncWithServer();
  }
}

void OutputConfigurator::ResumeDisplays() {
  // Force probing to ensure that we pick up any changes that were made
  // while the system was suspended.
  SetDisplayPower(power_state_, kSetDisplayPowerForceProbe);
}

void OutputConfigurator::ScheduleConfigureOutputs() {
  if (configure_timer_.get()) {
    configure_timer_->Reset();
  } else {
    configure_timer_.reset(new base::OneShotTimer<OutputConfigurator>());
    configure_timer_->Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(kConfigureDelayMs),
        this,
        &OutputConfigurator::ConfigureOutputs);
  }
}

void OutputConfigurator::ConfigureOutputs() {
  configure_timer_.reset();

  delegate_->GrabServer();
  std::vector<OutputSnapshot> outputs =
      delegate_->GetOutputs(state_controller_);
  OutputState new_state = GetOutputState(outputs, power_state_);
  bool success = EnterStateOrFallBackToSoftwareMirroring(
      new_state, power_state_, outputs);
  delegate_->UngrabServer();

  if (success) {
    NotifyOnDisplayChanged();
  } else {
    FOR_EACH_OBSERVER(
        Observer, observers_, OnDisplayModeChangeFailed(new_state));
  }
  delegate_->SendProjectingStateToPowerManager(IsProjecting(outputs));
}

void OutputConfigurator::NotifyOnDisplayChanged() {
  FOR_EACH_OBSERVER(Observer, observers_, OnDisplayModeChanged());
}

bool OutputConfigurator::EnterStateOrFallBackToSoftwareMirroring(
    OutputState output_state,
    DisplayPowerState power_state,
    const std::vector<OutputSnapshot>& outputs) {
  bool success = EnterState(output_state, power_state, outputs);
  if (mirroring_controller_) {
    bool enable_software_mirroring = false;
    if (!success && output_state == STATE_DUAL_MIRROR) {
      if (output_state_ != STATE_DUAL_EXTENDED || power_state_ != power_state)
        EnterState(STATE_DUAL_EXTENDED, power_state, outputs);
      enable_software_mirroring = success =
          output_state_ == STATE_DUAL_EXTENDED;
    }
    mirroring_controller_->SetSoftwareMirroring(enable_software_mirroring);
  }
  return success;
}

bool OutputConfigurator::EnterState(
    OutputState output_state,
    DisplayPowerState power_state,
    const std::vector<OutputSnapshot>& outputs) {
  std::vector<bool> output_power;
  int num_on_outputs = GetOutputPower(outputs, power_state, &output_power);
  VLOG(1) << "EnterState: output=" << OutputStateToString(output_state)
          << " power=" << DisplayPowerStateToString(power_state);

  // Framebuffer dimensions.
  int width = 0, height = 0;
  std::vector<OutputSnapshot> updated_outputs = outputs;

  switch (output_state) {
    case STATE_INVALID:
      NOTREACHED() << "Ignoring request to enter invalid state with "
                   << outputs.size() << " connected output(s)";
      return false;
    case STATE_HEADLESS:
      if (outputs.size() != 0) {
        LOG(WARNING) << "Ignoring request to enter headless mode with "
                     << outputs.size() << " connected output(s)";
        return false;
      }
      break;
    case STATE_SINGLE: {
      // If there are multiple outputs connected, only one should be turned on.
      if (outputs.size() != 1 && num_on_outputs != 1) {
        LOG(WARNING) << "Ignoring request to enter single mode with "
                     << outputs.size() << " connected outputs and "
                     << num_on_outputs << " turned on";
        return false;
      }

      for (size_t i = 0; i < updated_outputs.size(); ++i) {
        OutputSnapshot* output = &updated_outputs[i];
        output->x = 0;
        output->y = 0;
        output->current_mode = output_power[i] ? output->selected_mode : None;

        if (output_power[i] || outputs.size() == 1) {
          if (!delegate_->GetModeDetails(
                  output->selected_mode, &width, &height, NULL))
            return false;
        }
      }
      break;
    }
    case STATE_DUAL_MIRROR: {
      if (outputs.size() != 2 || (num_on_outputs != 0 && num_on_outputs != 2)) {
        LOG(WARNING) << "Ignoring request to enter mirrored mode with "
                     << outputs.size() << " connected output(s) and "
                     << num_on_outputs << " turned on";
        return false;
      }

      if (!delegate_->GetModeDetails(
              outputs[0].mirror_mode, &width, &height, NULL))
        return false;

      for (size_t i = 0; i < outputs.size(); ++i) {
        OutputSnapshot* output = &updated_outputs[i];
        output->x = 0;
        output->y = 0;
        output->current_mode = output_power[i] ? output->mirror_mode : None;
        if (output->touch_device_id) {
          // CTM needs to be calculated if aspect preserving scaling is used.
          // Otherwise, assume it is full screen, and use identity CTM.
          if (output->mirror_mode != output->native_mode &&
              output->is_aspect_preserving_scaling) {
            output->transform = GetMirrorModeCTM(output);
            mirrored_display_area_ratio_map_[output->touch_device_id] =
                GetMirroredDisplayAreaRatio(output);
          }
        }
      }
      break;
    }
    case STATE_DUAL_EXTENDED: {
      if (outputs.size() != 2 || (num_on_outputs != 0 && num_on_outputs != 2)) {
        LOG(WARNING) << "Ignoring request to enter extended mode with "
                     << outputs.size() << " connected output(s) and "
                     << num_on_outputs << " turned on";
        return false;
      }

      // Pairs are [width, height] corresponding to the given output's mode.
      std::vector<std::pair<int, int> > mode_sizes(outputs.size());

      for (size_t i = 0; i < outputs.size(); ++i) {
        if (!delegate_->GetModeDetails(outputs[i].selected_mode,
                &(mode_sizes[i].first), &(mode_sizes[i].second), NULL)) {
          return false;
        }

        OutputSnapshot* output = &updated_outputs[i];
        output->x = 0;
        output->y = height ? height + kVerticalGap : 0;
        output->current_mode = output_power[i] ? output->selected_mode : None;

        // Retain the full screen size even if all outputs are off so the
        // same desktop configuration can be restored when the outputs are
        // turned back on.
        width = std::max<int>(width, mode_sizes[i].first);
        height += (height ? kVerticalGap : 0) + mode_sizes[i].second;
      }

      for (size_t i = 0; i < outputs.size(); ++i) {
        OutputSnapshot* output = &updated_outputs[i];
        if (output->touch_device_id) {
          CoordinateTransformation* ctm = &(output->transform);
          ctm->x_scale = static_cast<float>(mode_sizes[i].first) / width;
          ctm->x_offset = static_cast<float>(output->x) / width;
          ctm->y_scale = static_cast<float>(mode_sizes[i].second) / height;
          ctm->y_offset = static_cast<float>(output->y) / height;
        }
      }
      break;
    }
  }

  // Finally, apply the desired changes.
  DCHECK_EQ(outputs.size(), updated_outputs.size());
  if (!outputs.empty()) {
    delegate_->CreateFrameBuffer(width, height, updated_outputs);
    for (size_t i = 0; i < outputs.size(); ++i) {
      const OutputSnapshot& output = updated_outputs[i];
      if (delegate_->ConfigureCrtc(output.crtc, output.current_mode,
                                   output.output, output.x, output.y)) {
        if (output.touch_device_id)
          delegate_->ConfigureCTM(output.touch_device_id, output.transform);
      } else {
        LOG(WARNING) << "Unable to configure CRTC " << output.crtc << ":"
                     << " mode=" << output.current_mode
                     << " output=" << output.output
                     << " x=" << output.x
                     << " y=" << output.y;
        updated_outputs[i] = outputs[i];
      }
    }
  }

  output_state_ = output_state;
  power_state_ = power_state;
  cached_outputs_ = updated_outputs;
  return true;
}

OutputState OutputConfigurator::GetOutputState(
    const std::vector<OutputSnapshot>& outputs,
    DisplayPowerState power_state) const {
  int num_on_outputs = GetOutputPower(outputs, power_state, NULL);
  switch (outputs.size()) {
    case 0:
      return STATE_HEADLESS;
    case 1:
      return STATE_SINGLE;
    case 2: {
      if (num_on_outputs == 1) {
        // If only one output is currently turned on, return the "single"
        // state so that its native mode will be used.
        return STATE_SINGLE;
      } else {
        // With either both outputs on or both outputs off, use one of the
        // dual modes.
        std::vector<int64> display_ids;
        for (size_t i = 0; i < outputs.size(); ++i) {
          // If display id isn't available, switch to extended mode.
          if (!outputs[i].has_display_id)
            return STATE_DUAL_EXTENDED;
          display_ids.push_back(outputs[i].display_id);
        }
        return state_controller_->GetStateForDisplayIds(display_ids);
      }
    }
    default:
      NOTREACHED();
  }
  return STATE_INVALID;
}

OutputConfigurator::CoordinateTransformation
OutputConfigurator::GetMirrorModeCTM(
    const OutputConfigurator::OutputSnapshot* output) {
  CoordinateTransformation ctm;  // Default to identity
  int native_mode_width = 0, native_mode_height = 0;
  int mirror_mode_width = 0, mirror_mode_height = 0;
  if (!delegate_->GetModeDetails(output->native_mode,
          &native_mode_width, &native_mode_height, NULL) ||
      !delegate_->GetModeDetails(output->mirror_mode,
          &mirror_mode_width, &mirror_mode_height, NULL))
    return ctm;

  if (native_mode_height == 0 || mirror_mode_height == 0 ||
      native_mode_width == 0 || mirror_mode_width == 0)
    return ctm;

  float native_mode_ar = static_cast<float>(native_mode_width) /
      static_cast<float>(native_mode_height);
  float mirror_mode_ar = static_cast<float>(mirror_mode_width) /
      static_cast<float>(mirror_mode_height);

  if (mirror_mode_ar > native_mode_ar) {  // Letterboxing
    ctm.x_scale = 1.0;
    ctm.x_offset = 0.0;
    ctm.y_scale = mirror_mode_ar / native_mode_ar;
    ctm.y_offset = (native_mode_ar / mirror_mode_ar - 1.0) * 0.5;
    return ctm;
  }
  if (native_mode_ar > mirror_mode_ar) {  // Pillarboxing
    ctm.y_scale = 1.0;
    ctm.y_offset = 0.0;
    ctm.x_scale = native_mode_ar / mirror_mode_ar;
    ctm.x_offset = (mirror_mode_ar / native_mode_ar - 1.0) * 0.5;
    return ctm;
  }

  return ctm;  // Same aspect ratio - return identity
}

float OutputConfigurator::GetMirroredDisplayAreaRatio(
    const OutputConfigurator::OutputSnapshot* output) {
  float area_ratio = 1.0f;
  int native_mode_width = 0, native_mode_height = 0;
  int mirror_mode_width = 0, mirror_mode_height = 0;
  if (!delegate_->GetModeDetails(output->native_mode,
          &native_mode_width, &native_mode_height, NULL) ||
      !delegate_->GetModeDetails(output->mirror_mode,
          &mirror_mode_width, &mirror_mode_height, NULL))
    return area_ratio;

  if (native_mode_height == 0 || mirror_mode_height == 0 ||
      native_mode_width == 0 || mirror_mode_width == 0)
    return area_ratio;

  float width_ratio = static_cast<float>(mirror_mode_width) /
      static_cast<float>(native_mode_width);
  float height_ratio = static_cast<float>(mirror_mode_height) /
      static_cast<float>(native_mode_height);

  area_ratio = width_ratio * height_ratio;
  return area_ratio;
}

}  // namespace chromeos
