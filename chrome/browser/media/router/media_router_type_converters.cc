// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "chrome/browser/media/router/media_router_type_converters.h"

using media_router::interfaces::IssuePtr;
using media_router::interfaces::MediaRoutePtr;
using media_router::interfaces::MediaSinkPtr;

using PresentationConnectionState =
    media_router::interfaces::MediaRouter::PresentationConnectionState;
using RouteRequestResultCode = media_router::interfaces::RouteRequestResultCode;

namespace mojo {

media_router::MediaSink::IconType SinkIconTypeFromMojo(
    media_router::interfaces::MediaSink::IconType type) {
  switch (type) {
    case media_router::interfaces::MediaSink::IconType::CAST:
      return media_router::MediaSink::CAST;
    case media_router::interfaces::MediaSink::IconType::CAST_AUDIO:
      return media_router::MediaSink::CAST_AUDIO;
    case media_router::interfaces::MediaSink::IconType::CAST_AUDIO_GROUP:
      return media_router::MediaSink::CAST_AUDIO_GROUP;
    case media_router::interfaces::MediaSink::IconType::HANGOUT:
      return media_router::MediaSink::HANGOUT;
    case media_router::interfaces::MediaSink::IconType::GENERIC:
      return media_router::MediaSink::GENERIC;
    default:
      NOTREACHED() << "Unknown sink icon type " << type;
      return media_router::MediaSink::GENERIC;
  }
}

media_router::interfaces::MediaSink::IconType SinkIconTypeToMojo(
    media_router::MediaSink::IconType type) {
  switch (type) {
    case media_router::MediaSink::CAST:
      return media_router::interfaces::MediaSink::IconType::CAST;
    case media_router::MediaSink::CAST_AUDIO:
      return media_router::interfaces::MediaSink::IconType::CAST_AUDIO;
    case media_router::MediaSink::CAST_AUDIO_GROUP:
      return media_router::interfaces::MediaSink::IconType::CAST_AUDIO_GROUP;
    case media_router::MediaSink::HANGOUT:
      return media_router::interfaces::MediaSink::IconType::HANGOUT;
    case media_router::MediaSink::GENERIC:
      return media_router::interfaces::MediaSink::IconType::GENERIC;
    default:
      NOTREACHED() << "Unknown sink icon type " << type;
      return media_router::interfaces::MediaSink::IconType::GENERIC;
  }
}

// static
media_router::MediaSink
TypeConverter<media_router::MediaSink, MediaSinkPtr>::Convert(
    const MediaSinkPtr& input) {
  media_router::MediaSink sink(input->sink_id, input->name,
                               SinkIconTypeFromMojo(input->icon_type));
  if (!input->description.get().empty())
    sink.set_description(input->description);
  if (!input->domain.get().empty())
    sink.set_domain(input->domain);

  return sink;
}

// static
media_router::MediaRoute
TypeConverter<media_router::MediaRoute, MediaRoutePtr>::Convert(
    const MediaRoutePtr& input) {
  return media_router::MediaRoute(
      input->media_route_id, media_router::MediaSource(input->media_source),
      input->media_sink_id, input->description, input->is_local,
      input->custom_controller_path, input->for_display);
}

// static
scoped_ptr<media_router::MediaRoute>
TypeConverter<scoped_ptr<media_router::MediaRoute>, MediaRoutePtr>::Convert(
    const MediaRoutePtr& input) {
  return make_scoped_ptr(new media_router::MediaRoute(
      input->media_route_id, media_router::MediaSource(input->media_source),
      input->media_sink_id, input->description, input->is_local,
      input->custom_controller_path, input->for_display));
}

media_router::Issue::Severity IssueSeverityFromMojo(
    media_router::interfaces::Issue::Severity severity) {
  switch (severity) {
    case media_router::interfaces::Issue::Severity::FATAL:
      return media_router::Issue::FATAL;
    case media_router::interfaces::Issue::Severity::WARNING:
      return media_router::Issue::WARNING;
    case media_router::interfaces::Issue::Severity::NOTIFICATION:
      return media_router::Issue::NOTIFICATION;
    default:
      NOTREACHED() << "Unknown issue severity " << severity;
      return media_router::Issue::WARNING;
  }
}

media_router::IssueAction::Type IssueActionTypeFromMojo(
    media_router::interfaces::Issue::ActionType action_type) {
  switch (action_type) {
    case media_router::interfaces::Issue::ActionType::DISMISS:
      return media_router::IssueAction::TYPE_DISMISS;
    case media_router::interfaces::Issue::ActionType::LEARN_MORE:
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

content::PresentationConnectionState PresentationConnectionStateFromMojo(
    PresentationConnectionState state) {
  switch (state) {
    case PresentationConnectionState::CONNECTING:
      return content::PRESENTATION_CONNECTION_STATE_CONNECTING;
    case PresentationConnectionState::CONNECTED:
      return content::PRESENTATION_CONNECTION_STATE_CONNECTED;
    case PresentationConnectionState::CLOSED:
      return content::PRESENTATION_CONNECTION_STATE_CLOSED;
    case PresentationConnectionState::TERMINATED:
      return content::PRESENTATION_CONNECTION_STATE_TERMINATED;
    default:
      NOTREACHED() << "Unknown PresentationConnectionState " << state;
      return content::PRESENTATION_CONNECTION_STATE_TERMINATED;
  }
}

media_router::RouteRequestResult::ResultCode RouteRequestResultCodeFromMojo(
    RouteRequestResultCode result_code) {
  switch (result_code) {
    case RouteRequestResultCode::UNKNOWN_ERROR:
      return media_router::RouteRequestResult::UNKNOWN_ERROR;
    case RouteRequestResultCode::OK:
      return media_router::RouteRequestResult::OK;
    case RouteRequestResultCode::TIMED_OUT:
      return media_router::RouteRequestResult::TIMED_OUT;
    default:
      NOTREACHED() << "Unknown RouteRequestResultCode " << result_code;
      return media_router::RouteRequestResult::UNKNOWN_ERROR;
  }
}

}  // namespace mojo
