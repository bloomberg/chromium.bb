// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTER_STRUCT_TRAITS_H_
#define CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTER_STRUCT_TRAITS_H_

#include <string>

#include "chrome/browser/media/router/mojo/media_router.mojom.h"
#include "mojo/common/common_custom_types_struct_traits.h"

namespace mojo {

template <>
struct StructTraits<media_router::mojom::RouteMessageDataView,
                    media_router::RouteMessage> {
  static media_router::mojom::RouteMessage::Type type(
      const media_router::RouteMessage& msg) {
    switch (msg.type) {
      case media_router::RouteMessage::TEXT:
        return media_router::mojom::RouteMessage::Type::TEXT;
      case media_router::RouteMessage::BINARY:
        return media_router::mojom::RouteMessage::Type::BINARY;
    }
    NOTREACHED();
    return media_router::mojom::RouteMessage::Type::TEXT;
  }

  static const base::Optional<std::string>& message(
      const media_router::RouteMessage& msg) {
    return msg.text;
  }

  static const base::Optional<std::vector<uint8_t>>& data(
      const media_router::RouteMessage& msg) {
    return msg.binary;
  }

  static bool Read(media_router::mojom::RouteMessageDataView data,
                   media_router::RouteMessage* out) {
    media_router::mojom::RouteMessage::Type type;
    if (!data.ReadType(&type))
      return false;
    switch (type) {
      case media_router::mojom::RouteMessage::Type::TEXT: {
        out->type = media_router::RouteMessage::TEXT;
        base::Optional<std::string> text;
        if (!data.ReadMessage(&text) || !text)
          return false;
        out->text = std::move(text);
        break;
      }
      case media_router::mojom::RouteMessage::Type::BINARY: {
        out->type = media_router::RouteMessage::BINARY;
        base::Optional<std::vector<uint8_t>> binary;
        if (!data.ReadData(&binary) || !binary)
          return false;
        out->binary = std::move(binary);
        break;
      }
      default:
        return false;
    }
    return true;
  }
};

}  // namespace mojo

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTER_STRUCT_TRAITS_H_
