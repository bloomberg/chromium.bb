// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/display/native_display_delegate_x11.h"

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/dpms.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/XInput2.h>

#include <utility>

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_pump_x11.h"
#include "base/x11/edid_parser_x11.h"
#include "base/x11/x11_error_tracker.h"
#include "chromeos/display/native_display_event_dispatcher_x11.h"
#include "chromeos/display/native_display_observer.h"
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

////////////////////////////////////////////////////////////////////////////////
// NativeDisplayDelegateX11::HelperDelegateX11

class NativeDisplayDelegateX11::HelperDelegateX11
    : public NativeDisplayDelegateX11::HelperDelegate {
 public:
  HelperDelegateX11(NativeDisplayDelegateX11* delegate)
      : delegate_(delegate) {}
  virtual ~HelperDelegateX11() {}

  // NativeDisplayDelegateX11::HelperDelegate overrides:
  virtual void UpdateXRandRConfiguration(
      const base::NativeEvent& event) OVERRIDE {
    XRRUpdateConfiguration(event);
  }
  virtual const std::vector<OutputConfigurator::OutputSnapshot>&
      GetCachedOutputs() const OVERRIDE {
    return delegate_->cached_outputs_;
  }
  virtual void NotifyDisplayObservers() OVERRIDE {
    FOR_EACH_OBSERVER(NativeDisplayObserver,
                      delegate_->observers_,
                      OnConfigurationChanged());
  }

 private:
  NativeDisplayDelegateX11* delegate_;

  DISALLOW_COPY_AND_ASSIGN(HelperDelegateX11);
};

////////////////////////////////////////////////////////////////////////////////
// NativeDisplayDelegateX11::MessagePumpObserverX11

class NativeDisplayDelegateX11::MessagePumpObserverX11
    : public base::MessagePumpObserver {
 public:
  MessagePumpObserverX11(HelperDelegate* delegate);
  virtual ~MessagePumpObserverX11();

  // base::MessagePumpObserver overrides:
  virtual base::EventStatus WillProcessEvent(
      const base::NativeEvent& event) OVERRIDE;
  virtual void DidProcessEvent(const base::NativeEvent& event) OVERRIDE;

 private:
  HelperDelegate* delegate_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(MessagePumpObserverX11);
};

NativeDisplayDelegateX11::MessagePumpObserverX11::MessagePumpObserverX11(
    HelperDelegate* delegate) : delegate_(delegate) {}

NativeDisplayDelegateX11::MessagePumpObserverX11::~MessagePumpObserverX11() {}

base::EventStatus
NativeDisplayDelegateX11::MessagePumpObserverX11::WillProcessEvent(
    const base::NativeEvent& event) {
  // XI_HierarchyChanged events are special. There is no window associated with
  // these events. So process them directly from here.
  if (event->type == GenericEvent &&
      event->xgeneric.evtype == XI_HierarchyChanged) {
    VLOG(1) << "Received XI_HierarchyChanged event";
    // Defer configuring outputs to not stall event processing.
    // This also takes care of same event being received twice.
    delegate_->NotifyDisplayObservers();
  }

  return base::EVENT_CONTINUE;
}

void NativeDisplayDelegateX11::MessagePumpObserverX11::DidProcessEvent(
    const base::NativeEvent& event) {
}

////////////////////////////////////////////////////////////////////////////////
// NativeDisplayDelegateX11 implementation:

NativeDisplayDelegateX11::NativeDisplayDelegateX11()
    : display_(base::MessagePumpX11::GetDefaultXDisplay()),
      window_(DefaultRootWindow(display_)),
      screen_(NULL) {}

NativeDisplayDelegateX11::~NativeDisplayDelegateX11() {
  base::MessagePumpX11::Current()->RemoveDispatcherForRootWindow(
      message_pump_dispatcher_.get());
  base::MessagePumpX11::Current()->RemoveObserver(message_pump_observer_.get());
}

void NativeDisplayDelegateX11::Initialize() {
  int error_base_ignored = 0;
  int xrandr_event_base = 0;
  XRRQueryExtension(display_, &xrandr_event_base, &error_base_ignored);

  helper_delegate_.reset(new HelperDelegateX11(this));
  message_pump_dispatcher_.reset(new NativeDisplayEventDispatcherX11(
      helper_delegate_.get(), xrandr_event_base));
  message_pump_observer_.reset(new MessagePumpObserverX11(
      helper_delegate_.get()));

  base::MessagePumpX11::Current()->AddDispatcherForRootWindow(
      message_pump_dispatcher_.get());
  // We can't do this with a root window listener because XI_HierarchyChanged
  // messages don't have a target window.
  base::MessagePumpX11::Current()->AddObserver(message_pump_observer_.get());
}

