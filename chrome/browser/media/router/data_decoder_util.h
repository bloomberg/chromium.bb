// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DATA_DECODER_UTIL_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DATA_DECODER_UTIL_H_

#include <string>

#include "base/token.h"
#include "services/data_decoder/public/cpp/safe_json_parser.h"
#include "services/data_decoder/public/cpp/safe_xml_parser.h"

namespace service_manager {
class Connector;
}

namespace media_router {

// The batch ID used by data_decoder_util functions.
static constexpr base::Token kDataDecoderServiceBatchId{0xabf3003d50bb0170ull,
                                                        0x0c659c570136566eull};

// A wrapper over their data_decoder functions for parsing XML/JSON that batches
// all calls with a shared batch ID.
// Thread safety: A newly constructed DataDecoder is not bound to any thread. On
// first use, it becomes bound to the calling thread.
class DataDecoder {
 public:
  // |connector|: Connector object to be cloned in the constructor.
  explicit DataDecoder(service_manager::Connector* connector);
  ~DataDecoder();

  void ParseXml(const std::string& unsafe_xml,
                data_decoder::XmlParserCallback callback);

  void ParseJson(const std::string& unsafe_json,
                 data_decoder::SafeJsonParser::SuccessCallback success_callback,
                 data_decoder::SafeJsonParser::ErrorCallback error_callback);

 private:
  std::unique_ptr<service_manager::Connector> connector_;
  DISALLOW_COPY_AND_ASSIGN(DataDecoder);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DATA_DECODER_UTIL_H_
