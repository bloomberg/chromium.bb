// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/display/output_configurator.h"

#include <cmath>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/dpms.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/Xrandr.h>

// Xlib defines Status as int which causes our include of dbus/bus.h to fail
// when it tries to name an enum Status.  Thus, we need to undefine it (note
// that this will cause a problem if code needs to use the Status type).
// RootWindow causes similar problems in that there is a Chromium type with that
// name.
#undef Status
#undef RootWindow

#include "base/bind.h"
#include "base/chromeos/chromeos_version.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/message_pump_aurax11.h"
#include "base/metrics/histogram.h"
#include "base/perftimer.h"
#include "base/time.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"

namespace chromeos {

struct OutputSnapshot {
  RROutput output;
  RRCrtc crtc;
  RRMode current_mode;
  int height;
  int y;
  RRMode native_mode;
  RRMode mirror_mode;
  bool is_internal;
  bool is_aspect_preserving_scaling;
  int touch_device_id;
};

struct CoordinateTransformation {
  // Initialize to identity transformation
  CoordinateTransformation()
    : x_scale(1.0),
      x_offset(0.0),
      y_scale(1.0),
      y_offset(0.0) {
  }

  float x_scale;
  float x_offset;
  float y_scale;
  float y_offset;
};

struct CrtcConfig {
  CrtcConfig()
    : crtc(None),
      x(0),
      y(0),
      mode(None),
      output(None) {}

  CrtcConfig(RRCrtc crtc, int x, int y, RRMode mode, RROutput output)
    : crtc(crtc),
      x(x),
      y(y),
      mode(mode),
      output(output) {}

