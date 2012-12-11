// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/dial/dial_device_data.h"

#include "chrome/common/extensions/api/dial.h"

namespace extensions {

DialDeviceData::DialDeviceData() : max_age_(-1), config_id_(-1) { }

DialDeviceData::DialDeviceData(const DialDeviceData& other_data) {
  CopyFrom(other_data);
}

DialDeviceData& DialDeviceData::operator=(const DialDeviceData& other_data) {
  CopyFrom(other_data);
  return *this;
}

DialDeviceData::~DialDeviceData() { }

const std::string& DialDeviceData::device_id() const {
  return device_id_;
}

void DialDeviceData::set_device_id(const std::string& id) {
  device_id_ = id;
}

const std::string& DialDeviceData::label() const {
  return label_;
}

void DialDeviceData::set_label(const std::string& label) {
  label_ = label;
}

const std::string& DialDeviceData::device_description_url() const {
  return device_description_url_;
}

void DialDeviceData::set_device_description_url(const std::string& url) {
  device_description_url_ = url;
}

const base::Time& DialDeviceData::response_time() const {
  return response_time_;
}

void DialDeviceData::set_response_time(const base::Time& response_time) {
  response_time_ = response_time;
}

void DialDeviceData::FillDialDevice(api::dial::DialDevice* device) const {
  DCHECK(!device_id_.empty());
  device->device_label = label();
  device->device_description_url = device_description_url();
  if (has_config_id()) {
    device->config_id.reset(new int(config_id()));
  }
}

void DialDeviceData::CopyFrom(const DialDeviceData& other_data) {
  device_id_ = other_data.device_id_;
  label_ = other_data.label_;
  device_description_url_ = other_data.device_description_url_;
  response_time_ = other_data.response_time_;
  max_age_ = other_data.max_age_;
  config_id_ = other_data.config_id_;
}

}  // namespace extensions
