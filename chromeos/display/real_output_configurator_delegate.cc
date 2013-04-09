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

#include "base/logging.h"
#include "base/message_pump_aurax11.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"

namespace chromeos {

namespace {

// DPI measurements.
const float kMmInInch = 25.4;
const float kDpi96 = 96.0;
const float kPixelsToMmScale = kMmInInch / kDpi96;

bool IsInternalOutput(const XRROutputInfo* output_info) {
  return OutputConfigurator::IsInternalOutputName(
      std::string(output_info->name));
}

RRMode GetOutputNativeMode(const XRROutputInfo* output_info) {
  return output_info->nmode > 0 ? output_info->modes[0] : None;
}

}  // namespace

RealOutputConfiguratorDelegate::RealOutputConfiguratorDelegate()
    : display_(base::MessagePumpAuraX11::GetDefaultXDisplay()),
      window_(DefaultRootWindow(display_)),
      screen_(NULL),
      is_panel_fitting_enabled_(false) {
}

RealOutputConfiguratorDelegate::~RealOutputConfiguratorDelegate() {
}

void RealOutputConfiguratorDelegate::SetPanelFittingEnabled(bool enabled) {
  is_panel_fitting_enabled_ = enabled;
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
  XRROutputInfo* one_info = NULL;
  XRROutputInfo* two_info = NULL;
  RRCrtc last_used_crtc = None;

  for (int i = 0; i < screen_->noutput && outputs.size() < 2; ++i) {
    RROutput this_id = screen_->outputs[i];
    XRROutputInfo* output_info = XRRGetOutputInfo(display_, screen_, this_id);
    bool is_connected = (output_info->connection == RR_Connected);

    if (is_connected) {
      OutputConfigurator::OutputSnapshot to_populate;
      to_populate.output = this_id;
      (outputs.empty() ? one_info : two_info) = output_info;

      // Now, look up the current CRTC and any related info.
      if (output_info->crtc) {
        XRRCrtcInfo* crtc_info = XRRGetCrtcInfo(
            display_, screen_, output_info->crtc);
        to_populate.current_mode = crtc_info->mode;
        to_populate.height = crtc_info->height;
        to_populate.y = crtc_info->y;
        XRRFreeCrtcInfo(crtc_info);
      }

      // Assign a CRTC that isn't already in use.
      for (int j = 0; j < output_info->ncrtc; ++j) {
        if (output_info->crtcs[j] != last_used_crtc) {
          to_populate.crtc = output_info->crtcs[j];
          last_used_crtc = to_populate.crtc;
          break;
        }
      }

      to_populate.native_mode = GetOutputNativeMode(output_info);
      to_populate.is_internal = IsInternalOutput(output_info);
      to_populate.is_aspect_preserving_scaling =
          IsOutputAspectPreservingScaling(this_id);
      to_populate.touch_device_id = None;

      VLOG(1) << "Found display #" << outputs.size()
              << " with output " << to_populate.output
              << " crtc " << to_populate.crtc
              << " current mode " << to_populate.current_mode;
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
    DCHECK_LT(internal_outputs, 2);
    LOG_IF(WARNING, internal_outputs == 2)
        << "Two internal outputs detected.";

    bool can_mirror = false;
    for (int attempt = 0; !can_mirror && attempt < 2; ++attempt) {
      // Try preserving external output's aspect ratio on the first attempt.
      // If that fails, fall back to the highest matching resolution.
      bool preserve_aspect = attempt == 0;

      if (internal_outputs == 1) {
        if (one_is_internal) {
          can_mirror = FindOrCreateMirrorMode(one_info, two_info,
              outputs[0].output, is_panel_fitting_enabled_, preserve_aspect,
              &outputs[0].mirror_mode, &outputs[1].mirror_mode);
        } else {  // if (two_is_internal)
          can_mirror = FindOrCreateMirrorMode(two_info, one_info,
              outputs[1].output, is_panel_fitting_enabled_, preserve_aspect,
              &outputs[1].mirror_mode, &outputs[0].mirror_mode);
        }
      } else {  // if (internal_outputs == 0)
        // No panel fitting for external outputs, so fall back to exact match.
        can_mirror = FindOrCreateMirrorMode(one_info, two_info,
            outputs[0].output, false, preserve_aspect,
            &outputs[0].mirror_mode, &outputs[1].mirror_mode);
        if (!can_mirror && preserve_aspect) {
          // FindOrCreateMirrorMode will try to preserve aspect ratio of
          // what it thinks is external display, so if it didn't succeed
          // with one, maybe it will succeed with the other.  This way we
          // will have correct aspect ratio on at least one of them.
          can_mirror = FindOrCreateMirrorMode(two_info, one_info,
              outputs[1].output, false, preserve_aspect,
              &outputs[1].mirror_mode, &outputs[0].mirror_mode);
        }
      }
    }
  }

  GetTouchscreens(&outputs);
  XRRFreeOutputInfo(one_info);
  XRRFreeOutputInfo(two_info);
  return outputs;
}

bool RealOutputConfiguratorDelegate::GetModeDetails(RRMode mode,
                                                    int* width,
                                                    int* height,
                                                    bool* interlaced) {
  CHECK(screen_) << "Server not grabbed";
  // TODO: Determine if we need to organize modes in a way which provides
  // better than O(n) lookup time.  In many call sites, for example, the
  // "next" mode is typically what we are looking for so using this
  // helper might be too expensive.
  for (int i = 0; i < screen_->nmode; ++i) {
    if (mode == screen_->modes[i].id) {
      const XRRModeInfo& info = screen_->modes[i];
      if (width)
        *width = info.width;
      if (height)
        *height = info.height;
      if (interlaced)
        *interlaced = info.modeFlags & RR_Interlace;
      return true;
    }
  }
  return false;
}

void RealOutputConfiguratorDelegate::ConfigureCrtc(
    OutputConfigurator::CrtcConfig* config) {
  VLOG(1) << "ConfigureCrtc crtc: " << config->crtc
          << ", mode " << config->mode
          << ", output " << config->output
          << ", x " << config->x
          << ", y " << config->y;
  CHECK(screen_) << "Server not grabbed";

  RROutput* outputs = NULL;
  int num_outputs = 0;
  if (config->output && config->mode) {
    outputs = &config->output;
    num_outputs = 1;
  }

  XRRSetCrtcConfig(display_,
                   screen_,
                   config->crtc,
                   CurrentTime,
                   config->x,
                   config->y,
                   config->mode,
                   RR_Rotate_0,
                   outputs,
                   num_outputs);
}

void RealOutputConfiguratorDelegate::CreateFrameBuffer(
    int width,
    int height,
    const std::vector<OutputConfigurator::CrtcConfig>& configs) {
  CHECK(screen_) << "Server not grabbed";
  int current_width = DisplayWidth(display_, DefaultScreen(display_));
  int current_height = DisplayHeight(display_, DefaultScreen(display_));
  VLOG(1) << "CreateFrameBuffer " << width << " x " << height
          << " (current=" << current_width << " x " << current_height << ")";
  if (width ==  current_width && height == current_height)
    return;

  DestroyUnusedCrtcs(configs);
  int mm_width = width * kPixelsToMmScale;
  int mm_height = height * kPixelsToMmScale;
  XRRSetScreenSize(display_, window_, width, height, mm_width, mm_height);
}

void RealOutputConfiguratorDelegate::ConfigureCTM(
    int touch_device_id,
    const OutputConfigurator::CoordinateTransformation& ctm) {
  int ndevices;
  XIDeviceInfo* info = XIQueryDevice(display_, touch_device_id, &ndevices);
  Atom prop = XInternAtom(
      display_, "Coordinate Transformation Matrix", False);
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

void RealOutputConfiguratorDelegate::DestroyUnusedCrtcs(
    const std::vector<OutputConfigurator::CrtcConfig>& configs) {
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
    OutputConfigurator::CrtcConfig config(
        screen_->crtcs[i], 0, 0, None, None);
    for (std::vector<OutputConfigurator::CrtcConfig>::const_iterator it =
         configs.begin(); it != configs.end(); ++it) {
      if (config.crtc == it->crtc) {
        config.mode = it->mode;
        config.output = it->output;
        break;
      }
    }

    if (config.mode != None) {
      // In case our CRTC doesn't fit in our current framebuffer, disable it.
      // It'll get reenabled after we resize the framebuffer.
      int mode_width = 0, mode_height = 0;
      CHECK(GetModeDetails(config.mode, &mode_width, &mode_height, NULL));
      int current_width = DisplayWidth(display_, DefaultScreen(display_));
      int current_height = DisplayHeight(display_, DefaultScreen(display_));
      if (mode_width > current_width || mode_height > current_height) {
        config.mode = None;
        config.output = None;
      }
    }

    ConfigureCrtc(&config);
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

bool RealOutputConfiguratorDelegate::FindOrCreateMirrorMode(
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

  int internal_native_width = 0, internal_native_height = 0;
  int external_native_width = 0, external_native_height = 0;
  CHECK(GetModeDetails(internal_mode_id, &internal_native_width,
                       &internal_native_height, NULL));
  CHECK(GetModeDetails(external_mode_id, &external_native_width,
                       &external_native_height, NULL));

  // Check if some external output resolution can be mirrored on internal.
  // Prefer the modes in the order that X sorts them,
  // assuming this is the order in which they look better on the monitor.
  // If X's order is not satisfactory, we can either fix X's sorting,
  // or implement our sorting here.
  for (int i = 0; i < external_info->nmode; i++) {
    external_mode_id = external_info->modes[i];
    int external_width = 0, external_height = 0;
    bool is_external_interlaced = false;
    CHECK(GetModeDetails(external_mode_id, &external_width, &external_height,
                         &is_external_interlaced));
    bool is_native_aspect_ratio =
        external_native_width * external_height ==
        external_native_height * external_width;
    if (preserve_aspect && !is_native_aspect_ratio)
      continue;  // Allow only aspect ratio preserving modes for mirroring

    // Try finding exact match
    for (int j = 0; j < internal_info->nmode; j++) {
      internal_mode_id = internal_info->modes[j];
      int internal_width = 0, internal_height = 0;
      bool is_internal_interlaced = false;
      CHECK(GetModeDetails(internal_mode_id, &internal_width,
                           &internal_height, &is_internal_interlaced));
      if (internal_width == external_width &&
          internal_height == external_height &&
          is_internal_interlaced == is_external_interlaced) {
        *internal_mirror_mode = internal_mode_id;
        *external_mirror_mode = external_mode_id;
        return true;  // Mirror mode found
      }
    }

    // Try to create a matching internal output mode by panel fitting
    if (try_creating) {
      // We can downscale by 1.125, and upscale indefinitely
      // Downscaling looks ugly, so, can fit == can upscale
      // Also, internal panels don't support fitting interlaced modes
      bool can_fit =
          internal_native_width >= external_width &&
          internal_native_height >= external_height &&
          !is_external_interlaced;
      if (can_fit) {
        XRRAddOutputMode(display_, internal_output_id, external_mode_id);
        *internal_mirror_mode = *external_mirror_mode = external_mode_id;
        return true;  // Mirror mode created
      }
    }
  }

  return false;
}

void RealOutputConfiguratorDelegate::GetTouchscreens(
    std::vector<OutputConfigurator::OutputSnapshot>* outputs) {
  int ndevices = 0;
  Atom valuator_x = XInternAtom(display_, "Abs MT Position X", False);
  Atom valuator_y = XInternAtom(display_, "Abs MT Position Y", False);
  if (valuator_x == None || valuator_y == None)
    return;

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
        if ((*outputs)[k].native_mode == None ||
            (*outputs)[k].touch_device_id != None)
          continue;
        int native_mode_width = 0, native_mode_height = 0;
        if (!GetModeDetails((*outputs)[k].native_mode, &native_mode_width,
                            &native_mode_height, NULL))
          continue;

        // Allow 1 pixel difference between screen and touchscreen
        // resolutions.  Because in some cases for monitor resolution
        // 1024x768 touchscreen's resolution would be 1024x768, but for
        // some 1023x767.  It really depends on touchscreen's firmware
        // configuration.
        if (std::abs(native_mode_width - width) <= 1.0 &&
            std::abs(native_mode_height - height) <= 1.0) {
          (*outputs)[k].touch_device_id = info[i].deviceid;

          VLOG(1) << "Found touchscreen for output #" << k
                  << " id " << (*outputs)[k].touch_device_id
                  << " width " << width
                  << " height " << height;
          break;
        }
      }

      VLOG_IF(1, k == outputs->size())
          << "No matching output - ignoring touchscreen"
          << " id " << info[i].deviceid
          << " width " << width
          << " height " << height;
    }
  }

  XIFreeDeviceInfo(info);
}

}  // namespace chromeos
