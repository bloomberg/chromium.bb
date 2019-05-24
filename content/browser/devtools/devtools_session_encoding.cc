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
using inspector_protocol_encoding::json::ConvertCBORToJSON;
using inspector_protocol_encoding::json::ConvertJSONToCBOR;
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
  return false;
}

// TODO(johannes): Push error handling to client code after devtools_session.cc
// is simplified.
std::string ConvertCBORToJSON(inspector_protocol_encoding::span<uint8_t> cbor) {
  ContentShellPlatform platform;
  std::string json_message;
  inspector_protocol_encoding::Status status =
      ConvertCBORToJSON(platform, cbor, &json_message);
  if (!status.ok()) {
    LOG(ERROR) << "ConvertCBORToJSON error "
               << static_cast<uint32_t>(status.error) << " position "
               << static_cast<uint32_t>(status.pos);
    return "";
  }
  return json_message;
}

std::string ConvertJSONToCBOR(inspector_protocol_encoding::span<uint8_t> json) {
  ContentShellPlatform platform;
  std::string cbor;
  inspector_protocol_encoding::Status status =
      ConvertJSONToCBOR(platform, json, &cbor);
  if (!status.ok()) {
    LOG(ERROR) << "ConvertJSONToCBOR error "
               << static_cast<uint32_t>(status.error) << " position "
               << static_cast<uint32_t>(status.pos);
    return "";
  }
  return cbor;
}
}  // namespace content
