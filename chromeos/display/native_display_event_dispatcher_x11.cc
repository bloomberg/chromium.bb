// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/display/native_display_event_dispatcher_x11.h"

#include <X11/extensions/Xrandr.h>

namespace chromeos {

NativeDisplayEventDispatcherX11::NativeDisplayEventDispatcherX11(
    NativeDisplayDelegateX11::HelperDelegate* delegate, int xrandr_event_base)
    : delegate_(delegate),
      xrandr_event_base_(xrandr_event_base) {}

NativeDisplayEventDispatcherX11::~NativeDisplayEventDispatcherX11() {}

uint32_t NativeDisplayEventDispatcherX11::Dispatch(
    const base::NativeEvent& event) {
  if (event->type - xrandr_event_base_ == RRScreenChangeNotify) {
    VLOG(1) << "Received RRScreenChangeNotify event";
    delegate_->UpdateXRandRConfiguration(event);
    return POST_DISPATCH_PERFORM_DEFAULT;
  }

  // Bail out early for everything except RRNotify_OutputChange events
  // about an output getting connected or disconnected.
  if (event->type - xrandr_event_base_ != RRNotify)
    return POST_DISPATCH_PERFORM_DEFAULT;
  const XRRNotifyEvent* notify_event = reinterpret_cast<XRRNotifyEvent*>(event);
  if (notify_event->subtype != RRNotify_OutputChange)
    return POST_DISPATCH_PERFORM_DEFAULT;
  const XRROutputChangeNotifyEvent* output_change_event =
      reinterpret_cast<XRROutputChangeNotifyEvent*>(event);
  const int action = output_change_event->connection;
  if (action != RR_Connected && action != RR_Disconnected)
    return POST_DISPATCH_PERFORM_DEFAULT;

  const bool connected = (action == RR_Connected);
  VLOG(1) << "Received RRNotify_OutputChange event:"
          << " output=" << output_change_event->output
          << " crtc=" << output_change_event->crtc
          << " mode=" << output_change_event->mode
          << " action=" << (connected ? "connected" : "disconnected");

  bool found_changed_output = false;
  const std::vector<OutputConfigurator::OutputSnapshot>& cached_outputs =
      delegate_->GetCachedOutputs();
  for (std::vector<OutputConfigurator::OutputSnapshot>::const_iterator
          it = cached_outputs.begin();
       it != cached_outputs.end(); ++it) {
    if (it->output == output_change_event->output) {
      if (connected && it->crtc == output_change_event->crtc &&
          it->current_mode == output_change_event->mode) {
        VLOG(1) << "Ignoring event describing already-cached state";
        return POST_DISPATCH_PERFORM_DEFAULT;
      }
      found_changed_output = true;
      break;
    }
  }

  if (!connected && !found_changed_output) {
    VLOG(1) << "Ignoring event describing already-disconnected output";
    return POST_DISPATCH_PERFORM_DEFAULT;
  }

  delegate_->NotifyDisplayObservers();

  return POST_DISPATCH_PERFORM_DEFAULT;
}

}  // namespace chromeos
