// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DIAL_DIAL_DEVICE_DATA_H_
#define CHROME_BROWSER_EXTENSIONS_API_DIAL_DIAL_DEVICE_DATA_H_

#include <string>
#include <vector>

#include "base/time.h"
#include "base/values.h"

namespace extensions {

namespace api {
namespace dial {
struct DialDevice;
}  // namespace api
}  // namespace dial

// Dial device information that is used within the DialService and Registry on
// the IO thread. It is updated as new information arrives and a list of
// DialDeviceData is copied and sent to event listeners on the UI thread.
class DialDeviceData {
 public:
  DialDeviceData();
  DialDeviceData(const DialDeviceData& other_data);
  ~DialDeviceData();

  DialDeviceData& operator=(const DialDeviceData& other_data);

  const std::string& device_id() const;
  void set_device_id(const std::string& id);

  const std::string& label() const;
  void set_label(const std::string& label);

  const std::string& device_description_url() const;
  void set_device_description_url(const std::string& url);

  const base::Time& response_time() const;
  void set_response_time(const base::Time& response_time);

  int max_age() const { return max_age_; }
  void set_max_age(int max_age) { max_age_ = max_age; }
  bool has_max_age() const { return max_age_ >= 0; }

  int config_id() const { return config_id_; }
  void set_config_id(int config_id) { config_id_ = config_id; }
  bool has_config_id() const { return config_id_ >= 0; }

  // Fills the |device| API struct from this instance.
  void FillDialDevice(api::dial::DialDevice* device) const;

 private:
  // Hardware identifier from the DIAL response.  Not exposed to API clients.
  std::string device_id_;

  // Identifies this device to clients of the API.
  std::string label_;

  // The device description URL.
  std::string device_description_url_;

  // The time that the most recent response was received.
  base::Time response_time_;

  // Optional (-1 means unset).
  int max_age_;

  // Optional (-1 means unset).
  int config_id_;

  // Implementation of copy ctor and assignment operator.
  void CopyFrom(const DialDeviceData& other_data);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DIAL_DIAL_DEVICE_DATA_H_
