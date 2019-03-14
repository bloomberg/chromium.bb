// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_session_encoding.h"

#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "content/public/common/content_switches.h"
#include "third_party/inspector_protocol/encoding/encoding.h"

using inspector_protocol_encoding::span;
using inspector_protocol_encoding::Status;
using inspector_protocol_encoding::StreamingParserHandler;
using inspector_protocol_encoding::cbor::NewCBOREncoder;
using inspector_protocol_encoding::cbor::ParseCBOR;
using inspector_protocol_encoding::json::NewJSONEncoder;
using inspector_protocol_encoding::json::ParseJSON;
using inspector_protocol_encoding::json::Platform;

namespace content {
namespace {
// ContentShellPlatform allows us to inject the string<->double conversion
// routines from base:: into the inspector_protocol JSON parser / serializer.
class ContentShellPlatform : public Platform {
 public:
  bool StrToD(const char* str, double* result) const override {
    return base::StringToDouble(str, result);
  }

  // Prints |value| in a format suitable for JSON.
  std::unique_ptr<char[]> DToStr(double value) const override {
    std::string str = base::NumberToString(value);
    std::unique_ptr<char[]> result(new char[str.size() + 1]);
    memcpy(result.get(), str.c_str(), str.size() + 1);
    return result;
  }
};
}  // namespace
bool EnableInternalDevToolsBinaryProtocol() {
  static bool enabled = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableInternalDevToolsBinaryProtocol);
  return enabled;
}

// TODO(johannes): Move this into the cbor library. Don't want to
// do this just yet to first gain more experience about the most
// appropriate API, including how to propagate errors.
std::string ConvertCBORToJSON(const std::string& cbor) {
  ContentShellPlatform platform;
  std::string json_message;
  Status status;
  std::unique_ptr<StreamingParserHandler> json_writer =
      NewJSONEncoder(&platform, &json_message, &status);
  ParseCBOR(
      span<uint8_t>(reinterpret_cast<const uint8_t*>(cbor.data()), cbor.size()),
      json_writer.get());
  if (!status.ok()) {
    LOG(ERROR) << "ConvertCBORToJSON error "
               << static_cast<uint32_t>(status.error) << " position "
               << static_cast<uint32_t>(status.pos);
    return "";
  }
  return json_message;
}

std::string ConvertJSONToCBOR(const std::string& json) {
  ContentShellPlatform platform;
  std::vector<uint8_t> cbor;
  Status status;
  std::unique_ptr<StreamingParserHandler> encoder =
      NewCBOREncoder(&cbor, &status);
  ParseJSON(
      &platform,
      span<uint8_t>(reinterpret_cast<const uint8_t*>(json.data()), json.size()),
      encoder.get());
  if (!status.ok()) {
    LOG(ERROR) << "ConvertJSONToCBOR error "
               << static_cast<uint32_t>(status.error) << " position "
               << static_cast<uint32_t>(status.pos);
    return "";
  }
  return std::string(cbor.begin(), cbor.end());
}

bool IsCBOR(const std::string& serialized) {
  return serialized.size() >= 6 &&
         reinterpret_cast<const uint8_t&>(serialized[0]) == 0xd8 &&
         reinterpret_cast<const uint8_t&>(serialized[1]) == 0x5a;
}
}  // namespace content
