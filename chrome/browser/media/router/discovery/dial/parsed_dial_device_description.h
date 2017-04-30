// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_PARSED_DIAL_DEVICE_DESCRIPTION_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_PARSED_DIAL_DEVICE_DESCRIPTION_H_

#include <string>
#include "url/gurl.h"

namespace media_router {

struct ParsedDialDeviceDescription {
  ParsedDialDeviceDescription();
  ParsedDialDeviceDescription(const std::string& unique_id,
                              const std::string& friendly_name,
                              const GURL& app_url);
  ParsedDialDeviceDescription(const ParsedDialDeviceDescription& other);
  ~ParsedDialDeviceDescription();

  ParsedDialDeviceDescription& operator=(
      const ParsedDialDeviceDescription& other);

  bool operator==(const ParsedDialDeviceDescription& other) const;

  // UUID (UDN).
  std::string unique_id;

  // Short user-friendly device name.
  std::string friendly_name;

  // Device model name.
  std::string model_name;

  // The DIAL application URL (used to launch DIAL applications).
  GURL app_url;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_PARSED_DIAL_DEVICE_DESCRIPTION_H_
