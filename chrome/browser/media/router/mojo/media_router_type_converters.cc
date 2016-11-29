// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "chrome/browser/media/router/mojo/media_router_type_converters.h"

using media_router::mojom::IssuePtr;
using media_router::mojom::MediaRoutePtr;
using media_router::mojom::MediaSinkPtr;

using PresentationConnectionState =
    media_router::mojom::MediaRouter::PresentationConnectionState;
using PresentationConnectionCloseReason =
    media_router::mojom::MediaRouter::PresentationConnectionCloseReason;
using RouteRequestResultCode = media_router::mojom::RouteRequestResultCode;

namespace mojo {

media_router::MediaSink::IconType SinkIconTypeFromMojo(
    media_router::mojom::MediaSink::IconType type) {
  switch (type) {
    case media_router::mojom::MediaSink::IconType::CAST:
      return media_router::MediaSink::CAST;
    case media_router::mojom::MediaSink::IconType::CAST_AUDIO:
      return media_router::MediaSink::CAST_AUDIO;
    case media_router::mojom::MediaSink::IconType::CAST_AUDIO_GROUP:
      return media_router::MediaSink::CAST_AUDIO_GROUP;
    case media_router::mojom::MediaSink::IconType::HANGOUT:
      return media_router::MediaSink::HANGOUT;
    case media_router::mojom::MediaSink::IconType::GENERIC:
      return media_router::MediaSink::GENERIC;
    default:
      NOTREACHED() << "Unknown sink icon type " << type;
      return media_router::MediaSink::GENERIC;
  }
}

media_router::mojom::MediaSink::IconType SinkIconTypeToMojo(
    media_router::MediaSink::IconType type) {
  switch (type) {
    case media_router::MediaSink::CAST:
      return media_router::mojom::MediaSink::IconType::CAST;
    case media_router::MediaSink::CAST_AUDIO:
      return media_router::mojom::MediaSink::IconType::CAST_AUDIO;
    case media_router::MediaSink::CAST_AUDIO_GROUP:
      return media_router::mojom::MediaSink::IconType::CAST_AUDIO_GROUP;
    case media_router::MediaSink::HANGOUT:
      return media_router::mojom::MediaSink::IconType::HANGOUT;
    case media_router::MediaSink::GENERIC:
      return media_router::mojom::MediaSink::IconType::GENERIC;
    default:
      NOTREACHED() << "Unknown sink icon type " << type;
      return media_router::mojom::MediaSink::IconType::GENERIC;
  }
}

// static
media_router::MediaSink
TypeConverter<media_router::MediaSink, MediaSinkPtr>::Convert(
    const MediaSinkPtr& input) {
  media_router::MediaSink sink(input->sink_id, input->name,
                               SinkIconTypeFromMojo(input->icon_type));
  if (input->description && !input->description->empty())
    sink.set_description(*input->description);
  if (input->domain && !input->domain->empty())
    sink.set_domain(*input->domain);

  return sink;
}

// static
media_router::MediaRoute
TypeConverter<media_router::MediaRoute, MediaRoutePtr>::Convert(
    const MediaRoutePtr& input) {
  media_router::MediaRoute media_route(
      input->media_route_id,
      media_router::MediaSource(input->media_source.value_or(std::string())),
      input->media_sink_id, input->description, input->is_local,
      input->custom_controller_path.value_or(std::string()),
      input->for_display);
  media_route.set_incognito(input->is_incognito);
  media_route.set_offscreen_presentation(input->is_offscreen_presentation);
  return media_route;
}

// static
std::unique_ptr<media_router::MediaRoute>
TypeConverter<std::unique_ptr<media_router::MediaRoute>,
              MediaRoutePtr>::Convert(const MediaRoutePtr& input) {
  std::unique_ptr<media_router::MediaRoute> media_route(
      new media_router::MediaRoute(
          input->media_route_id,
          media_router::MediaSource(
              input->media_source.value_or(std::string())),
          input->media_sink_id, input->description, input->is_local,
          input->custom_controller_path.value_or(std::string()),
          input->for_display));
  media_route->set_incognito(input->is_incognito);
  media_route->set_offscreen_presentation(input->is_offscreen_presentation);
  return media_route;
}

media_router::Issue::Severity IssueSeverityFromMojo(
    media_router::mojom::Issue::Severity severity) {
  switch (severity) {
    case media_router::mojom::Issue::Severity::FATAL:
      return media_router::Issue::FATAL;
    case media_router::mojom::Issue::Severity::WARNING:
      return media_router::Issue::WARNING;
    case media_router::mojom::Issue::Severity::NOTIFICATION:
      return media_router::Issue::NOTIFICATION;
    default:
      NOTREACHED() << "Unknown issue severity " << severity;
      return media_router::Issue::WARNING;
  }
}

media_router::IssueAction::Type IssueActionTypeFromMojo(
    media_router::mojom::Issue::ActionType action_type) {
  switch (action_type) {
    case media_router::mojom::Issue::ActionType::DISMISS:
      return media_router::IssueAction::TYPE_DISMISS;
    case media_router::mojom::Issue::ActionType::LEARN_MORE:
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
    actions.reserve(input->secondary_actions->size());
    for (auto a : *input->secondary_actions)
      actions.push_back(media_router::IssueAction(IssueActionTypeFromMojo(a)));
  }
  return media_router::Issue(
      input->title, input->message.value_or(std::string()),
      media_router::IssueAction(IssueActionTypeFromMojo(input->default_action)),
      actions, input->route_id.value_or(std::string()),
      IssueSeverityFromMojo(input->severity),
      input->is_blocking, input->help_page_id);
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

content::PresentationConnectionCloseReason
PresentationConnectionCloseReasonFromMojo(
    PresentationConnectionCloseReason reason) {
  switch (reason) {
    case PresentationConnectionCloseReason::CONNECTION_ERROR:
      return content::PRESENTATION_CONNECTION_CLOSE_REASON_CONNECTION_ERROR;
    case PresentationConnectionCloseReason::CLOSED:
      return content::PRESENTATION_CONNECTION_CLOSE_REASON_CLOSED;
    case PresentationConnectionCloseReason::WENT_AWAY:
      return content::PRESENTATION_CONNECTION_CLOSE_REASON_WENT_AWAY;
    default:
      NOTREACHED() << "Unknown PresentationConnectionCloseReason " << reason;
      return content::PRESENTATION_CONNECTION_CLOSE_REASON_CONNECTION_ERROR;
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
    case RouteRequestResultCode::ROUTE_NOT_FOUND:
      return media_router::RouteRequestResult::ROUTE_NOT_FOUND;
    case RouteRequestResultCode::SINK_NOT_FOUND:
      return media_router::RouteRequestResult::SINK_NOT_FOUND;
    case RouteRequestResultCode::INVALID_ORIGIN:
      return media_router::RouteRequestResult::INVALID_ORIGIN;
    case RouteRequestResultCode::INCOGNITO_MISMATCH:
      return media_router::RouteRequestResult::INCOGNITO_MISMATCH;
    case RouteRequestResultCode::NO_SUPPORTED_PROVIDER:
      return media_router::RouteRequestResult::NO_SUPPORTED_PROVIDER;
    case RouteRequestResultCode::CANCELLED:
      return media_router::RouteRequestResult::CANCELLED;
    default:
      NOTREACHED() << "Unknown RouteRequestResultCode " << result_code;
      return media_router::RouteRequestResult::UNKNOWN_ERROR;
  }
}

}  // namespace mojo
