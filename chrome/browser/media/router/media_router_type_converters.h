// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_TYPE_CONVERTERS_H_
#define CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_TYPE_CONVERTERS_H_

#include <string>
#include <vector>

#include "chrome/browser/media/router/issue.h"
#include "chrome/browser/media/router/media_router.mojom.h"
#include "chrome/browser/media/router/media_sink.h"
#include "chrome/browser/media/router/media_source.h"
#include "mojo/common/common_type_converters.h"

namespace mojo {

// Type/enum conversions from Media Router data types to Mojo data types
// and vice versa.

// MediaSink conversion.
template <>
struct TypeConverter<media_router::MediaSink,
                     media_router::interfaces::MediaSinkPtr> {
  static media_router::MediaSink Convert(
      const media_router::interfaces::MediaSinkPtr& input) {
    return media_router::MediaSink(input->sink_id, input->name);
  }
};
template <>
struct TypeConverter<media_router::interfaces::MediaSinkPtr,
                     media_router::MediaSink> {
  static media_router::interfaces::MediaSinkPtr Convert(
      const media_router::MediaSink& input) {
    media_router::interfaces::MediaSinkPtr output(
        media_router::interfaces::MediaSink::New());
    output->sink_id = input.id();
    output->name = input.name();
    return output.Pass();
  }
};

// MediaRoute conversion.
template <>
struct TypeConverter<media_router::MediaRoute,
                     media_router::interfaces::MediaRoutePtr> {
  static media_router::MediaRoute Convert(
      const media_router::interfaces::MediaRoutePtr& input) {
    return media_router::MediaRoute(
        input->media_route_id, media_router::MediaSource(input->media_source),
        input->media_sink.To<media_router::MediaSink>(), input->description,
        input->is_local);
  }
};
template <>
struct TypeConverter<media_router::interfaces::MediaRoutePtr,
                     media_router::MediaRoute> {
  static media_router::interfaces::MediaRoutePtr Convert(
      const media_router::MediaRoute& input) {
    media_router::interfaces::MediaRoutePtr output(
        media_router::interfaces::MediaRoute::New());
    if (!input.media_source().Empty())
      output->media_source = input.media_source().id();
    output->media_route_id = input.media_route_id();
    output->media_sink =
        media_router::interfaces::MediaSink::From<media_router::MediaSink>(
            input.media_sink());
    output->description = input.description();
    output->is_local = input.is_local();
    return output.Pass();
  }
};

// Issue conversion.
media_router::Issue::Severity IssueSeverityFromMojo(
    media_router::interfaces::Issue::Severity severity);

media_router::IssueAction::Type IssueActionTypeFromMojo(
    media_router::interfaces::Issue::ActionType action_type);

template <>
struct TypeConverter<media_router::Issue, media_router::interfaces::IssuePtr> {
  static media_router::Issue Convert(
      const media_router::interfaces::IssuePtr& input) {
    std::vector<media_router::IssueAction> actions;
    if (input->secondary_actions) {
      actions.reserve(input->secondary_actions.size());
      for (size_t i = 0; i < input->secondary_actions.size(); ++i) {
        actions.push_back(media_router::IssueAction(
            IssueActionTypeFromMojo(input->secondary_actions[i])));
      }
    }
    return media_router::Issue(
        input->title, input->message,
        media_router::IssueAction(
            IssueActionTypeFromMojo(input->default_action)),
        actions, input->route_id, IssueSeverityFromMojo(input->severity),
        input->is_blocking, input->help_url);
  }
};

}  // namespace mojo

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_TYPE_CONVERTERS_H_
