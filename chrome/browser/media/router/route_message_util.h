// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_ROUTE_MESSAGE_UTIL_H_
#define CHROME_BROWSER_MEDIA_ROUTER_ROUTE_MESSAGE_UTIL_H_

#include <cstdint>
#include <string>
#include <vector>

#include "chrome/common/media_router/mojo/media_router.mojom.h"
#include "content/public/common/presentation_connection_message.h"

namespace media_router {
namespace message_util {

media_router::mojom::RouteMessagePtr RouteMessageFromString(
    std::string message);

media_router::mojom::RouteMessagePtr RouteMessageFromData(
    std::vector<uint8_t> data);

content::PresentationConnectionMessage PresentationConnectionFromRouteMessage(
    media_router::mojom::RouteMessagePtr route_message);

}  // namespace message_util
}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_ROUTE_MESSAGE_UTIL_H_
