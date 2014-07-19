// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/device_uma.h"

#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/x/device_data_manager_x11.h"

// Enum type for CrOS gesture lib UMA
enum UMACrosGestureMetricsType{
  // To give an estimated number of all interested events.
  UMA_CROS_GESTURE_METRICS_ALL_EVENTS,
  UMA_CROS_GESTURE_METRICS_NOISY_GROUND,
  // NOTE: Add states only immediately above this line. Make sure to
  // update the enum list in tools/metrics/histograms/histograms.xml
  // accordingly.
  UMA_CROS_GESTURE_METRICS_COUNT
};

namespace chromeos {

DeviceUMA* DeviceUMA::GetInstance() {
  return Singleton<DeviceUMA>::get();
}

DeviceUMA::DeviceUMA()
    : stopped_(false) {
  ui::PlatformEventSource::GetInstance()->AddPlatformEventObserver(this);
}

DeviceUMA::~DeviceUMA() {
  Stop();
}

void DeviceUMA::Stop() {
  if (stopped_)
    return;
  ui::PlatformEventSource::GetInstance()->RemovePlatformEventObserver(this);
  stopped_ = true;
}

void DeviceUMA::WillProcessEvent(const ui::PlatformEvent& event) {
  CheckIncomingEvent(event);
}

void DeviceUMA::DidProcessEvent(const ui::PlatformEvent& event) {
}

void DeviceUMA::CheckTouchpadEvent(XEvent* native_event) {
  XIDeviceEvent* xiev =
      static_cast<XIDeviceEvent*>(native_event->xcookie.data);
  // We take only the slave event since there is no need to count twice.
  if (xiev->deviceid != xiev->sourceid)
    return;
  UMA_HISTOGRAM_ENUMERATION("Touchpad.Metrics",
                            UMA_CROS_GESTURE_METRICS_ALL_EVENTS,
                            UMA_CROS_GESTURE_METRICS_COUNT);

  // Check for the CrOS touchpad metrics gesture
  ui::DeviceDataManagerX11 *manager = ui::DeviceDataManagerX11::GetInstance();
  if (manager->IsCMTMetricsEvent(native_event)) {
    ui::GestureMetricsType type;
    float data1, data2;
    manager->GetMetricsData(native_event, &type, &data1, &data2);

    // We currently track only the noisy ground issue. Please see
    // http://crbug.com/237683.
    if (type == ui::kGestureMetricsTypeNoisyGround) {
      UMA_HISTOGRAM_ENUMERATION("Touchpad.Metrics",
                                UMA_CROS_GESTURE_METRICS_NOISY_GROUND,
                                UMA_CROS_GESTURE_METRICS_COUNT);
    }
  }
}

void DeviceUMA::CheckIncomingEvent(XEvent* event) {
  switch (event->type) {
    case GenericEvent: {
      ui::DeviceDataManagerX11* devices =
          ui::DeviceDataManagerX11::GetInstance();
      if (devices->IsXIDeviceEvent(event) &&
          devices->IsTouchpadXInputEvent(event)) {
        CheckTouchpadEvent(event);
      }
      break;
    }
    default:
      break;
  }
  return;
}

}  // namespace chromeos
