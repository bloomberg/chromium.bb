// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTER_TYPE_CONVERTERS_H_
#define CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTER_TYPE_CONVERTERS_H_

#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/media/router/media_router.h"
#include "chrome/browser/media/router/media_sink.h"
#include "chrome/browser/media/router/media_source.h"
#include "chrome/browser/media/router/mojo/media_router.mojom.h"
#include "chrome/browser/media/router/route_request_result.h"
#include "content/public/common/presentation_session.h"

namespace mojo {

// Type/enum conversions from Media Router data types to Mojo data types
// and vice versa.

// MediaSink conversion.
media_router::MediaSink::IconType SinkIconTypeFromMojo(
    media_router::mojom::MediaSink::IconType type);

media_router::mojom::MediaSink::IconType SinkIconTypeToMojo(
    media_router::MediaSink::IconType type);

template <>
struct TypeConverter<media_router::MediaSink,
                     media_router::mojom::MediaSinkPtr> {
  static media_router::MediaSink Convert(
      const media_router::mojom::MediaSinkPtr& input);
};

// MediaRoute conversion.
template <>
struct TypeConverter<media_router::MediaRoute,
                     media_router::mojom::MediaRoutePtr> {
  static media_router::MediaRoute Convert(
      const media_router::mojom::MediaRoutePtr& input);
};

template <>
struct TypeConverter<std::unique_ptr<media_router::MediaRoute>,
                     media_router::mojom::MediaRoutePtr> {
  static std::unique_ptr<media_router::MediaRoute> Convert(
      const media_router::mojom::MediaRoutePtr& input);
};

// PresentationConnectionState conversion.
content::PresentationConnectionState PresentationConnectionStateFromMojo(
    media_router::mojom::MediaRouter::PresentationConnectionState state);

// PresentationConnectionCloseReason conversion.
content::PresentationConnectionCloseReason
PresentationConnectionCloseReasonFromMojo(
    media_router::mojom::MediaRouter::PresentationConnectionCloseReason
        reason);

// RouteRequestResult conversion.
media_router::RouteRequestResult::ResultCode RouteRequestResultCodeFromMojo(
    media_router::mojom::RouteRequestResultCode result_code);

}  // namespace mojo

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTER_TYPE_CONVERTERS_H_
