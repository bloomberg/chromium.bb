// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_MEDIA_ROUTER_ROUTE_MESSAGE_H_
#define CHROME_COMMON_MEDIA_ROUTER_ROUTE_MESSAGE_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/optional.h"

namespace media_router {

// TODO(mfoltz): Replace with nearly-identical
// content::PresentationConnectionMessage.
struct RouteMessage {
  enum Type { TEXT, BINARY } type = TEXT;
  // Used when the |type| is TEXT.
  base::Optional<std::string> text;
  // Used when the |type| is BINARY.
  base::Optional<std::vector<uint8_t>> binary;

  RouteMessage();
  RouteMessage(const RouteMessage& other);
  ~RouteMessage();

  std::string ToHumanReadableString() const;
};

}  // namespace media_router

#endif  // CHROME_COMMON_MEDIA_ROUTER_ROUTE_MESSAGE_H_
