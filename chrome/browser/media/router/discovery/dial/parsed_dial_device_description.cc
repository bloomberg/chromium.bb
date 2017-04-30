// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/dial/parsed_dial_device_description.h"

namespace media_router {

ParsedDialDeviceDescription::ParsedDialDeviceDescription() = default;

ParsedDialDeviceDescription::ParsedDialDeviceDescription(
    const std::string& unique_id,
    const std::string& friendly_name,
    const GURL& app_url)
    : unique_id(unique_id), friendly_name(friendly_name), app_url(app_url) {}

ParsedDialDeviceDescription::ParsedDialDeviceDescription(
    const ParsedDialDeviceDescription& other) = default;
ParsedDialDeviceDescription::~ParsedDialDeviceDescription() = default;

ParsedDialDeviceDescription& ParsedDialDeviceDescription::operator=(
    const ParsedDialDeviceDescription& other) {
  if (this == &other)
    return *this;

  this->unique_id = other.unique_id;
  this->friendly_name = other.friendly_name;
  this->model_name = other.model_name;
  this->app_url = other.app_url;

  return *this;
}

bool ParsedDialDeviceDescription::operator==(
    const ParsedDialDeviceDescription& other) const {
  return unique_id == other.unique_id && friendly_name == other.friendly_name &&
         model_name == other.model_name && app_url == other.app_url;
}

}  // namespace media_router
