// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_MEDIA_ROUTER_DIAL_DEVICE_DESCRIPTION_PARSER_IMPL_H_
#define CHROME_UTILITY_MEDIA_ROUTER_DIAL_DEVICE_DESCRIPTION_PARSER_IMPL_H_

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "chrome/common/media_router/mojo/dial_device_description_parser.mojom.h"
#include "services/service_manager/public/cpp/bind_source_info.h"

namespace media_router {

// Implementation of Mojo DialDeviceDescriptionParser. It accepts device
// description parsing request from browser process, and handles it in utility
// process. Class must be created and destroyed on utility thread.
class DialDeviceDescriptionParserImpl
    : public chrome::mojom::DialDeviceDescriptionParser {
 public:
  DialDeviceDescriptionParserImpl();
  ~DialDeviceDescriptionParserImpl() override;

  static void Create(const service_manager::BindSourceInfo& source_info,
                     chrome::mojom::DialDeviceDescriptionParserRequest request);

 private:
  friend class DialDeviceDescriptionParserImplTest;

  // extensions::mojom::DialDeviceDescriptionParser:
  void ParseDialDeviceDescription(
      const std::string& device_description_xml_data,
      ParseDialDeviceDescriptionCallback callback) override;

  // Processes the result of getting device description. Returns valid
  // DialDeviceDescriptionPtr if processing succeeds; otherwise returns nullptr.
  // |xml|: The device description xml text.
  chrome::mojom::DialDeviceDescriptionPtr Parse(const std::string& xml);

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(DialDeviceDescriptionParserImpl);
};

}  // namespace media_router

#endif  // CHROME_UTILITY_MEDIA_ROUTER_DIAL_DEVICE_DESCRIPTION_PARSER_IMPL_H_
