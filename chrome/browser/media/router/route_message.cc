// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/route_message.h"

#include "base/base64.h"
#include "base/json/string_escape.h"

namespace media_router {

RouteMessage::RouteMessage() = default;
RouteMessage::RouteMessage(const RouteMessage& other) = default;
RouteMessage::~RouteMessage() = default;

std::string RouteMessage::ToHumanReadableString() const {
  if (!text && !binary)
    return "null";
  DCHECK((type == TEXT && text) || (type == BINARY && binary));
  std::string result;
  if (text) {
    result = "text=";
    base::EscapeJSONString(*text, true, &result);
  } else {
    const base::StringPiece src(reinterpret_cast<const char*>(binary->data()),
                                binary->size());
    base::Base64Encode(src, &result);
    result = "binary=" + result;
  }
  return result;
}

}  // namespace media_router