void NativeDisplayDelegateX11::GrabServer() {
  CHECK(!screen_) << "Server already grabbed";
  XGrabServer(display_);
  screen_ = XRRGetScreenResources(display_, window_);
  CHECK(screen_);
}

void NativeDisplayDelegateX11::UngrabServer() {
  CHECK(screen_) << "Server not grabbed";
  XRRFreeScreenResources(screen_);
  screen_ = NULL;
  XUngrabServer(display_);
}

void NativeDisplayDelegateX11::SyncWithServer() { XSync(display_, 0); }

void NativeDisplayDelegateX11::SetBackgroundColor(uint32 color_argb) {
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

void NativeDisplayDelegateX11::ForceDPMSOn() {
  CHECK(DPMSEnable(display_));
  CHECK(DPMSForceLevel(display_, DPMSModeOn));
}

std::vector<OutputConfigurator::OutputSnapshot>
NativeDisplayDelegateX11::GetOutputs() {
  CHECK(screen_) << "Server not grabbed";

  cached_outputs_.clear();
  RRCrtc last_used_crtc = None;

  for (int i = 0; i < screen_->noutput && cached_outputs_.size() < 2; ++i) {
    RROutput output_id = screen_->outputs[i];
    XRROutputInfo* output_info = XRRGetOutputInfo(display_, screen_, output_id);
    if (output_info->connection == RR_Connected) {
      OutputConfigurator::OutputSnapshot output =
          InitOutputSnapshot(output_id, output_info, &last_used_crtc, i);
      VLOG(2) << "Found display " << cached_outputs_.size() << ":"
              << " output=" << output.output << " crtc=" << output.crtc
              << " current_mode=" << output.current_mode;
      cached_outputs_.push_back(output);
    }
    XRRFreeOutputInfo(output_info);
  }

  return cached_outputs_;
}

void NativeDisplayDelegateX11::AddMode(
    const OutputConfigurator::OutputSnapshot& output, RRMode mode) {
  CHECK(screen_) << "Server not grabbed";
  VLOG(1) << "AddOutputMode: output=" << output.output << " mode=" << mode;
  XRRAddOutputMode(display_, output.output, mode);
}

bool NativeDisplayDelegateX11::Configure(
    const OutputConfigurator::OutputSnapshot& output,
    RRMode mode,
    int x,
    int y) {
  return ConfigureCrtc(output.crtc, mode, output.output, x, y);
}

bool NativeDisplayDelegateX11::ConfigureCrtc(
    RRCrtc crtc,
    RRMode mode,
    RROutput output,
    int x,
    int y) {
  CHECK(screen_) << "Server not grabbed";
  VLOG(1) << "ConfigureCrtc: crtc=" << crtc << " mode=" << mode
          << " output=" << output << " x=" << x << " y=" << y;
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

void NativeDisplayDelegateX11::CreateFrameBuffer(
    int width,
    int height,
    const std::vector<OutputConfigurator::OutputSnapshot>& outputs) {
  CHECK(screen_) << "Server not grabbed";
  int current_width = DisplayWidth(display_, DefaultScreen(display_));
  int current_height = DisplayHeight(display_, DefaultScreen(display_));
  VLOG(1) << "CreateFrameBuffer: new=" << width << "x" << height
          << " current=" << current_width << "x" << current_height;
  if (width == current_width && height == current_height)
    return;

  DestroyUnusedCrtcs(outputs);
  int mm_width = width * kPixelsToMmScale;
  int mm_height = height * kPixelsToMmScale;
  XRRSetScreenSize(display_, window_, width, height, mm_width, mm_height);
}

bool NativeDisplayDelegateX11::InitModeInfo(
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
        mode_info->refresh_rate =
            static_cast<float>(info.dotClock) /
            (static_cast<float>(info.hTotal) * static_cast<float>(info.vTotal));
      } else {
        mode_info->refresh_rate = 0.0f;
      }
      return true;
    }
  }
  return false;
}

