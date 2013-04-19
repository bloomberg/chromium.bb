// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/display/output_configurator.h"

#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

#include "base/bind.h"
#include "base/chromeos/chromeos_version.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/time.h"
#include "chromeos/display/real_output_configurator_delegate.h"

namespace chromeos {

namespace {

// Prefixes for the built-in displays.
const char kInternal_LVDS[] = "LVDS";
const char kInternal_eDP[] = "eDP";

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

OutputConfigurator::OutputSnapshot::OutputSnapshot()
    : output(None),
      crtc(None),
      current_mode(None),
      native_mode(None),
      mirror_mode(None),
      y(0),
      height(0),
      is_internal(false),
      is_aspect_preserving_scaling(false),
      touch_device_id(0) {}

OutputConfigurator::CoordinateTransformation::CoordinateTransformation()
  : x_scale(1.0),
    x_offset(0.0),
    y_scale(1.0),
    y_offset(0.0) {}

OutputConfigurator::CrtcConfig::CrtcConfig()
    : crtc(None),
      x(0),
      y(0),
      mode(None),
      output(None) {}

OutputConfigurator::CrtcConfig::CrtcConfig(RRCrtc crtc,
                                           int x, int y,
                                           RRMode mode,
                                           RROutput output)
    : crtc(crtc),
      x(x),
      y(y),
      mode(mode),
      output(output) {}

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

// static
bool OutputConfigurator::IsInternalOutputName(const std::string& name) {
  return name.find(kInternal_LVDS) == 0 || name.find(kInternal_eDP) == 0;
}

OutputConfigurator::OutputConfigurator()
    : state_controller_(NULL),
      configure_display_(base::chromeos::IsRunningOnChromeOS()),
      xrandr_event_base_(0),
      output_state_(STATE_INVALID),
      power_state_(DISPLAY_POWER_ALL_ON) {
}

OutputConfigurator::~OutputConfigurator() {}

void OutputConfigurator::SetDelegateForTesting(
    scoped_ptr<Delegate> delegate) {
  delegate_ = delegate.Pass();
  configure_display_ = true;
}

void OutputConfigurator::SetInitialDisplayPower(DisplayPowerState power_state) {
  DCHECK_EQ(output_state_, STATE_INVALID);
  power_state_ = power_state;
}

void OutputConfigurator::Init(bool is_panel_fitting_enabled,
                              uint32 background_color_argb) {
  if (!configure_display_)
    return;

  if (!delegate_)
    delegate_.reset(new RealOutputConfiguratorDelegate());

  // Cache the initial output state.
  delegate_->SetPanelFittingEnabled(is_panel_fitting_enabled);
  delegate_->GrabServer();
  std::vector<OutputSnapshot> outputs = delegate_->GetOutputs();
  if (outputs.size() > 1 && background_color_argb)
    delegate_->SetBackgroundColor(background_color_argb);
  delegate_->UngrabServer();
}

void OutputConfigurator::Start() {
  if (!configure_display_)
    return;

  delegate_->GrabServer();
  delegate_->InitXRandRExtension(&xrandr_event_base_);

  std::vector<OutputSnapshot> outputs = delegate_->GetOutputs();
  EnterState(GetOutputState(outputs, power_state_), power_state_, outputs);

  // Force the DPMS on chrome startup as the driver doesn't always detect
  // that all displays are on when signing out.
  delegate_->ForceDPMSOn();
  delegate_->UngrabServer();
  delegate_->SendProjectingStateToPowerManager(IsProjecting(outputs));
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
  std::vector<OutputSnapshot> outputs = delegate_->GetOutputs();

  bool only_if_single_internal_display =
      flags & kSetDisplayPowerOnlyIfSingleInternalDisplay;
  bool single_internal_display = outputs.size() == 1 && outputs[0].is_internal;
  if ((single_internal_display || !only_if_single_internal_display) &&
      EnterState(GetOutputState(outputs, power_state), power_state, outputs)) {
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

  if (output_state_ == new_state)
    return true;

  delegate_->GrabServer();
  std::vector<OutputSnapshot> outputs = delegate_->GetOutputs();
  bool success = EnterState(new_state, power_state_, outputs);
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
  }

  return true;
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

void OutputConfigurator::ConfigureOutputs() {
  configure_timer_.reset();

  delegate_->GrabServer();
  std::vector<OutputSnapshot> outputs = delegate_->GetOutputs();
  OutputState new_state = GetOutputState(outputs, power_state_);
  bool success = EnterState(new_state, power_state_, outputs);
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

bool OutputConfigurator::EnterState(
    OutputState output_state,
    DisplayPowerState power_state,
    const std::vector<OutputSnapshot>& outputs) {
  std::vector<bool> output_power;
  int num_on_outputs = GetOutputPower(outputs, power_state, &output_power);

  switch (output_state) {
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

      // Determine which output to use.
      const OutputSnapshot& output = outputs.size() == 1 ? outputs[0] :
          (output_power[0] ? outputs[0] : outputs[1]);
      int width = 0, height = 0;
      if (!delegate_->GetModeDetails(output.native_mode, &width, &height, NULL))
        return false;

      std::vector<CrtcConfig> configs(outputs.size());
      for (size_t i = 0; i < outputs.size(); ++i) {
        configs[i] = CrtcConfig(
            outputs[i].crtc, 0, 0,
            output_power[i] ? outputs[i].native_mode : None,
            outputs[i].output);
      }
      delegate_->CreateFrameBuffer(width, height, configs);

      for (size_t i = 0; i < outputs.size(); ++i) {
        delegate_->ConfigureCrtc(&configs[i]);
        if (outputs[i].touch_device_id) {
          delegate_->ConfigureCTM(outputs[i].touch_device_id,
                                  CoordinateTransformation());
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

      int width = 0, height = 0;
      if (!delegate_->GetModeDetails(
              outputs[0].mirror_mode, &width, &height, NULL)) {
        return false;
      }

      std::vector<CrtcConfig> configs(outputs.size());
      for (size_t i = 0; i < outputs.size(); ++i) {
        configs[i] = CrtcConfig(
            outputs[i].crtc, 0, 0,
            output_power[i] ? outputs[i].mirror_mode : None,
            outputs[i].output);
      }
      delegate_->CreateFrameBuffer(width, height, configs);

      for (size_t i = 0; i < outputs.size(); ++i) {
        delegate_->ConfigureCrtc(&configs[i]);
        if (outputs[i].touch_device_id) {
          CoordinateTransformation ctm;
          // CTM needs to be calculated if aspect preserving scaling is used.
          // Otherwise, assume it is full screen, and use identity CTM.
          if (outputs[i].mirror_mode != outputs[i].native_mode &&
              outputs[i].is_aspect_preserving_scaling) {
            ctm = GetMirrorModeCTM(&outputs[i]);
            mirrored_display_area_ratio_map_[outputs[i].touch_device_id] =
              GetMirroredDisplayAreaRatio(&outputs[i]);
          }
          delegate_->ConfigureCTM(outputs[i].touch_device_id, ctm);
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
      std::vector<CrtcConfig> configs(outputs.size());
      int width = 0, height = 0;

      for (size_t i = 0; i < outputs.size(); ++i) {
        if (!delegate_->GetModeDetails(outputs[i].native_mode,
                &(mode_sizes[i].first), &(mode_sizes[i].second), NULL)) {
          return false;
        }

        configs[i] = CrtcConfig(
            outputs[i].crtc, 0, (height ? height + kVerticalGap : 0),
            output_power[i] ? outputs[i].native_mode : None,
            outputs[i].output);

        // Retain the full screen size even if all outputs are off so the
        // same desktop configuration can be restored when the outputs are
        // turned back on.
        width = std::max<int>(width, mode_sizes[i].first);
        height += (height ? kVerticalGap : 0) + mode_sizes[i].second;
      }

      delegate_->CreateFrameBuffer(width, height, configs);

      for (size_t i = 0; i < outputs.size(); ++i) {
        delegate_->ConfigureCrtc(&configs[i]);
        if (outputs[i].touch_device_id) {
          CoordinateTransformation ctm;
          ctm.x_scale = static_cast<float>(mode_sizes[i].first) / width;
          ctm.x_offset = static_cast<float>(configs[i].x) / width;
          ctm.y_scale = static_cast<float>(mode_sizes[i].second) / height;
          ctm.y_offset = static_cast<float>(configs[i].y) / height;
          delegate_->ConfigureCTM(outputs[i].touch_device_id, ctm);
        }
      }
      break;
    }
    default:
      NOTREACHED() << "Got request to enter output state " << output_state
                   << " with " << outputs.size() << " output(s)";
      return false;
  }

  output_state_ = output_state;
  power_state_ = power_state;
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
        std::vector<OutputInfo> output_infos;
        for (size_t i = 0; i < outputs.size(); ++i) {
          output_infos.push_back(OutputInfo());
          output_infos[i].output = outputs[i].output;
          output_infos[i].output_index = i;
        }
        return state_controller_->GetStateForOutputs(output_infos);
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
