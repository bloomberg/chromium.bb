// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_router_type_converters.h"

using media_router::interfaces::IssuePtr;
using media_router::interfaces::MediaRoutePtr;
using media_router::interfaces::MediaSinkPtr;

using PresentationConnectionState =
    media_router::interfaces::MediaRouter::PresentationConnectionState;

namespace mojo {

media_router::MediaSink::IconType SinkIconTypeFromMojo(
    media_router::interfaces::MediaSink::IconType type) {
  switch (type) {
    case media_router::interfaces::MediaSink::IconType::ICON_TYPE_CAST:
      return media_router::MediaSink::CAST;
    case media_router::interfaces::MediaSink::IconType::ICON_TYPE_CAST_AUDIO:
      return media_router::MediaSink::CAST_AUDIO;
    case media_router::interfaces::MediaSink::
        IconType::ICON_TYPE_CAST_AUDIO_GROUP:
      return media_router::MediaSink::CAST_AUDIO_GROUP;
    case media_router::interfaces::MediaSink::IconType::ICON_TYPE_HANGOUT:
      return media_router::MediaSink::HANGOUT;
    case media_router::interfaces::MediaSink::IconType::ICON_TYPE_GENERIC:
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
      return media_router::interfaces::MediaSink::IconType::ICON_TYPE_CAST;
    case media_router::MediaSink::CAST_AUDIO:
      return
          media_router::interfaces::MediaSink::IconType::ICON_TYPE_CAST_AUDIO;
    case media_router::MediaSink::CAST_AUDIO_GROUP:
      return
          media_router::interfaces::MediaSink::
              IconType::ICON_TYPE_CAST_AUDIO_GROUP;
    case media_router::MediaSink::HANGOUT:
      return media_router::interfaces::MediaSink::IconType::ICON_TYPE_HANGOUT;
    case media_router::MediaSink::GENERIC:
      return media_router::interfaces::MediaSink::IconType::ICON_TYPE_GENERIC;
    default:
      NOTREACHED() << "Unknown sink icon type " << type;
      return media_router::interfaces::MediaSink::ICON_TYPE_GENERIC;
  }
}

// static
media_router::MediaSink
TypeConverter<media_router::MediaSink, MediaSinkPtr>::Convert(
    const MediaSinkPtr& input) {
  return media_router::MediaSink(input->sink_id, input->name,
                                 SinkIconTypeFromMojo(input->icon_type));
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

content::PresentationConnectionState PresentationConnectionStateFromMojo(
    PresentationConnectionState state) {
  switch (state) {
    case PresentationConnectionState::PRESENTATION_CONNECTION_STATE_CONNECTED:
      return content::PRESENTATION_CONNECTION_STATE_CONNECTED;
    case PresentationConnectionState::PRESENTATION_CONNECTION_STATE_CLOSED:
      return content::PRESENTATION_CONNECTION_STATE_CLOSED;
    case PresentationConnectionState::PRESENTATION_CONNECTION_STATE_TERMINATED:
      return content::PRESENTATION_CONNECTION_STATE_TERMINATED;
    default:
      NOTREACHED() << "Unknown PresentationConnectionState " << state;
      return content::PRESENTATION_CONNECTION_STATE_TERMINATED;
  }
}

}  // namespace mojo
