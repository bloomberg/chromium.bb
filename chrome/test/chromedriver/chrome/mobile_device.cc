// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/mobile_device.h"
#include "chrome/test/chromedriver/chrome/mobile_device_list.h"
#include "chrome/test/chromedriver/chrome/status.h"

MobileDevice::MobileDevice() {}
MobileDevice::~MobileDevice() {}

Status FindMobileDevice(std::string device_name,
                        scoped_ptr<MobileDevice>* mobile_device) {
  base::JSONReader json_reader(base::JSON_ALLOW_TRAILING_COMMAS);
  scoped_ptr<base::Value> devices_value;
  devices_value.reset(json_reader.ReadToValue(kMobileDevices));
  if (!devices_value.get())
    return Status(kUnknownError,
                  "could not parse mobile device list because " +
                  json_reader.GetErrorMessage());

  base::ListValue* mobile_devices;
  if (!devices_value->GetAsList(&mobile_devices))
    return Status(kUnknownError, "malformed device metrics list");

  for (base::ListValue::iterator it = mobile_devices->begin();
       it != mobile_devices->end();
       ++it) {
    base::ListValue* device = NULL;
    if (!(*it)->GetAsList(&device)) {
      return Status(kUnknownError,
                    "malformed device in list: should be an array");
    }

    if (device != NULL) {
      std::string name;
      if (!device->GetString(0, &name)) {
        return Status(kUnknownError,
                      "malformed device name: should be a string");
      }
      if (name != device_name)
        continue;

      scoped_ptr<MobileDevice> tmp_mobile_device(new MobileDevice());
      std::string device_metrics_string;
      if (!device->GetString(1, &tmp_mobile_device->user_agent)) {
        return Status(kUnknownError,
                      "malformed device user agent: should be a string");
      }
      if (!device->GetString(2, &device_metrics_string)) {
        return Status(kUnknownError,
                      "malformed device metrics: should be a string");
      }
      std::vector<std::string> metrics_vector;
      base::SplitString(device_metrics_string, 'x', &metrics_vector);
      if (metrics_vector.size() < 3)
        return Status(kUnknownError, "malformed device metrics string");

      int width = 0;
      int height = 0;
      double device_scale_factor = 0.0;
      if (!base::StringToInt(metrics_vector[0], &width)) {
        return Status(kUnknownError,
                      "malformed device width: should be an integer");
      }
      if (!base::StringToInt(metrics_vector[1], &height)) {
        return Status(kUnknownError,
                      "malformed device height: should be an integer");
      }
      if (!base::StringToDouble(metrics_vector[2], &device_scale_factor)) {
        return Status(kUnknownError,
                      "malformed device scale factor: should be a double");
      }
      tmp_mobile_device->device_metrics.reset(
          new DeviceMetrics(width, height, device_scale_factor));

      *mobile_device = tmp_mobile_device.Pass();
      return Status(kOk);
    }
  }

  return Status(kUnknownError, "must be a valid device");
}
