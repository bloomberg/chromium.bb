// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/display/real_output_configurator_delegate.h"

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/dpms.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/Xrandr.h>

#include <cmath>
#include <set>
#include <utility>

#include "base/logging.h"
#include "base/message_loop/message_pump_x11.h"
#include "base/x11/edid_parser_x11.h"
#include "base/x11/x11_error_tracker.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/display/output_util.h"

namespace chromeos {

namespace {

// DPI measurements.
const float kMmInInch = 25.4;
const float kDpi96 = 96.0;
const float kPixelsToMmScale = kMmInInch / kDpi96;

// Prefixes of output name
const char kOutputName_VGA[] = "VGA";
const char kOutputName_HDMI[] = "HDMI";
const char kOutputName_DVI[] = "DVI";
const char kOutputName_DisplayPort[] = "DP";

const char kContentProtectionAtomName[] = "Content Protection";
const char kProtectionUndesiredAtomName[] = "Undesired";
const char kProtectionDesiredAtomName[] = "Desired";
const char kProtectionEnabledAtomName[] = "Enabled";

bool IsInternalOutput(const XRROutputInfo* output_info) {
  return IsInternalOutputName(std::string(output_info->name));
}

RRMode GetOutputNativeMode(const XRROutputInfo* output_info) {
  return output_info->nmode > 0 ? output_info->modes[0] : None;
}

}  // namespace

RealOutputConfiguratorDelegate::RealOutputConfiguratorDelegate()
    : display_(base::MessagePumpX11::GetDefaultXDisplay()),
      window_(DefaultRootWindow(display_)),
      screen_(NULL) {
}

RealOutputConfiguratorDelegate::~RealOutputConfiguratorDelegate() {
}

void RealOutputConfiguratorDelegate::InitXRandRExtension(int* event_base) {
  int error_base_ignored = 0;
  XRRQueryExtension(display_, event_base, &error_base_ignored);
}

void RealOutputConfiguratorDelegate::UpdateXRandRConfiguration(
    const base::NativeEvent& event) {
  XRRUpdateConfiguration(event);
}

void RealOutputConfiguratorDelegate::GrabServer() {
  CHECK(!screen_) << "Server already grabbed";
  XGrabServer(display_);
  screen_ = XRRGetScreenResources(display_, window_);
  CHECK(screen_);
}

void RealOutputConfiguratorDelegate::UngrabServer() {
  CHECK(screen_) << "Server not grabbed";
  XRRFreeScreenResources(screen_);
  screen_ = NULL;
  XUngrabServer(display_);
}

void RealOutputConfiguratorDelegate::SyncWithServer() {
  XSync(display_, 0);
}

void RealOutputConfiguratorDelegate::SetBackgroundColor(uint32 color_argb) {
  // Configuring CRTCs/Framebuffer clears the boot screen image.  Set the
  // same background color while configuring the display to minimize the
  // duration of black screen at boot time. The background is filled with
  // black later in ash::DisplayManager.  crbug.com/171050.
  XSetWindowAttributes swa = {0};
  XColor color;
  Colormap colormap = DefaultColormap(display_, 0);
  // XColor uses 16 bits per color.
  color.red = (color_argb & 0x00FF0000) >> 8;
  color.green = (color_argb & 0x0000FF00);
  color.blue = (color_argb & 0x000000FF) << 8;
  color.flags = DoRed | DoGreen | DoBlue;
  XAllocColor(display_, colormap, &color);
  swa.background_pixel = color.pixel;
  XChangeWindowAttributes(display_, window_, CWBackPixel, &swa);
  XFreeColors(display_, colormap, &color.pixel, 1, 0);
}

void RealOutputConfiguratorDelegate::ForceDPMSOn() {
  CHECK(DPMSEnable(display_));
  CHECK(DPMSForceLevel(display_, DPMSModeOn));
}

std::vector<OutputConfigurator::OutputSnapshot>
RealOutputConfiguratorDelegate::GetOutputs() {
  CHECK(screen_) << "Server not grabbed";

  std::vector<OutputConfigurator::OutputSnapshot> outputs;
  RRCrtc last_used_crtc = None;

  for (int i = 0; i < screen_->noutput && outputs.size() < 2; ++i) {
    RROutput output_id = screen_->outputs[i];
    XRROutputInfo* output_info = XRRGetOutputInfo(display_, screen_, output_id);
    if (output_info->connection == RR_Connected) {
      OutputConfigurator::OutputSnapshot output = InitOutputSnapshot(
          output_id, output_info, &last_used_crtc, i);
      VLOG(2) << "Found display " << outputs.size() << ":"
              << " output=" << output.output
              << " crtc=" << output.crtc
              << " current_mode=" << output.current_mode;
      outputs.push_back(output);
    }
    XRRFreeOutputInfo(output_info);
  }

  GetTouchscreens(&outputs);
  return outputs;
}

void RealOutputConfiguratorDelegate::AddOutputMode(RROutput output,
                                                   RRMode mode) {
  CHECK(screen_) << "Server not grabbed";
  VLOG(1) << "AddOutputMode: output=" << output << " mode=" << mode;
  XRRAddOutputMode(display_, output, mode);
}

bool RealOutputConfiguratorDelegate::ConfigureCrtc(
    RRCrtc crtc,
    RRMode mode,
    RROutput output,
    int x,
    int y) {
  CHECK(screen_) << "Server not grabbed";
  VLOG(1) << "ConfigureCrtc: crtc=" << crtc
          << " mode=" << mode
          << " output=" << output
          << " x=" << x
          << " y=" << y;
  // Xrandr.h is full of lies. XRRSetCrtcConfig() is defined as returning a
  // Status, which is typically 0 for failure and 1 for success. In
  // actuality it returns a RRCONFIGSTATUS, which uses 0 for success.
  return XRRSetCrtcConfig(display_,
                          screen_,
                          crtc,
                          CurrentTime,
                          x,
                          y,
                          mode,
                          RR_Rotate_0,
                          (output && mode) ? &output : NULL,
                          (output && mode) ? 1 : 0) == RRSetConfigSuccess;
}

void RealOutputConfiguratorDelegate::CreateFrameBuffer(
    int width,
    int height,
    const std::vector<OutputConfigurator::OutputSnapshot>& outputs) {
  CHECK(screen_) << "Server not grabbed";
  int current_width = DisplayWidth(display_, DefaultScreen(display_));
  int current_height = DisplayHeight(display_, DefaultScreen(display_));
  VLOG(1) << "CreateFrameBuffer: new=" << width << "x" << height
          << " current=" << current_width << "x" << current_height;
  if (width ==  current_width && height == current_height)
    return;

  DestroyUnusedCrtcs(outputs);
  int mm_width = width * kPixelsToMmScale;
  int mm_height = height * kPixelsToMmScale;
  XRRSetScreenSize(display_, window_, width, height, mm_width, mm_height);
}

void RealOutputConfiguratorDelegate::ConfigureCTM(
    int touch_device_id,
    const OutputConfigurator::CoordinateTransformation& ctm) {
  VLOG(1) << "ConfigureCTM: id=" << touch_device_id
          << " scale=" << ctm.x_scale << "x" << ctm.y_scale
          << " offset=(" << ctm.x_offset << ", " << ctm.y_offset << ")";
  int ndevices = 0;
  XIDeviceInfo* info = XIQueryDevice(display_, touch_device_id, &ndevices);
  Atom prop = XInternAtom(display_, "Coordinate Transformation Matrix", False);
  Atom float_atom = XInternAtom(display_, "FLOAT", False);
  if (ndevices == 1 && prop != None && float_atom != None) {
    Atom type;
    int format;
    unsigned long num_items;
    unsigned long bytes_after;
    unsigned char* data = NULL;
    // Verify that the property exists with correct format, type, etc.
    int status = XIGetProperty(display_, info->deviceid, prop, 0, 0, False,
        AnyPropertyType, &type, &format, &num_items, &bytes_after, &data);
    if (data)
      XFree(data);
    if (status == Success && type == float_atom && format == 32) {
      float value[3][3] = {
          { ctm.x_scale,         0.0, ctm.x_offset },
          {         0.0, ctm.y_scale, ctm.y_offset },
          {         0.0,         0.0,          1.0 }
      };
      XIChangeProperty(display_,
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

void RealOutputConfiguratorDelegate::SendProjectingStateToPowerManager(
    bool projecting) {
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
      SetIsProjecting(projecting);
}

bool RealOutputConfiguratorDelegate::InitModeInfo(
    RRMode mode,
    OutputConfigurator::ModeInfo* mode_info) {
  DCHECK(mode_info);
  CHECK(screen_) << "Server not grabbed";
  // TODO: Determine if we need to organize modes in a way which provides
  // better than O(n) lookup time.  In many call sites, for example, the
  // "next" mode is typically what we are looking for so using this
  // helper might be too expensive.
  for (int i = 0; i < screen_->nmode; ++i) {
    if (mode == screen_->modes[i].id) {
      const XRRModeInfo& info = screen_->modes[i];
      mode_info->width = info.width;
      mode_info->height = info.height;
      mode_info->interlaced = info.modeFlags & RR_Interlace;
      if (info.hTotal && info.vTotal) {
        mode_info->refresh_rate = static_cast<float>(info.dotClock) /
            (static_cast<float>(info.hTotal) *
             static_cast<float>(info.vTotal));
      } else {
        mode_info->refresh_rate = 0.0f;
      }
      return true;
    }
  }
  return false;
}

OutputConfigurator::OutputSnapshot
RealOutputConfiguratorDelegate::InitOutputSnapshot(
    RROutput id,
    XRROutputInfo* info,
    RRCrtc* last_used_crtc,
    int index) {
  OutputConfigurator::OutputSnapshot output;
  output.output = id;
  output.width_mm = info->mm_width;
  output.height_mm = info->mm_height;
  output.has_display_id = base::GetDisplayId(id, index, &output.display_id);
  output.is_internal = IsInternalOutput(info);
  output.index = index;

  // Use the index as a valid display ID even if the internal
  // display doesn't have valid EDID because the index
  // will never change.
  if (!output.has_display_id && output.is_internal)
    output.has_display_id = true;

  if (info->crtc) {
    XRRCrtcInfo* crtc_info = XRRGetCrtcInfo(display_, screen_, info->crtc);
    output.current_mode = crtc_info->mode;
    output.x = crtc_info->x;
    output.y = crtc_info->y;
    XRRFreeCrtcInfo(crtc_info);
  }

  // Assign a CRTC that isn't already in use.
  for (int i = 0; i < info->ncrtc; ++i) {
    if (info->crtcs[i] != *last_used_crtc) {
      output.crtc = info->crtcs[i];
      *last_used_crtc = output.crtc;
      break;
    }
  }

  output.native_mode = GetOutputNativeMode(info);
  output.is_aspect_preserving_scaling = IsOutputAspectPreservingScaling(id);
  output.touch_device_id = None;

  for (int i = 0; i < info->nmode; ++i) {
    const RRMode mode = info->modes[i];
    OutputConfigurator::ModeInfo mode_info;
    if (InitModeInfo(mode, &mode_info))
      output.mode_infos.insert(std::make_pair(mode, mode_info));
    else
      LOG(WARNING) << "Unable to find XRRModeInfo for mode " << mode;
  }

  std::string name(info->name);
  if (output.is_internal) {
    output.type = OUTPUT_TYPE_INTERNAL;
  } else if (name.find(kOutputName_VGA) == 0) {
    output.type = OUTPUT_TYPE_VGA;
  } else if (name.find(kOutputName_HDMI) == 0) {
    output.type = OUTPUT_TYPE_HDMI;
  } else if (name.find(kOutputName_DVI) == 0) {
    output.type = OUTPUT_TYPE_DVI;
  } else if (name.find(kOutputName_DisplayPort) == 0) {
    output.type = OUTPUT_TYPE_DISPLAYPORT;
  } else {
    LOG(ERROR) << "Unknown link type: " << name;
    output.type = OUTPUT_TYPE_UNKNOWN;
  }

  return output;
}

bool RealOutputConfiguratorDelegate::GetHDCPState(RROutput id,
                                                  HDCPState* state) {
  unsigned char* values = NULL;
  int actual_format = 0;
  unsigned long nitems = 0;
  unsigned long bytes_after = 0;
  Atom actual_type = None;
  int success = 0;
  // TODO(kcwu): Use X11AtomCache to save round trip time of XInternAtom.
  Atom prop = XInternAtom(display_, kContentProtectionAtomName, False);

  bool ok = true;
  // TODO(kcwu): Move this to x11_util (similar method calls in this file and
  // output_util.cc)
  success = XRRGetOutputProperty(display_, id, prop, 0, 100, False,
                                 False, AnyPropertyType, &actual_type,
                                 &actual_format, &nitems, &bytes_after,
                                 &values);
  if (actual_type == None) {
    LOG(ERROR) << "Property '" << kContentProtectionAtomName
               << "' does not exist";
    ok = false;
  } else if (success == Success && actual_type == XA_ATOM &&
             actual_format == 32 && nitems == 1) {
    Atom value = reinterpret_cast<Atom*>(values)[0];
    if (value == XInternAtom(display_, kProtectionUndesiredAtomName, False)) {
      *state = HDCP_STATE_UNDESIRED;
    } else if (value == XInternAtom(display_, kProtectionDesiredAtomName,
                                    False)) {
      *state = HDCP_STATE_DESIRED;
    } else if (value == XInternAtom(display_, kProtectionEnabledAtomName,
                                    False)) {
      *state = HDCP_STATE_ENABLED;
    } else {
      LOG(ERROR) << "Unknown " << kContentProtectionAtomName << " value: "
                 << value;
      ok = false;
    }
  } else {
    LOG(ERROR) << "XRRGetOutputProperty failed";
    ok = false;
  }
  if (values)
    XFree(values);

  VLOG(3) << "HDCP state: " << ok << "," << *state;
  return ok;
}

bool RealOutputConfiguratorDelegate::SetHDCPState(RROutput id,
                                                  HDCPState state) {
  Atom name = XInternAtom(display_, kContentProtectionAtomName, False);
  Atom value = None;
  switch (state) {
    case HDCP_STATE_UNDESIRED:
      value = XInternAtom(display_, kProtectionUndesiredAtomName, False);
      break;
    case HDCP_STATE_DESIRED:
      value = XInternAtom(display_, kProtectionDesiredAtomName, False);
      break;
    default:
      NOTREACHED() << "Invalid HDCP state: " << state;
      return false;
  }
  base::X11ErrorTracker err_tracker;
  unsigned char* data = reinterpret_cast<unsigned char*>(&value);
  XRRChangeOutputProperty(display_, id, name, XA_ATOM, 32,
                          PropModeReplace, data, 1);
  if (err_tracker.FoundNewError()) {
    LOG(ERROR) << "XRRChangeOutputProperty failed";
    return false;
  } else {
    return true;
  }
}

void RealOutputConfiguratorDelegate::DestroyUnusedCrtcs(
    const std::vector<OutputConfigurator::OutputSnapshot>& outputs) {
  CHECK(screen_) << "Server not grabbed";
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
  for (int i = 0; i < screen_->ncrtc; ++i) {
    // Default config is to disable the crtcs.
    RRCrtc crtc = screen_->crtcs[i];
    RRMode mode = None;
    RROutput output = None;
    const OutputConfigurator::ModeInfo* mode_info = NULL;
    for (std::vector<OutputConfigurator::OutputSnapshot>::const_iterator it =
         outputs.begin(); it != outputs.end(); ++it) {
      if (crtc == it->crtc) {
        mode = it->current_mode;
        output = it->output;
        if (mode != None)
          mode_info = OutputConfigurator::GetModeInfo(*it, mode);
        break;
      }
    }

    if (mode_info) {
      // In case our CRTC doesn't fit in our current framebuffer, disable it.
      // It'll get reenabled after we resize the framebuffer.
      int current_width = DisplayWidth(display_, DefaultScreen(display_));
      int current_height = DisplayHeight(display_, DefaultScreen(display_));
      if (mode_info->width > current_width ||
          mode_info->height > current_height) {
        mode = None;
        output = None;
        mode_info = NULL;
      }
    }

    ConfigureCrtc(crtc, mode, output, 0, 0);
  }
}

bool RealOutputConfiguratorDelegate::IsOutputAspectPreservingScaling(
    RROutput id) {
  bool ret = false;

  Atom scaling_prop = XInternAtom(display_, "scaling mode", False);
  Atom full_aspect_atom = XInternAtom(display_, "Full aspect", False);
  if (scaling_prop == None || full_aspect_atom == None)
    return false;

  int nprop = 0;
  Atom* props = XRRListOutputProperties(display_, id, &nprop);
  for (int j = 0; j < nprop && !ret; j++) {
    Atom prop = props[j];
    if (scaling_prop == prop) {
      unsigned char* values = NULL;
      int actual_format;
      unsigned long nitems;
      unsigned long bytes_after;
      Atom actual_type;
      int success;

      success = XRRGetOutputProperty(display_, id, prop, 0, 100, False, False,
          AnyPropertyType, &actual_type, &actual_format, &nitems,
          &bytes_after, &values);
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

void RealOutputConfiguratorDelegate::GetTouchscreens(
    std::vector<OutputConfigurator::OutputSnapshot>* outputs) {
  int ndevices = 0;
  Atom valuator_x = XInternAtom(display_, "Abs MT Position X", False);
  Atom valuator_y = XInternAtom(display_, "Abs MT Position Y", False);
  if (valuator_x == None || valuator_y == None)
    return;

  std::set<int> no_match_touchscreen;
  XIDeviceInfo* info = XIQueryDevice(display_, XIAllDevices, &ndevices);
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
      for (; k < outputs->size(); k++) {
        OutputConfigurator::OutputSnapshot* output = &(*outputs)[k];
        if (output->native_mode == None || output->touch_device_id != None)
          continue;

        const OutputConfigurator::ModeInfo* mode_info =
            OutputConfigurator::GetModeInfo(*output, output->native_mode);
        if (!mode_info)
          continue;

        // Allow 1 pixel difference between screen and touchscreen
        // resolutions.  Because in some cases for monitor resolution
        // 1024x768 touchscreen's resolution would be 1024x768, but for
        // some 1023x767.  It really depends on touchscreen's firmware
        // configuration.
        if (std::abs(mode_info->width - width) <= 1.0 &&
            std::abs(mode_info->height - height) <= 1.0) {
          output->touch_device_id = info[i].deviceid;

          VLOG(2) << "Found touchscreen for output #" << k
                  << " id " << output->touch_device_id
                  << " width " << width
                  << " height " << height;
          break;
        }
      }

      if (k == outputs->size()) {
        no_match_touchscreen.insert(info[i].deviceid);
        VLOG(2) << "No matching output for touchscreen"
                << " id " << info[i].deviceid
                << " width " << width
                << " height " << height;
      }

    }
  }

  // Sometimes we can't find a matching screen for the touchscreen, e.g.
  // due to the touchscreen's reporting range having no correlation with the
  // screen's resolution. In this case, we arbitrarily assign unmatched
  // touchscreens to unmatched screens.
  for (std::set<int>::iterator it = no_match_touchscreen.begin();
       it != no_match_touchscreen.end();
       it++) {
    for (size_t i = 0; i < outputs->size(); i++) {
      if ((*outputs)[i].is_internal == false &&
          (*outputs)[i].native_mode != None &&
          (*outputs)[i].touch_device_id == None ) {
        (*outputs)[i].touch_device_id = *it;
        VLOG(2) << "Arbitrarily matching touchscreen "
                << (*outputs)[i].touch_device_id << " to output #" << i;
        break;
      }
    }
  }

  XIFreeDeviceInfo(info);
}

}  // namespace chromeos