OutputConfigurator::OutputSnapshot NativeDisplayDelegateX11::InitOutputSnapshot(
    RROutput id,
    XRROutputInfo* info,
    RRCrtc* last_used_crtc,
    int index) {
  OutputConfigurator::OutputSnapshot output;
  output.output = id;
  output.width_mm = info->mm_width;
  output.height_mm = info->mm_height;
  output.has_display_id = base::GetDisplayId(id, index, &output.display_id);
  output.index = index;
  bool is_internal = IsInternalOutput(info);

  // Use the index as a valid display ID even if the internal
  // display doesn't have valid EDID because the index
  // will never change.
  if (!output.has_display_id && is_internal)
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
  if (is_internal) {
    output.type = ui::OUTPUT_TYPE_INTERNAL;
  } else if (name.find(kOutputName_VGA) == 0) {
    output.type = ui::OUTPUT_TYPE_VGA;
  } else if (name.find(kOutputName_HDMI) == 0) {
    output.type = ui::OUTPUT_TYPE_HDMI;
  } else if (name.find(kOutputName_DVI) == 0) {
    output.type = ui::OUTPUT_TYPE_DVI;
  } else if (name.find(kOutputName_DisplayPort) == 0) {
    output.type = ui::OUTPUT_TYPE_DISPLAYPORT;
  } else {
    LOG(ERROR) << "Unknown link type: " << name;
    output.type = ui::OUTPUT_TYPE_UNKNOWN;
  }

  return output;
}

bool NativeDisplayDelegateX11::GetHDCPState(
    const OutputConfigurator::OutputSnapshot& output, ui::HDCPState* state) {
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
  success = XRRGetOutputProperty(display_,
                                 output.output,
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
  if (actual_type == None) {
    LOG(ERROR) << "Property '" << kContentProtectionAtomName
               << "' does not exist";
    ok = false;
  } else if (success == Success && actual_type == XA_ATOM &&
             actual_format == 32 && nitems == 1) {
    Atom value = reinterpret_cast<Atom*>(values)[0];
    if (value == XInternAtom(display_, kProtectionUndesiredAtomName, False)) {
      *state = ui::HDCP_STATE_UNDESIRED;
    } else if (value ==
               XInternAtom(display_, kProtectionDesiredAtomName, False)) {
      *state = ui::HDCP_STATE_DESIRED;
    } else if (value ==
               XInternAtom(display_, kProtectionEnabledAtomName, False)) {
      *state = ui::HDCP_STATE_ENABLED;
    } else {
      LOG(ERROR) << "Unknown " << kContentProtectionAtomName
                 << " value: " << value;
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

bool NativeDisplayDelegateX11::SetHDCPState(
    const OutputConfigurator::OutputSnapshot& output, ui::HDCPState state) {
  Atom name = XInternAtom(display_, kContentProtectionAtomName, False);
  Atom value = None;
  switch (state) {
    case ui::HDCP_STATE_UNDESIRED:
      value = XInternAtom(display_, kProtectionUndesiredAtomName, False);
      break;
    case ui::HDCP_STATE_DESIRED:
      value = XInternAtom(display_, kProtectionDesiredAtomName, False);
      break;
    default:
      NOTREACHED() << "Invalid HDCP state: " << state;
      return false;
  }
  base::X11ErrorTracker err_tracker;
  unsigned char* data = reinterpret_cast<unsigned char*>(&value);
  XRRChangeOutputProperty(
      display_, output.output, name, XA_ATOM, 32, PropModeReplace, data, 1);
  if (err_tracker.FoundNewError()) {
    LOG(ERROR) << "XRRChangeOutputProperty failed";
    return false;
  } else {
    return true;
  }
}

void NativeDisplayDelegateX11::DestroyUnusedCrtcs(
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
             outputs.begin();
         it != outputs.end();
         ++it) {
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

bool NativeDisplayDelegateX11::IsOutputAspectPreservingScaling(RROutput id) {
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

      success = XRRGetOutputProperty(display_,
                                     id,
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
      if (success == Success && actual_type == XA_ATOM && actual_format == 32 &&
          nitems == 1) {
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

void NativeDisplayDelegateX11::AddObserver(NativeDisplayObserver* observer) {
  observers_.AddObserver(observer);
}

void NativeDisplayDelegateX11::RemoveObserver(NativeDisplayObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace chromeos
