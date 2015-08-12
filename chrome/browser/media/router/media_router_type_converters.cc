// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_router_type_converters.h"

using media_router::interfaces::IssuePtr;
using media_router::interfaces::MediaSinkPtr;
using media_router::interfaces::MediaRoutePtr;

namespace mojo {

// static
media_router::MediaSink
TypeConverter<media_router::MediaSink, MediaSinkPtr>::Convert(
    const MediaSinkPtr& input) {
  return media_router::MediaSink(input->sink_id, input->name,
                                 input->is_launching);
}

// static
MediaSinkPtr TypeConverter<MediaSinkPtr, media_router::MediaSink>::Convert(
    const media_router::MediaSink& input) {
  MediaSinkPtr output(media_router::interfaces::MediaSink::New());
  output->sink_id = input.id();
  output->name = input.name();
  output->is_launching = input.is_launching();
  return output.Pass();
}

// static
media_router::MediaRoute
TypeConverter<media_router::MediaRoute, MediaRoutePtr>::Convert(
    const MediaRoutePtr& input) {
  return media_router::MediaRoute(
      input->media_route_id, media_router::MediaSource(input->media_source),
      input->media_sink.To<media_router::MediaSink>(), input->description,
      input->is_local, input->custom_controller_path);
}

// static
scoped_ptr<media_router::MediaRoute>
TypeConverter<scoped_ptr<media_router::MediaRoute>, MediaRoutePtr>::Convert(
    const MediaRoutePtr& input) {
  return make_scoped_ptr(new media_router::MediaRoute(
      input->media_route_id, media_router::MediaSource(input->media_source),
      input->media_sink.To<media_router::MediaSink>(), input->description,
      input->is_local, input->custom_controller_path));
}

// static
MediaRoutePtr TypeConverter<MediaRoutePtr, media_router::MediaRoute>::Convert(
    const media_router::MediaRoute& input) {
  MediaRoutePtr output(media_router::interfaces::MediaRoute::New());
  if (!input.media_source().Empty())
    output->media_source = input.media_source().id();
  output->media_route_id = input.media_route_id();
  output->media_sink =
      media_router::interfaces::MediaSink::From<media_router::MediaSink>(
          input.media_sink());
  output->description = input.description();
  output->is_local = input.is_local();
  output->custom_controller_path = input.custom_controller_path();
  return output.Pass();
}

media_router::Issue::Severity IssueSeverityFromMojo(
    media_router::interfaces::Issue::Severity severity) {
  switch (severity) {
    case media_router::interfaces::Issue::Severity::SEVERITY_FATAL:
      return media_router::Issue::FATAL;
    case media_router::interfaces::Issue::Severity::SEVERITY_WARNING:
      return media_router::Issue::WARNING;
    case media_router::interfaces::Issue::Severity::SEVERITY_NOTIFICATION:
      return media_router::Issue::NOTIFICATION;
    default:
      NOTREACHED() << "Unknown issue severity " << severity;
      return media_router::Issue::WARNING;
  }
}

media_router::IssueAction::Type IssueActionTypeFromMojo(
    media_router::interfaces::Issue::ActionType action_type) {
  switch (action_type) {
    case media_router::interfaces::Issue::ActionType::ACTION_TYPE_OK:
      return media_router::IssueAction::TYPE_OK;
    case media_router::interfaces::Issue::ActionType::ACTION_TYPE_CANCEL:
      return media_router::IssueAction::TYPE_CANCEL;
    case media_router::interfaces::Issue::ActionType::ACTION_TYPE_DISMISS:
      return media_router::IssueAction::TYPE_DISMISS;
    case media_router::interfaces::Issue::ActionType::ACTION_TYPE_LEARN_MORE:
      return media_router::IssueAction::TYPE_LEARN_MORE;
    default:
      NOTREACHED() << "Unknown issue action type " << action_type;
      return media_router::IssueAction::TYPE_DISMISS;
  }
}

// static
media_router::Issue TypeConverter<media_router::Issue, IssuePtr>::Convert(
    const IssuePtr& input) {
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
      media_router::IssueAction(IssueActionTypeFromMojo(input->default_action)),
      actions, input->route_id, IssueSeverityFromMojo(input->severity),
      input->is_blocking, input->help_url);
}

}  // namespace mojo