  RRCrtc crtc;
  int x;
  int y;
  RRMode mode;
  RROutput output;
};

enum MirrorModeType {
  MIRROR_MODE_NONE,
  MIRROR_MODE_ASPECT_PRESERVING,
  MIRROR_MODE_FALLBACK,
  MIRROR_MODE_TYPE_COUNT
};

namespace {
// DPI measurements.
const float kMmInInch = 25.4;
const float kDpi96 = 96.0;
const float kPixelsToMmScale = kMmInInch / kDpi96;

// The DPI threshold to detech high density screen.
// Higher DPI than this will use device_scale_factor=2
// Should be kept in sync with display_change_observer_x11.cc
const unsigned int kHighDensityDIPThreshold = 160;

// Prefixes for the built-in displays.
const char kInternal_LVDS[] = "LVDS";
const char kInternal_eDP[] = "eDP";

// The delay to perform configuration after RRNotify.  See the comment
// in |Dispatch()|.
const int64 kConfigureDelayMs = 500;

// Gap between screens so cursor at bottom of active display doesn't partially
// appear on top of inactive display. Higher numbers guard against larger
// cursors, but also waste more memory.
// For simplicity, this is hard-coded to 60 to avoid the complexity of always
// determining the DPI of the screen and rationalizing which screen we need to
// use for the DPI calculation.
// See crbug.com/130188 for initial discussion.
const int kVerticalGap = 60;

// TODO: Determine if we need to organize modes in a way which provides better
// than O(n) lookup time.  In many call sites, for example, the "next" mode is
// typically what we are looking for so using this helper might be too
// expensive.
XRRModeInfo* ModeInfoForID(XRRScreenResources* screen, RRMode modeID) {
  XRRModeInfo* result = NULL;
  for (int i = 0; (i < screen->nmode) && (result == NULL); i++)
    if (modeID == screen->modes[i].id)
      result = &screen->modes[i];

  return result;
}

// A helper to call XRRSetCrtcConfig with the given options but some of our
// default output count and rotation arguments.
void ConfigureCrtc(Display* display,
                   XRRScreenResources* screen,
                   CrtcConfig* config) {
  VLOG(1) << "ConfigureCrtc crtc: " << config->crtc
          << ", mode " << config->mode
          << ", output " << config->output
          << ", x " << config->x
          << ", y " << config->y;

  const Rotation kRotate = RR_Rotate_0;
  RROutput* outputs = NULL;
  int num_outputs = 0;

  // Check the output and mode argument - if either are None, we should disable.
  if ((config->output != None) && (config->mode != None)) {
    outputs = &config->output;
    num_outputs = 1;
  }

  XRRSetCrtcConfig(display,
                   screen,
                   config->crtc,
                   CurrentTime,
                   config->x,
                   config->y,
                   config->mode,
                   kRotate,
                   outputs,
                   num_outputs);
}

// Destroys unused Crtcs, and parks used Crtcs in a way which allows a
// framebuffer resize. This is faster than turning them off, resizing,
// then turning them back on.
void DestroyUnusedCrtcs(Display* display,
                        XRRScreenResources* screen,
                        Window window,
                        CrtcConfig* config1,
                        CrtcConfig* config2) {
  // Setting the screen size will fail if any CRTC doesn't fit afterwards.
  // At the same time, turning CRTCs off and back on uses up a lot of time.
  // This function tries to be smart to avoid too many off/on cycles:
  // - We disable all the CRTCs we won't need after the FB resize.
  // - We set the new modes on CRTCs, if they fit in both the old and new
  //   FBs, and park them at (0,0)
  // - We disable the CRTCs we will need but don't fit in the old FB. Those
  //   will be reenabled after the resize.
  // We don't worry about the cached state of the outputs here since we are
  // not interested in the state we are setting - we just try to get the CRTCs
  // out of the way so we can rebuild the frame buffer.
  for (int i = 0; i < screen->ncrtc; ++i) {
    // Default config is to disable the crtcs.
    CrtcConfig config(screen->crtcs[i], 0, 0, None, None);

    // If we are going to use that CRTC later, prepare it now.
    if (config1 && screen->crtcs[i] == config1->crtc) {
      config = *config1;
      config.x = 0;
      config.y = 0;
    } else if (config2 && screen->crtcs[i] == config2->crtc) {
      config = *config2;
      config.x = 0;
      config.y = 0;
    }

    if (config.mode != None) {
      // In case our CRTC doesn't fit in our current framebuffer, disable it.
      // It'll get reenabled after we resize the framebuffer.
      XRRModeInfo* mode_info = ModeInfoForID(screen, config.mode);
      int current_width = DisplayWidth(display, DefaultScreen(display));
      int current_height = DisplayHeight(display, DefaultScreen(display));
      if (static_cast<int>(mode_info->width) > current_width ||
          static_cast<int>(mode_info->height) > current_height) {
        config.mode = None;
        config.output = None;
      }
    }

    ConfigureCrtc(display, screen, &config);
  }
}

// Called to set the frame buffer (underling XRR "screen") size.  Has a
// side-effect of disabling all CRTCs.
void CreateFrameBuffer(Display* display,
                       XRRScreenResources* screen,
                       Window window,
                       int width,
                       int height,
                       CrtcConfig* config1,
                       CrtcConfig* config2) {
  TRACE_EVENT0("chromeos", "OutputConfigurator::CreateFrameBuffer");
  VLOG(1) << "CreateFrameBuffer " << width << " by " << height;

  DestroyUnusedCrtcs(display, screen, window, config1, config2);

  int mm_width = width * kPixelsToMmScale;
  int mm_height = height * kPixelsToMmScale;
  XRRSetScreenSize(display, window, width, height, mm_width, mm_height);
}

// Configures X input's Coordinate Transformation Matrix property.
// |display| is used to make X calls.
// |touch_device_id| is X's id of touchscreen device to configure.
// |ctm| contains the desired transformation parameters.
// The offsets in it should be normalized,
// so that 1 corresponds to x or y axis size for the respectful offset.
void ConfigureCTM(Display* display,
                  int touch_device_id,
                  const CoordinateTransformation& ctm) {
  int ndevices;
  XIDeviceInfo* info = XIQueryDevice(display, touch_device_id, &ndevices);
  Atom prop = XInternAtom(display, "Coordinate Transformation Matrix", False);
  Atom float_atom = XInternAtom(display, "FLOAT", False);
  if (ndevices == 1 && prop != None && float_atom != None) {
    Atom type;
    int format;
    unsigned long num_items;
    unsigned long bytes_after;
    unsigned char* data = NULL;
    // Verify that the property exists with correct format, type, etc.
    int status = XIGetProperty(display,
                               info->deviceid,
                               prop,
                               0,  // Irrelevant - we are not interested in data
                               0,  // Irrelevant - we are not interested in data
                               False,  // Leave the property as is
                               AnyPropertyType,
                               &type,
                               &format,
                               &num_items,
                               &bytes_after,
                               &data);
    if (data)
      XFree(data);
    if (status == Success && type == float_atom && format == 32) {
      float value[3][3] = {
          { ctm.x_scale,         0.0, ctm.x_offset },
          {         0.0, ctm.y_scale, ctm.y_offset },
          {         0.0,         0.0,          1.0 }
      };
      XIChangeProperty(display,
                       info->deviceid,
                       prop,
                       type,
                       format,
                       PropModeReplace,
                       reinterpret_cast<unsigned char*>(value),
                       9);
    }
  }
  XIFreeDeviceInfo(info);
}

// Computes the relevant transformation for mirror mode.
// |screen| is used to make X calls.
// |output| is the output on which mirror mode is being applied.
// Returns the transformation, which would be identity if computations fail.
CoordinateTransformation GetMirrorModeCTM(XRRScreenResources* screen,
                                          const OutputSnapshot* output) {
  CoordinateTransformation ctm;  // Default to identity
  XRRModeInfo* native_mode_info = ModeInfoForID(screen, output->native_mode);
  XRRModeInfo* mirror_mode_info = ModeInfoForID(screen, output->mirror_mode);
  if (native_mode_info == NULL || mirror_mode_info == NULL)
    return ctm;

  if (native_mode_info->height == 0 || mirror_mode_info->height == 0 ||
      native_mode_info->width == 0 || mirror_mode_info->width == 0)
    return ctm;

  float native_mode_ar = static_cast<float>(native_mode_info->width) /
      static_cast<float>(native_mode_info->height);
  float mirror_mode_ar = static_cast<float>(mirror_mode_info->width) /
      static_cast<float>(mirror_mode_info->height);

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

OutputState InferCurrentState(Display* display,
                              XRRScreenResources* screen,
                              const std::vector<OutputSnapshot>& outputs) {
  TRACE_EVENT0("chromeos", "OutputConfigurator::InferCurrentState");
  OutputState state = STATE_INVALID;
  switch (outputs.size()) {
    case 0:
      state = STATE_HEADLESS;
      break;
    case 1:
      state = STATE_SINGLE;
      break;
    case 2: {
      RRMode primary_mode = outputs[0].current_mode;
      RRMode secondary_mode = outputs[1].current_mode;

      if ((outputs[0].y == 0) && (outputs[1].y == 0)) {
        // Displays in the same spot so this is either mirror or unknown.
        // Note that we should handle no configured CRTC as a "wildcard" since
        // that allows us to preserve mirror mode state while power is switched
        // off on one display.
        bool primary_mirror = (outputs[0].mirror_mode == primary_mode) ||
            (primary_mode == None);
        bool secondary_mirror = (outputs[1].mirror_mode == secondary_mode) ||
            (secondary_mode == None);
        if (primary_mirror && secondary_mirror) {
          state = STATE_DUAL_MIRROR;
        } else {
          // We should never normally get into this state but it can help us
          // make sense of situations where the configuration may have been
          // changed for testing, etc.
          state = STATE_DUAL_UNKNOWN;
        }
      } else {
        // At this point, we expect both displays to be in native mode and tiled
        // such that one is primary and another is correctly positioned as
        // secondary.  If any of these assumptions are false, this is an unknown
        // configuration.
        bool primary_native = (primary_mode == outputs[0].native_mode) ||
            (primary_mode == None);
        bool secondary_native = (secondary_mode == outputs[1].native_mode) ||
            (secondary_mode == None);
        if (primary_native && secondary_native) {
          // Just check the relative locations.
          int secondary_offset = outputs[0].height + kVerticalGap;
          if ((outputs[0].y == 0) && (outputs[1].y == secondary_offset)) {
            state = STATE_DUAL_EXTENDED;
          } else {
            // Unexpected locations.
            state = STATE_DUAL_UNKNOWN;
          }
        } else {
          // Mode assumptions don't hold.
          state = STATE_DUAL_UNKNOWN;
        }
      }
      break;
    }
    default:
      CHECK(false);
  }
  return state;
}

OutputState GetNextState(Display* display,
                         XRRScreenResources* screen,
                         OutputState current_state,
                         const std::vector<OutputSnapshot>& outputs) {
  TRACE_EVENT0("chromeos", "OutputConfigurator::GetNextState");
  OutputState state = STATE_INVALID;

  switch (outputs.size()) {
    case 0:
      state = STATE_HEADLESS;
      break;
    case 1:
      state = STATE_SINGLE;
      break;
    case 2: {
      bool mirror_supported = (0 != outputs[0].mirror_mode) &&
          (0 != outputs[1].mirror_mode);
      switch (current_state) {
        case STATE_DUAL_EXTENDED:
          state =
              mirror_supported ? STATE_DUAL_MIRROR : STATE_DUAL_EXTENDED;
          break;
        case STATE_DUAL_MIRROR:
          state = STATE_DUAL_EXTENDED;
          break;
        default:
          // Default to extended mode.
          state = STATE_DUAL_EXTENDED;
      }
      break;
    }
    default:
      CHECK(false);
  }
  return state;
}

RRCrtc GetNextCrtcAfter(Display* display,
                        XRRScreenResources* screen,
                        RROutput output,
                        RRCrtc previous) {
  RRCrtc crtc = None;
  XRROutputInfo* output_info = XRRGetOutputInfo(display, screen, output);

  for (int i = 0; (i < output_info->ncrtc) && (crtc == None); ++i) {
    RRCrtc this_crtc = output_info->crtcs[i];

    if (previous != this_crtc)
      crtc = this_crtc;
  }
  XRRFreeOutputInfo(output_info);
  return crtc;
}

XRRScreenResources* GetScreenResourcesAndRecordUMA(Display* display,
                                                   Window window) {
  // This call to XRRGetScreenResources is implicated in a hang bug so
  // instrument it to see its typical running time (crbug.com/134449).
  // TODO(disher): Remove these UMA calls once crbug.com/134449 is resolved.
  UMA_HISTOGRAM_BOOLEAN("Display.XRRGetScreenResources_completed", false);
  PerfTimer histogram_timer;
  XRRScreenResources* screen = XRRGetScreenResources(display, window);
  base::TimeDelta duration = histogram_timer.Elapsed();
  UMA_HISTOGRAM_BOOLEAN("Display.XRRGetScreenResources_completed", true);
  UMA_HISTOGRAM_LONG_TIMES("Display.XRRGetScreenResources_duration", duration);
  return screen;
}

// Determine if there is an "internal" output and how many outputs are
// connected.
bool IsProjecting(const std::vector<OutputSnapshot>& outputs) {
  bool has_internal_output = false;
  int connected_output_count = outputs.size();
  for (size_t i = 0; i < outputs.size(); ++i)
    has_internal_output |= outputs[i].is_internal;

  // "Projecting" is defined as having more than 1 output connected while at
  // least one of them is an internal output.
  return has_internal_output && (connected_output_count > 1);
}

// Returns whether the |output| is configured to preserve aspect when scaling.
bool IsOutputAspectPreservingScaling(Display* display,
                                     RROutput output) {
  bool ret = false;

  Atom scaling_prop = XInternAtom(display, "scaling mode", False);
  Atom full_aspect_atom = XInternAtom(display, "Full aspect", False);
  if (scaling_prop == None || full_aspect_atom == None)
    return false;

  int nprop = 0;
  Atom* props = XRRListOutputProperties(display, output, &nprop);
  for (int j = 0; j < nprop && !ret; j++) {
    Atom prop = props[j];
    if (scaling_prop == prop) {
      unsigned char* values = NULL;
      int actual_format;
      unsigned long nitems;
      unsigned long bytes_after;
      Atom actual_type;
      int success;

      success = XRRGetOutputProperty(display,
                                     output,
                                     prop,
                                     0,
                                     100,
                                     False,
                                     False,
                                     AnyPropertyType,
                                     &actual_type,
                                     &actual_format,
                                     &nitems,
                                     &bytes_after,
                                     &values);
      if (success == Success && actual_type == XA_ATOM &&
          actual_format == 32 && nitems == 1) {
        Atom value = reinterpret_cast<Atom*>(values)[0];
        if (full_aspect_atom == value)
          ret = true;
      }
      if (values)
        XFree(values);
    }
  }
  if (props)
    XFree(props);

  return ret;
}

}  // namespace

OutputConfigurator::OutputConfigurator()
    // If we aren't running on ChromeOS (like linux desktop),
    // don't try to configure display.
    : configure_display_(base::chromeos::IsRunningOnChromeOS()),
      is_panel_fitting_enabled_(false),
      connected_output_count_(0),
      xrandr_event_base_(0),
      output_state_(STATE_INVALID),
      mirror_mode_will_preserve_aspect_(false),
      mirror_mode_preserved_aspect_(false),
      last_enter_state_time_() {
}

OutputConfigurator::~OutputConfigurator() {
  RecordPreviousStateUMA();
}

void OutputConfigurator::Init(bool is_panel_fitting_enabled,
                              uint32 background_color_argb) {
  TRACE_EVENT0("chromeos", "OutputConfigurator::Init");
  if (!configure_display_)
    return;
  is_panel_fitting_enabled_ = is_panel_fitting_enabled;

  // Cache the initial output state.
  Display* display = base::MessagePumpAuraX11::GetDefaultXDisplay();
  CHECK(display != NULL);
  XGrabServer(display);
  Window window = DefaultRootWindow(display);
  XRRScreenResources* screen = GetScreenResourcesAndRecordUMA(display, window);
  CHECK(screen != NULL);

  // Detect our initial state.
  std::vector<OutputSnapshot> outputs = GetDualOutputs(display, screen);
  connected_output_count_ = outputs.size();
  if (outputs.size() > 1 && background_color_argb) {
    // Configuring CRTCs/Framebuffer clears the boot screen image.
    // Set the same background color while configuring the
    // display to minimize the duration of black screen at boot
    // time. The background is filled with black later in
    // ash::DisplayManager.
    // crbug.com/171050.
    XSetWindowAttributes swa = {0};
    XColor color;
    Colormap colormap = DefaultColormap(display, 0);
    // XColor uses 16 bits per color.
    color.red = (background_color_argb & 0x00FF0000) >> 8;
    color.green = (background_color_argb & 0x0000FF00);
    color.blue = (background_color_argb & 0x000000FF) << 8;
    color.flags = DoRed | DoGreen | DoBlue;
    XAllocColor(display, colormap, &color);
    swa.background_pixel = color.pixel;
    XChangeWindowAttributes(display, window, CWBackPixel, &swa);
    XFreeColors(display, colormap, &color.pixel, 1, 0);
  }

  output_state_ = InferCurrentState(display, screen, outputs);
  // Ensure that we are in a supported state with all connected displays powered
  // on.
  OutputState starting_state = GetNextState(display,
                                            screen,
                                            STATE_INVALID,
                                            outputs);
  if (output_state_ != starting_state &&
      EnterState(display,
                 screen,
                 window,
                 starting_state,
                 outputs)) {
    output_state_ = starting_state;
  }
  bool is_projecting = IsProjecting(outputs);

  // Find xrandr_event_base_ since we need it to interpret events, later.
  int error_base_ignored = 0;
  XRRQueryExtension(display, &xrandr_event_base_, &error_base_ignored);

  // Force the DPMS on chrome startup as the driver doesn't always detect
  // that all displays are on when signing out.
  CHECK(DPMSEnable(display));
  CHECK(DPMSForceLevel(display, DPMSModeOn));

  // Relinquish X resources.
  XRRFreeScreenResources(screen);
  XUngrabServer(display);

  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
      SetIsProjecting(is_projecting);
}

void OutputConfigurator::Stop() {
  configure_display_ = false;
}

bool OutputConfigurator::CycleDisplayMode() {
  TRACE_EVENT0("chromeos", "OutputConfigurator::CycleDisplayMode");
  VLOG(1) << "CycleDisplayMode";
  if (!configure_display_)
    return false;

  bool did_change = false;
  Display* display = base::MessagePumpAuraX11::GetDefaultXDisplay();
  CHECK(display != NULL);
  XGrabServer(display);
  Window window = DefaultRootWindow(display);
  XRRScreenResources* screen = GetScreenResourcesAndRecordUMA(display, window);
  CHECK(screen != NULL);

  std::vector<OutputSnapshot> outputs = GetDualOutputs(display, screen);
  connected_output_count_ = outputs.size();
  OutputState original = InferCurrentState(display, screen, outputs);
  OutputState next_state = GetNextState(display, screen, original, outputs);
  if (original != next_state &&
      EnterState(display, screen, window, next_state, outputs)) {
    did_change = true;
  }
  // We have seen cases where the XRandR data can get out of sync with our own
  // cache so over-write it with whatever we detected, even if we didn't think
  // anything changed.
  output_state_ = next_state;

  XRRFreeScreenResources(screen);
  XUngrabServer(display);

  if (did_change)
    NotifyOnDisplayChanged();
  else
    FOR_EACH_OBSERVER(Observer, observers_, OnDisplayModeChangeFailed());
  return did_change;
}

bool OutputConfigurator::ScreenPowerSet(bool power_on, bool all_displays) {
  TRACE_EVENT0("chromeos", "OutputConfigurator::ScreenPowerSet");
  VLOG(1) << "OutputConfigurator::SetScreensOn " << power_on
          << " all displays " << all_displays;
  if (!configure_display_)
    return false;

  bool success = false;
  Display* display = base::MessagePumpAuraX11::GetDefaultXDisplay();
  CHECK(display != NULL);
  XGrabServer(display);
  Window window = DefaultRootWindow(display);
  XRRScreenResources* screen = GetScreenResourcesAndRecordUMA(display, window);
  CHECK(screen != NULL);

  std::vector<OutputSnapshot> outputs = GetDualOutputs(display, screen);
  connected_output_count_ = outputs.size();

  if (all_displays && power_on) {
    // Resume all displays using the current state.
    if (EnterState(display, screen, window, output_state_, outputs)) {
      // Force the DPMS on since the driver doesn't always detect that it should
      // turn on. This is needed when coming back from idle suspend.
      CHECK(DPMSEnable(display));
      CHECK(DPMSForceLevel(display, DPMSModeOn));

      XRRFreeScreenResources(screen);
      XUngrabServer(display);
      return true;
    }
  }

  CrtcConfig config;
  config.crtc = None;
  // Set the CRTCs based on whether we want to turn the power on or off and
  // select the outputs to operate on by name or all_displays.
  for (int i = 0; i < connected_output_count_; ++i) {
    if (all_displays || outputs[i].is_internal || power_on) {
      config.x = 0;
      config.y = outputs[i].y;
      config.output = outputs[i].output;
      config.mode = None;
      if (power_on) {
        config.mode = (output_state_ == STATE_DUAL_MIRROR) ?
            outputs[i].mirror_mode : outputs[i].native_mode;
      } else if (connected_output_count_ > 1 && !all_displays &&
                 outputs[i].is_internal) {
        // Workaround for crbug.com/148365: leave internal display in native
        // mode so user can move cursor (and hence windows) onto internal
        // display even when dimmed
        config.mode = outputs[i].native_mode;
      }
      config.crtc = GetNextCrtcAfter(display, screen, config.output,
                                     config.crtc);

      ConfigureCrtc(display, screen, &config);
      success = true;
    }
  }

  // Force the DPMS on since the driver doesn't always detect that it should
  // turn on. This is needed when coming back from idle suspend.
  if (power_on) {
    CHECK(DPMSEnable(display));
    CHECK(DPMSForceLevel(display, DPMSModeOn));
  }

  XRRFreeScreenResources(screen);
  XUngrabServer(display);

  return success;
}

bool OutputConfigurator::SetDisplayMode(OutputState new_state) {
  TRACE_EVENT0("chromeos", "OutputConfigurator::SetDisplayMode");
  if (output_state_ == STATE_INVALID ||
      output_state_ == STATE_HEADLESS ||
      output_state_ == STATE_SINGLE)
    return false;

  if (output_state_ == new_state)
    return true;

  Display* display = base::MessagePumpAuraX11::GetDefaultXDisplay();
  CHECK(display != NULL);
  XGrabServer(display);
  Window window = DefaultRootWindow(display);
  XRRScreenResources* screen = GetScreenResourcesAndRecordUMA(display, window);
  CHECK(screen != NULL);

  std::vector<OutputSnapshot> outputs = GetDualOutputs(display, screen);
  connected_output_count_ = outputs.size();
  if (EnterState(display, screen, window, new_state, outputs))
    output_state_ = new_state;

  XRRFreeScreenResources(screen);
  XUngrabServer(display);

  if (output_state_ == new_state)
    NotifyOnDisplayChanged();
  else
    FOR_EACH_OBSERVER(Observer, observers_, OnDisplayModeChangeFailed());
  return true;
}

bool OutputConfigurator::Dispatch(const base::NativeEvent& event) {
  TRACE_EVENT0("chromeos", "OutputConfigurator::Dispatch");
  if (event->type - xrandr_event_base_ == RRScreenChangeNotify)
    XRRUpdateConfiguration(event);
  // Ignore this event if the Xrandr extension isn't supported, or
  // the device is being shutdown.
  if (!configure_display_ ||
      (event->type - xrandr_event_base_ != RRNotify)) {
    return true;
  }
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

  // Ignore the case of RR_UnknownConnection.
  return true;
}

void OutputConfigurator::ConfigureOutputs() {
  TRACE_EVENT0("chromeos", "OutputConfigurator::ConfigureOutputs");
  configure_timer_.reset();

  Display* display = base::MessagePumpAuraX11::GetDefaultXDisplay();
  CHECK(display != NULL);
  XGrabServer(display);
  Window window = DefaultRootWindow(display);
  XRRScreenResources* screen = GetScreenResourcesAndRecordUMA(display, window);
  CHECK(screen != NULL);

  std::vector<OutputSnapshot> outputs = GetDualOutputs(display, screen);
  int new_output_count = outputs.size();
  // Don't skip even if the output counts didn't change because
  // a display might have been swapped during the suspend.
  connected_output_count_ = new_output_count;
  OutputState new_state =
      GetNextState(display, screen, STATE_INVALID, outputs);
  // When a display was swapped, the state moves from
  // STATE_DUAL_EXTENDED to STATE_DUAL_EXTENDED, so don't rely on
  // the state chagne to tell if it was successful.
  bool success = EnterState(display, screen, window, new_state, outputs);
  bool is_projecting = IsProjecting(outputs);
  XRRFreeScreenResources(screen);
  XUngrabServer(display);

  if (success) {
    output_state_ = new_state;
    NotifyOnDisplayChanged();
  }
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
      SetIsProjecting(is_projecting);
}

void OutputConfigurator::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void OutputConfigurator::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

// static
bool OutputConfigurator::IsInternalOutputName(const std::string& name) {
  return name.find(kInternal_LVDS) == 0 || name.find(kInternal_eDP) == 0;
}

void OutputConfigurator::SuspendDisplays() {
  // Turn displays on before suspend. At this point, the backlight is off,
  // so we turn on the internal display so that we can resume directly into
  // "on" state. This greatly reduces resume times.
  ScreenPowerSet(true, true);
  // We need to make sure that the monitor configuration we just did actually
  // completes before we return, because otherwise the X message could be
  // racing with the HandleSuspendReadiness message.
  XSync(base::MessagePumpAuraX11::GetDefaultXDisplay(), 0);
}

void OutputConfigurator::NotifyOnDisplayChanged() {
  TRACE_EVENT0("chromeos", "OutputConfigurator::NotifyOnDisplayChanged");
  FOR_EACH_OBSERVER(Observer, observers_, OnDisplayModeChanged());
}

std::vector<OutputSnapshot> OutputConfigurator::GetDualOutputs(
    Display* display,
    XRRScreenResources* screen) {
  std::vector<OutputSnapshot> outputs;
  XRROutputInfo* one_info = NULL;
  XRROutputInfo* two_info = NULL;

  for (int i = 0; (i < screen->noutput) && (outputs.size() < 2); ++i) {
    RROutput this_id = screen->outputs[i];
    XRROutputInfo* output_info = XRRGetOutputInfo(display, screen, this_id);
    bool is_connected = (output_info->connection == RR_Connected);

    if (is_connected) {
      OutputSnapshot to_populate;

      if (outputs.size() == 0) {
        one_info = output_info;
      } else {
        two_info = output_info;
      }

      to_populate.output = this_id;
      // Now, look up the corresponding CRTC and any related info.
      to_populate.crtc = output_info->crtc;
      if (None != to_populate.crtc) {
        XRRCrtcInfo* crtc_info =
            XRRGetCrtcInfo(display, screen, to_populate.crtc);
        to_populate.current_mode = crtc_info->mode;
        to_populate.height = crtc_info->height;
        to_populate.y = crtc_info->y;
        XRRFreeCrtcInfo(crtc_info);
      } else {
        to_populate.current_mode = 0;
        to_populate.height = 0;
        to_populate.y = 0;
      }
      // Find the native_mode and leave the mirror_mode for the pass after the
      // loop.
      to_populate.native_mode = GetOutputNativeMode(output_info);
      to_populate.mirror_mode = 0;

      // See if this output refers to an internal display.
      to_populate.is_internal = IsInternalOutput(output_info);

      to_populate.is_aspect_preserving_scaling =
          IsOutputAspectPreservingScaling(display, this_id);
      to_populate.touch_device_id = None;

      VLOG(1) << "Found display #" << outputs.size()
              << " with output " << (int)to_populate.output
              << " crtc " << (int)to_populate.crtc
              << " current mode " << (int)to_populate.current_mode;
      outputs.push_back(to_populate);
    } else {
      XRRFreeOutputInfo(output_info);
    }
  }

  if (outputs.size() == 2) {
    bool one_is_internal = IsInternalOutput(one_info);
    bool two_is_internal = IsInternalOutput(two_info);
    int internal_outputs = (one_is_internal ? 1 : 0) +
        (two_is_internal ? 1 : 0);

    DCHECK(internal_outputs < 2);
    LOG_IF(WARNING, internal_outputs == 2) << "Two internal outputs detected.";

    bool can_mirror = false;

    for (int attempt = 0; attempt < 2 && !can_mirror; attempt++) {
      // Try preserving external output's aspect ratio on the first attempt
      // If that fails, fall back to the highest matching resolution
      bool preserve_aspect = attempt == 0;

      if (internal_outputs == 1) {
        if (one_is_internal) {
          can_mirror = FindOrCreateMirrorMode(display,
                                              screen,
                                              one_info,
                                              two_info,
                                              outputs[0].output,
                                              is_panel_fitting_enabled_,
                                              preserve_aspect,
                                              &outputs[0].mirror_mode,
                                              &outputs[1].mirror_mode);
        } else {  // if (two_is_internal)
          can_mirror = FindOrCreateMirrorMode(display,
                                              screen,
                                              two_info,
                                              one_info,
                                              outputs[1].output,
                                              is_panel_fitting_enabled_,
                                              preserve_aspect,
                                              &outputs[1].mirror_mode,
                                              &outputs[0].mirror_mode);
        }
      } else {  // if (internal_outputs == 0)
        // No panel fitting for external outputs, so fall back to exact match
        can_mirror = FindOrCreateMirrorMode(display,
                                            screen,
                                            one_info,
                                            two_info,
                                            outputs[0].output,
                                            false,
                                            preserve_aspect,
                                            &outputs[0].mirror_mode,
                                            &outputs[1].mirror_mode);
        if (!can_mirror && preserve_aspect) {
          // FindOrCreateMirrorMode will try to preserve aspect ratio of
          // what it thinks is external display, so if it didn't succeed
          // with one, maybe it will succeed with the other.
          // This way we will have correct aspect ratio on at least one of them.
          can_mirror = FindOrCreateMirrorMode(display,
                                              screen,
                                              two_info,
                                              one_info,
                                              outputs[1].output,
                                              false,
                                              preserve_aspect,
                                              &outputs[1].mirror_mode,
                                              &outputs[0].mirror_mode);
        }
      }

      if (can_mirror) {
        if (preserve_aspect) {
          UMA_HISTOGRAM_ENUMERATION(
              "Display.GetDualOutputs.detected_mirror_mode",
              MIRROR_MODE_ASPECT_PRESERVING,
              MIRROR_MODE_TYPE_COUNT);
        } else {
          UMA_HISTOGRAM_ENUMERATION(
              "Display.GetDualOutputs.detected_mirror_mode",
              MIRROR_MODE_FALLBACK,
              MIRROR_MODE_TYPE_COUNT);
        }
        mirror_mode_will_preserve_aspect_ = preserve_aspect;
      }
    }

    if (!can_mirror) {
      // We can't mirror so set mirror_mode to None.
      outputs[0].mirror_mode = None;
      outputs[1].mirror_mode = None;

      UMA_HISTOGRAM_ENUMERATION(
          "Display.GetDualOutputs.detected_mirror_mode",
          MIRROR_MODE_NONE,
          MIRROR_MODE_TYPE_COUNT);
      mirror_mode_will_preserve_aspect_ = false;
    }
  }

  GetTouchscreens(display, screen, outputs);

  XRRFreeOutputInfo(one_info);
  XRRFreeOutputInfo(two_info);
  return outputs;
}

bool OutputConfigurator::FindOrCreateMirrorMode(Display* display,
                                                XRRScreenResources* screen,
                                                XRROutputInfo* internal_info,
                                                XRROutputInfo* external_info,
                                                RROutput internal_output_id,
                                                bool try_creating,
                                                bool preserve_aspect,
                                                RRMode* internal_mirror_mode,
                                                RRMode* external_mirror_mode) {
  RRMode internal_mode_id = GetOutputNativeMode(internal_info);
  RRMode external_mode_id = GetOutputNativeMode(external_info);

  if (internal_mode_id == None || external_mode_id == None)
    return false;

  XRRModeInfo* internal_native_mode = ModeInfoForID(screen, internal_mode_id);
  XRRModeInfo* external_native_mode = ModeInfoForID(screen, external_mode_id);

  // Check if some external output resolution can be mirrored on internal.
  // Prefer the modes in the order that X sorts them,
  // assuming this is the order in which they look better on the monitor.
  // If X's order is not satisfactory, we can either fix X's sorting,
  // or implement our sorting here.
  for (int i = 0; i < external_info->nmode; i++) {
    external_mode_id = external_info->modes[i];
    XRRModeInfo* external_mode = ModeInfoForID(screen, external_mode_id);
    bool is_native_aspect_ratio =
        external_native_mode->width * external_mode->height ==
        external_native_mode->height * external_mode->width;
    if (preserve_aspect && !is_native_aspect_ratio)
      continue;  // Allow only aspect ratio preserving modes for mirroring

    // Try finding exact match
    for (int j = 0; j < internal_info->nmode; j++) {
      internal_mode_id = internal_info->modes[j];
      XRRModeInfo* internal_mode = ModeInfoForID(screen, internal_mode_id);
      if (internal_mode->width == external_mode->width &&
          internal_mode->height == external_mode->height) {
        *internal_mirror_mode = internal_mode_id;
        *external_mirror_mode = external_mode_id;
        return true;  // Mirror mode found
      }
    }

    // Try to create a matching internal output mode by panel fitting
    if (try_creating) {
      // We can downscale by 1.125, and upscale indefinitely
      // Downscaling looks ugly, so, can fit == can upscale
      bool can_fit =
          internal_native_mode->width >= external_mode->width &&
          internal_native_mode->height >= external_mode->height;
      if (can_fit) {
        XRRAddOutputMode(display, internal_output_id, external_mode_id);
        *internal_mirror_mode = *external_mirror_mode = external_mode_id;
        return true;  // Mirror mode created
      }
    }
  }

  return false;
}

void OutputConfigurator::GetTouchscreens(Display* display,
                                         XRRScreenResources* screen,
                                         std::vector<OutputSnapshot>& outputs) {
  int ndevices = 0;
  Atom valuator_x = XInternAtom(display, "Abs MT Position X", False);
  Atom valuator_y = XInternAtom(display, "Abs MT Position Y", False);
  if (valuator_x == None || valuator_y == None)
    return;

  XIDeviceInfo* info = XIQueryDevice(display, XIAllDevices, &ndevices);
  for (int i = 0; i < ndevices; i++) {
    if (!info[i].enabled || info[i].use != XIFloatingSlave)
      continue;  // Assume all touchscreens are floating slaves

    double width = -1.0;
    double height = -1.0;
    bool is_direct_touch = false;

    for (int j = 0; j < info[i].num_classes; j++) {
      XIAnyClassInfo* class_info = info[i].classes[j];

      if (class_info->type == XIValuatorClass) {
        XIValuatorClassInfo* valuator_info =
            reinterpret_cast<XIValuatorClassInfo*>(class_info);

        if (valuator_x == valuator_info->label) {
          // Ignore X axis valuator with unexpected properties
          if (valuator_info->number == 0 && valuator_info->mode == Absolute &&
              valuator_info->min == 0.0) {
            width = valuator_info->max;
          }
        } else if (valuator_y == valuator_info->label) {
          // Ignore Y axis valuator with unexpected properties
          if (valuator_info->number == 1 && valuator_info->mode == Absolute &&
              valuator_info->min == 0.0) {
            height = valuator_info->max;
          }
        }
      }
#if defined(USE_XI2_MT)
      if (class_info->type == XITouchClass) {
        XITouchClassInfo* touch_info =
            reinterpret_cast<XITouchClassInfo*>(class_info);
        is_direct_touch = touch_info->mode == XIDirectTouch;
      }
#endif
    }

    // Touchscreens should have absolute X and Y axes,
    // and be direct touch devices.
    if (width > 0.0 && height > 0.0 && is_direct_touch) {
      size_t k = 0;
      for (; k < outputs.size(); k++) {
        if (outputs[k].native_mode == None ||
            outputs[k].touch_device_id != None)
          continue;
        XRRModeInfo* native_mode = ModeInfoForID(screen,
                                                 outputs[k].native_mode);
        if (native_mode == NULL)
          continue;

        // Allow 1 pixel difference between screen and touchscreen resolutions.
        // Because in some cases for monitor resolution 1024x768 touchscreen's
        // resolution would be 1024x768, but for some 1023x767.
        // It really depends on touchscreen's firmware configuration.
        if (std::abs(native_mode->width - width) <= 1.0 &&
            std::abs(native_mode->height - height) <= 1.0) {
          outputs[k].touch_device_id = info[i].deviceid;

          VLOG(1) << "Found touchscreen for output #" << k
                  << " id " << outputs[k].touch_device_id
                  << " width " << width
                  << " height " << height;
          break;
        }
      }

      VLOG_IF(1, k == outputs.size())
          << "No matching output - ignoring touchscreen id " << info[i].deviceid
          << " width " << width
          << " height " << height;
    }
  }

  XIFreeDeviceInfo(info);
}

bool OutputConfigurator::EnterState(
    Display* display,
    XRRScreenResources* screen,
    Window window,
    OutputState new_state,
    const std::vector<OutputSnapshot>& outputs) {
  TRACE_EVENT0("chromeos", "OutputConfigurator::EnterState");
  switch (outputs.size()) {
    case 0:
      // Do nothing as no 0-display states are supported.
      break;
    case 1: {
      // Re-allocate the framebuffer to fit.
      XRRModeInfo* mode_info = ModeInfoForID(screen, outputs[0].native_mode);
      if (mode_info == NULL) {
        UMA_HISTOGRAM_COUNTS("Display.EnterState.single_failures", 1);
        return false;
      }

      CrtcConfig config(
          GetNextCrtcAfter(display, screen, outputs[0].output, None),
          0, 0, outputs[0].native_mode, outputs[0].output);

      int width = mode_info->width;
      int height = mode_info->height;
      CreateFrameBuffer(display, screen, window, width, height, &config, NULL);

      // Re-attach native mode for the CRTC.
      ConfigureCrtc(display, screen, &config);
      // Restore identity transformation for single monitor in native mode.
      if (outputs[0].touch_device_id != None) {
        CoordinateTransformation ctm;  // Defaults to identity
        ConfigureCTM(display, outputs[0].touch_device_id, ctm);
      }
      break;
    }
    case 2: {
      RRCrtc primary_crtc =
          GetNextCrtcAfter(display, screen, outputs[0].output, None);
      RRCrtc secondary_crtc =
          GetNextCrtcAfter(display, screen, outputs[1].output, primary_crtc);

      if (new_state == STATE_DUAL_MIRROR) {
        XRRModeInfo* mode_info = ModeInfoForID(screen, outputs[0].mirror_mode);
        if (mode_info == NULL) {
          UMA_HISTOGRAM_COUNTS("Display.EnterState.mirror_failures", 1);
          return false;
        }

        CrtcConfig config1(primary_crtc, 0, 0, outputs[0].mirror_mode,
                           outputs[0].output);
        CrtcConfig config2(secondary_crtc, 0, 0, outputs[1].mirror_mode,
                           outputs[1].output);

        int width = mode_info->width;
        int height = mode_info->height;
        CreateFrameBuffer(display, screen, window, width, height,
                          &config1, &config2);

        ConfigureCrtc(display, screen, &config1);
        ConfigureCrtc(display, screen, &config2);
        for (size_t i = 0; i < outputs.size(); i++) {
          if (outputs[i].touch_device_id == None)
            continue;

          CoordinateTransformation ctm;
          // CTM needs to be calculated if aspect preserving scaling is used.
          // Otherwise, assume it is full screen, and use identity CTM.
          if (outputs[i].mirror_mode != outputs[i].native_mode &&
              outputs[i].is_aspect_preserving_scaling) {
            ctm = GetMirrorModeCTM(screen, &outputs[i]);
          }
          ConfigureCTM(display, outputs[i].touch_device_id, ctm);
        }
      } else {
        XRRModeInfo* primary_mode_info =
            ModeInfoForID(screen, outputs[0].native_mode);
        XRRModeInfo* secondary_mode_info =
            ModeInfoForID(screen, outputs[1].native_mode);
        if (primary_mode_info == NULL || secondary_mode_info == NULL) {
          UMA_HISTOGRAM_COUNTS("Display.EnterState.dual_failures", 1);
          return false;
        }

        int primary_height = primary_mode_info->height;
        int secondary_height = secondary_mode_info->height;
        CrtcConfig config1(primary_crtc, 0, 0, outputs[0].native_mode,
                           outputs[0].output);
        CrtcConfig config2(secondary_crtc, 0, 0, outputs[1].native_mode,
                           outputs[1].output);

        if (new_state == STATE_DUAL_EXTENDED)
          config2.y = primary_height + kVerticalGap;
        else
          config1.y = secondary_height + kVerticalGap;


        int width =
            std::max<int>(primary_mode_info->width, secondary_mode_info->width);
        int height = primary_height + secondary_height + kVerticalGap;

        CreateFrameBuffer(display, screen, window, width, height, &config1,
                          &config2);

        ConfigureCrtc(display, screen, &config1);
        ConfigureCrtc(display, screen, &config2);

        if (outputs[0].touch_device_id != None) {
          CoordinateTransformation ctm;
          ctm.x_scale = static_cast<float>(primary_mode_info->width) / width;
          ctm.x_offset = static_cast<float>(config1.x) / width;
          ctm.y_scale = static_cast<float>(primary_height) / height;
          ctm.y_offset = static_cast<float>(config1.y) / height;
          ConfigureCTM(display, outputs[0].touch_device_id, ctm);
        }
        if (outputs[1].touch_device_id != None) {
          CoordinateTransformation ctm;
          ctm.x_scale = static_cast<float>(secondary_mode_info->width) / width;
          ctm.x_offset = static_cast<float>(config2.x) / width;
          ctm.y_scale = static_cast<float>(secondary_height) / height;
          ctm.y_offset = static_cast<float>(config2.y) / height;
          ConfigureCTM(display, outputs[1].touch_device_id, ctm);
        }
      }
      break;
    }
    default:
      CHECK(false);
  }

  RecordPreviousStateUMA();

  return true;
}

void OutputConfigurator::RecordPreviousStateUMA() {
  base::TimeDelta duration = base::TimeTicks::Now() - last_enter_state_time_;

  // |output_state_| can be used for the state being left,
  // since RecordPreviousStateUMA is called from EnterState,
  // and |output_state_| is always updated after EnterState is called.
  switch (output_state_) {
    case STATE_SINGLE:
      UMA_HISTOGRAM_LONG_TIMES("Display.EnterState.single_duration", duration);
      break;
    case STATE_DUAL_MIRROR:
      if (mirror_mode_preserved_aspect_)
        UMA_HISTOGRAM_LONG_TIMES("Display.EnterState.mirror_aspect_duration",
                                 duration);
      else
        UMA_HISTOGRAM_LONG_TIMES("Display.EnterState.mirror_fallback_duration",
                                 duration);
      break;
    case STATE_DUAL_EXTENDED:
      UMA_HISTOGRAM_LONG_TIMES("Display.EnterState.dual_primary_duration",
                               duration);
      break;
    default:
      break;
  }

  mirror_mode_preserved_aspect_ = mirror_mode_will_preserve_aspect_;
  last_enter_state_time_ = base::TimeTicks::Now();
}

// static
bool OutputConfigurator::IsInternalOutput(const XRROutputInfo* output_info) {
  return IsInternalOutputName(std::string(output_info->name));
}

// static
RRMode OutputConfigurator::GetOutputNativeMode(
    const XRROutputInfo* output_info) {
  if (output_info->nmode <= 0)
    return None;

  return output_info->modes[0];
}

}  // namespace chromeos
