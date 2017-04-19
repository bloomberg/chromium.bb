// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_SAFE_DIAL_DEVICE_DESCRIPTION_PARSER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_SAFE_DIAL_DEVICE_DESCRIPTION_PARSER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/threading/thread_checker.h"
#include "chrome/common/media_router/mojo/dial_device_description_parser.mojom.h"
#include "content/public/browser/utility_process_mojo_client.h"

namespace media_router {

// SafeDialDeviceDescriptionParser parses the given device description XML file
// safely via a utility process. This class runs on IO thread.
class SafeDialDeviceDescriptionParser {
 public:
  // Callback function to be invoked when utility process finishes parsing
  // device description XML.
  // |success|: returns false if parsing fails.
  // |device_description|: device description object. Empty if parsing fails.
  using DeviceDescriptionCallback = chrome::mojom::DialDeviceDescriptionParser::
      ParseDialDeviceDescriptionCallback;

  SafeDialDeviceDescriptionParser();

  // Start parsing device description XML file in utility process.
  void Start(const std::string& xml_text,
             const DeviceDescriptionCallback& callback);

 private:
  ~SafeDialDeviceDescriptionParser();

  // See SafeDialDeviceDescriptionParser::DeviceDescriptionCallback
  void OnParseDeviceDescriptionComplete(
      chrome::mojom::DialDeviceDescriptionPtr device_description);

  // TODO(crbug.com/702766): Add an enum type describing why utility process
  // fails to parse device description xml.
  void OnParseDeviceDescriptionFailed();

  // Utility client used to send device description parsing task to the utility
  // process.
  std::unique_ptr<content::UtilityProcessMojoClient<
      chrome::mojom::DialDeviceDescriptionParser>>
      utility_process_mojo_client_;

  // Only accessed on the IO thread.
  DeviceDescriptionCallback device_description_callback_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(SafeDialDeviceDescriptionParser);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_SAFE_DIAL_DEVICE_DESCRIPTION_PARSER_H_
