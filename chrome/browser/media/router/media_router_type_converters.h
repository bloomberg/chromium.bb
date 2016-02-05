// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_TYPE_CONVERTERS_H_
#define CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_TYPE_CONVERTERS_H_

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/media/router/issue.h"
#include "chrome/browser/media/router/media_router.h"
#include "chrome/browser/media/router/media_router.mojom.h"
#include "chrome/browser/media/router/media_sink.h"
#include "chrome/browser/media/router/media_source.h"
#include "chrome/browser/media/router/route_request_result.h"
#include "content/public/browser/presentation_session.h"
#include "mojo/common/common_type_converters.h"

namespace mojo {

// Type/enum conversions from Media Router data types to Mojo data types
// and vice versa.

// MediaSink conversion.
media_router::MediaSink::IconType SinkIconTypeFromMojo(
    media_router::interfaces::MediaSink::IconType type);

media_router::interfaces::MediaSink::IconType SinkIconTypeToMojo(
    media_router::MediaSink::IconType type);

template <>
struct TypeConverter<media_router::MediaSink,
                     media_router::interfaces::MediaSinkPtr> {
  static media_router::MediaSink Convert(
      const media_router::interfaces::MediaSinkPtr& input);
};

// MediaRoute conversion.
template <>
struct TypeConverter<media_router::MediaRoute,
                     media_router::interfaces::MediaRoutePtr> {
  static media_router::MediaRoute Convert(
      const media_router::interfaces::MediaRoutePtr& input);
};

template <>
struct TypeConverter<scoped_ptr<media_router::MediaRoute>,
                     media_router::interfaces::MediaRoutePtr> {
  static scoped_ptr<media_router::MediaRoute> Convert(
      const media_router::interfaces::MediaRoutePtr& input);
};

// Issue conversion.
media_router::Issue::Severity IssueSeverityFromMojo(
    media_router::interfaces::Issue::Severity severity);

media_router::IssueAction::Type IssueActionTypeFromMojo(
    media_router::interfaces::Issue::ActionType action_type);

template <>
struct TypeConverter<media_router::Issue, media_router::interfaces::IssuePtr> {
  static media_router::Issue Convert(
      const media_router::interfaces::IssuePtr& input);
};

// PresentationConnectionState conversion.
content::PresentationConnectionState PresentationConnectionStateFromMojo(
    media_router::interfaces::MediaRouter::PresentationConnectionState state);

// RouteRequestResult conversion.
media_router::RouteRequestResult::ResultCode RouteRequestResultCodeFromMojo(
    media_router::interfaces::RouteRequestResultCode result_code);

}  // namespace mojo

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_TYPE_CONVERTERS_H_
