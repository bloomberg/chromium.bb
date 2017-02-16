// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTER_STRUCT_TRAITS_H_
#define CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTER_STRUCT_TRAITS_H_

#include <string>

#include "chrome/browser/media/router/issue.h"
#include "chrome/browser/media/router/media_sink.h"
#include "chrome/browser/media/router/mojo/media_router.mojom.h"
#include "chrome/browser/media/router/route_request_result.h"
#include "mojo/common/common_custom_types_struct_traits.h"

namespace mojo {

// Issue

template <>
struct EnumTraits<media_router::mojom::Issue::ActionType,
                  media_router::IssueInfo::Action> {
  static media_router::mojom::Issue::ActionType ToMojom(
      media_router::IssueInfo::Action action) {
    switch (action) {
      case media_router::IssueInfo::Action::DISMISS:
        return media_router::mojom::Issue::ActionType::DISMISS;
      case media_router::IssueInfo::Action::LEARN_MORE:
        return media_router::mojom::Issue::ActionType::LEARN_MORE;
    }
    NOTREACHED() << "Unknown issue action type " << static_cast<int>(action);
    return media_router::mojom::Issue::ActionType::DISMISS;
  }

  static bool FromMojom(media_router::mojom::Issue::ActionType input,
                        media_router::IssueInfo::Action* output) {
    switch (input) {
      case media_router::mojom::Issue::ActionType::DISMISS:
        *output = media_router::IssueInfo::Action::DISMISS;
        return true;
      case media_router::mojom::Issue::ActionType::LEARN_MORE:
        *output = media_router::IssueInfo::Action::LEARN_MORE;
        return true;
    }
    return false;
  }
};

template <>
struct EnumTraits<media_router::mojom::Issue::Severity,
                  media_router::IssueInfo::Severity> {
  static media_router::mojom::Issue::Severity ToMojom(
      media_router::IssueInfo::Severity severity) {
    switch (severity) {
      case media_router::IssueInfo::Severity::FATAL:
        return media_router::mojom::Issue::Severity::FATAL;
      case media_router::IssueInfo::Severity::WARNING:
        return media_router::mojom::Issue::Severity::WARNING;
      case media_router::IssueInfo::Severity::NOTIFICATION:
        return media_router::mojom::Issue::Severity::NOTIFICATION;
    }
    NOTREACHED() << "Unknown issue severity " << static_cast<int>(severity);
    return media_router::mojom::Issue::Severity::WARNING;
  }

  static bool FromMojom(media_router::mojom::Issue::Severity input,
                        media_router::IssueInfo::Severity* output) {
    switch (input) {
      case media_router::mojom::Issue::Severity::FATAL:
        *output = media_router::IssueInfo::Severity::FATAL;
        return true;
      case media_router::mojom::Issue::Severity::WARNING:
        *output = media_router::IssueInfo::Severity::WARNING;
        return true;
      case media_router::mojom::Issue::Severity::NOTIFICATION:
        *output = media_router::IssueInfo::Severity::NOTIFICATION;
        return true;
    }
    return false;
  }
};

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

template <>
struct StructTraits<media_router::mojom::IssueDataView,
                    media_router::IssueInfo> {
  static bool Read(media_router::mojom::IssueDataView data,
                   media_router::IssueInfo* out);

  static base::Optional<std::string> route_id(
      const media_router::IssueInfo& issue) {
    return issue.route_id.empty() ? base::Optional<std::string>()
                                  : base::make_optional(issue.route_id);
  }

  static media_router::IssueInfo::Severity severity(
      const media_router::IssueInfo& issue) {
    return issue.severity;
  }

  static bool is_blocking(const media_router::IssueInfo& issue) {
    return issue.is_blocking;
  }

  static std::string title(const media_router::IssueInfo& issue) {
    return issue.title;
  }

  static base::Optional<std::string> message(
      const media_router::IssueInfo& issue) {
    return issue.message.empty() ? base::Optional<std::string>()
                                 : base::make_optional(issue.message);
  }

  static media_router::IssueInfo::Action default_action(
      const media_router::IssueInfo& issue) {
    return issue.default_action;
  }

  static base::Optional<std::vector<media_router::IssueInfo::Action>>
  secondary_actions(const media_router::IssueInfo& issue) {
    return issue.secondary_actions;
  }

  static int32_t help_page_id(const media_router::IssueInfo& issue) {
    return issue.help_page_id;
  }
};

// MediaSink

template <>
struct EnumTraits<media_router::mojom::MediaSink::IconType,
                  media_router::MediaSink::IconType> {
  static media_router::mojom::MediaSink::IconType ToMojom(
      media_router::MediaSink::IconType icon_type) {
    switch (icon_type) {
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
    }
    NOTREACHED() << "Unknown sink icon type " << static_cast<int>(icon_type);
    return media_router::mojom::MediaSink::IconType::GENERIC;
  }

  static bool FromMojom(media_router::mojom::MediaSink::IconType input,
                        media_router::MediaSink::IconType* output) {
    switch (input) {
      case media_router::mojom::MediaSink::IconType::CAST:
        *output = media_router::MediaSink::CAST;
        return true;
      case media_router::mojom::MediaSink::IconType::CAST_AUDIO:
        *output = media_router::MediaSink::CAST_AUDIO;
        return true;
      case media_router::mojom::MediaSink::IconType::CAST_AUDIO_GROUP:
        *output = media_router::MediaSink::CAST_AUDIO_GROUP;
        return true;
      case media_router::mojom::MediaSink::IconType::HANGOUT:
        *output = media_router::MediaSink::HANGOUT;
        return true;
      case media_router::mojom::MediaSink::IconType::GENERIC:
        *output = media_router::MediaSink::GENERIC;
        return true;
    }
    return false;
  }
};

template <>
struct StructTraits<media_router::mojom::MediaSinkDataView,
                    media_router::MediaSink> {
  static bool Read(media_router::mojom::MediaSinkDataView data,
                   media_router::MediaSink* out);

  static std::string sink_id(const media_router::MediaSink& sink) {
    return sink.id();
  }

  static std::string name(const media_router::MediaSink& sink) {
    return sink.name();
  }

  static base::Optional<std::string> description(
      const media_router::MediaSink& sink) {
    return sink.description();
  }

  static base::Optional<std::string> domain(
      const media_router::MediaSink& sink) {
    return sink.domain();
  }

  static media_router::MediaSink::IconType icon_type(
      const media_router::MediaSink& sink) {
    return sink.icon_type();
  }
};

// MediaRoute

template <>
struct StructTraits<media_router::mojom::MediaRouteDataView,
                    media_router::MediaRoute> {
  static bool Read(media_router::mojom::MediaRouteDataView data,
                   media_router::MediaRoute* out);

  static std::string media_route_id(const media_router::MediaRoute& route) {
    return route.media_route_id();
  }

  static base::Optional<std::string> media_source(
      const media_router::MediaRoute& route) {
    // TODO(imcheng): If we ever convert from Mojo to C++ outside of unit tests,
    // it would be better to make the |media_source_| field on MediaRoute a
    // base::Optional<MediaSource::Id> instead so it can be returned directly
    // here.
    return route.media_source().id().empty()
               ? base::Optional<std::string>()
               : base::make_optional(route.media_source().id());
  }

  static std::string media_sink_id(const media_router::MediaRoute& route) {
    return route.media_sink_id();
  }

  static std::string description(const media_router::MediaRoute& route) {
    return route.description();
  }

  static bool is_local(const media_router::MediaRoute& route) {
    return route.is_local();
  }

  static base::Optional<std::string> custom_controller_path(
      const media_router::MediaRoute& route) {
    return route.custom_controller_path().empty()
               ? base::Optional<std::string>()
               : base::make_optional(route.custom_controller_path());
  }

  static bool for_display(const media_router::MediaRoute& route) {
    return route.for_display();
  }

  static bool is_incognito(const media_router::MediaRoute& route) {
    return route.is_incognito();
  }

  static bool is_offscreen_presentation(const media_router::MediaRoute& route) {
    return route.is_offscreen_presentation();
  }
};

// PresentationConnectionState

template <>
struct EnumTraits<media_router::mojom::MediaRouter::PresentationConnectionState,
                  content::PresentationConnectionState> {
  static media_router::mojom::MediaRouter::PresentationConnectionState ToMojom(
      content::PresentationConnectionState state) {
    switch (state) {
      case content::PRESENTATION_CONNECTION_STATE_CONNECTING:
        return media_router::mojom::MediaRouter::PresentationConnectionState::
            CONNECTING;
      case content::PRESENTATION_CONNECTION_STATE_CONNECTED:
        return media_router::mojom::MediaRouter::PresentationConnectionState::
            CONNECTED;
      case content::PRESENTATION_CONNECTION_STATE_CLOSED:
        return media_router::mojom::MediaRouter::PresentationConnectionState::
            CLOSED;
      case content::PRESENTATION_CONNECTION_STATE_TERMINATED:
        return media_router::mojom::MediaRouter::PresentationConnectionState::
            TERMINATED;
    }
    NOTREACHED() << "Unknown PresentationConnectionState "
                 << static_cast<int>(state);
    return media_router::mojom::MediaRouter::PresentationConnectionState::
        TERMINATED;
  }

  static bool FromMojom(
      media_router::mojom::MediaRouter::PresentationConnectionState input,
      content::PresentationConnectionState* state) {
    switch (input) {
      case media_router::mojom::MediaRouter::PresentationConnectionState::
          CONNECTING:
        *state = content::PRESENTATION_CONNECTION_STATE_CONNECTING;
        return true;
      case media_router::mojom::MediaRouter::PresentationConnectionState::
          CONNECTED:
        *state = content::PRESENTATION_CONNECTION_STATE_CONNECTED;
        return true;
      case media_router::mojom::MediaRouter::PresentationConnectionState::
          CLOSED:
        *state = content::PRESENTATION_CONNECTION_STATE_CLOSED;
        return true;
      case media_router::mojom::MediaRouter::PresentationConnectionState::
          TERMINATED:
        *state = content::PRESENTATION_CONNECTION_STATE_TERMINATED;
        return true;
    }
    return false;
  }
};

// PresentationConnectionCloseReason

template <>
struct EnumTraits<
    media_router::mojom::MediaRouter::PresentationConnectionCloseReason,
    content::PresentationConnectionCloseReason> {
  static media_router::mojom::MediaRouter::PresentationConnectionCloseReason
  ToMojom(content::PresentationConnectionCloseReason reason) {
    switch (reason) {
      case content::PRESENTATION_CONNECTION_CLOSE_REASON_CONNECTION_ERROR:
        return media_router::mojom::MediaRouter::
            PresentationConnectionCloseReason::CONNECTION_ERROR;
      case content::PRESENTATION_CONNECTION_CLOSE_REASON_CLOSED:
        return media_router::mojom::MediaRouter::
            PresentationConnectionCloseReason::CLOSED;
      case content::PRESENTATION_CONNECTION_CLOSE_REASON_WENT_AWAY:
        return media_router::mojom::MediaRouter::
            PresentationConnectionCloseReason::WENT_AWAY;
    }
    NOTREACHED() << "Unknown PresentationConnectionCloseReason "
                 << static_cast<int>(reason);
    return media_router::mojom::MediaRouter::PresentationConnectionCloseReason::
        CONNECTION_ERROR;
  }

  static bool FromMojom(
      media_router::mojom::MediaRouter::PresentationConnectionCloseReason input,
      content::PresentationConnectionCloseReason* state) {
    switch (input) {
      case media_router::mojom::MediaRouter::PresentationConnectionCloseReason::
          CONNECTION_ERROR:
        *state = content::PRESENTATION_CONNECTION_CLOSE_REASON_CONNECTION_ERROR;
        return true;
      case media_router::mojom::MediaRouter::PresentationConnectionCloseReason::
          CLOSED:
        *state = content::PRESENTATION_CONNECTION_CLOSE_REASON_CLOSED;
        return true;
      case media_router::mojom::MediaRouter::PresentationConnectionCloseReason::
          WENT_AWAY:
        *state = content::PRESENTATION_CONNECTION_CLOSE_REASON_WENT_AWAY;
        return true;
    }
    return false;
  }
};

// RouteRequestResultCode

template <>
struct EnumTraits<media_router::mojom::RouteRequestResultCode,
                  media_router::RouteRequestResult::ResultCode> {
  static media_router::mojom::RouteRequestResultCode ToMojom(
      media_router::RouteRequestResult::ResultCode code) {
    switch (code) {
      case media_router::RouteRequestResult::UNKNOWN_ERROR:
        return media_router::mojom::RouteRequestResultCode::UNKNOWN_ERROR;
      case media_router::RouteRequestResult::OK:
        return media_router::mojom::RouteRequestResultCode::OK;
      case media_router::RouteRequestResult::TIMED_OUT:
        return media_router::mojom::RouteRequestResultCode::TIMED_OUT;
      case media_router::RouteRequestResult::ROUTE_NOT_FOUND:
        return media_router::mojom::RouteRequestResultCode::ROUTE_NOT_FOUND;
      case media_router::RouteRequestResult::SINK_NOT_FOUND:
        return media_router::mojom::RouteRequestResultCode::SINK_NOT_FOUND;
      case media_router::RouteRequestResult::INVALID_ORIGIN:
        return media_router::mojom::RouteRequestResultCode::INVALID_ORIGIN;
      case media_router::RouteRequestResult::INCOGNITO_MISMATCH:
        return media_router::mojom::RouteRequestResultCode::INCOGNITO_MISMATCH;
      case media_router::RouteRequestResult::NO_SUPPORTED_PROVIDER:
        return media_router::mojom::RouteRequestResultCode::
            NO_SUPPORTED_PROVIDER;
      case media_router::RouteRequestResult::CANCELLED:
        return media_router::mojom::RouteRequestResultCode::CANCELLED;
      default:
        NOTREACHED() << "Unknown RouteRequestResultCode "
                     << static_cast<int>(code);
        return media_router::mojom::RouteRequestResultCode::UNKNOWN_ERROR;
    }
  }

  static bool FromMojom(media_router::mojom::RouteRequestResultCode input,
                        media_router::RouteRequestResult::ResultCode* output) {
    switch (input) {
      case media_router::mojom::RouteRequestResultCode::UNKNOWN_ERROR:
        *output = media_router::RouteRequestResult::UNKNOWN_ERROR;
        return true;
      case media_router::mojom::RouteRequestResultCode::OK:
        *output = media_router::RouteRequestResult::OK;
        return true;
      case media_router::mojom::RouteRequestResultCode::TIMED_OUT:
        *output = media_router::RouteRequestResult::TIMED_OUT;
        return true;
      case media_router::mojom::RouteRequestResultCode::ROUTE_NOT_FOUND:
        *output = media_router::RouteRequestResult::ROUTE_NOT_FOUND;
        return true;
      case media_router::mojom::RouteRequestResultCode::SINK_NOT_FOUND:
        *output = media_router::RouteRequestResult::SINK_NOT_FOUND;
        return true;
      case media_router::mojom::RouteRequestResultCode::INVALID_ORIGIN:
        *output = media_router::RouteRequestResult::INVALID_ORIGIN;
        return true;
      case media_router::mojom::RouteRequestResultCode::INCOGNITO_MISMATCH:
        *output = media_router::RouteRequestResult::INCOGNITO_MISMATCH;
        return true;
      case media_router::mojom::RouteRequestResultCode::NO_SUPPORTED_PROVIDER:
        *output = media_router::RouteRequestResult::NO_SUPPORTED_PROVIDER;
        return true;
      case media_router::mojom::RouteRequestResultCode::CANCELLED:
        *output = media_router::RouteRequestResult::CANCELLED;
        return true;
    }
    return false;
  }
};

}  // namespace mojo

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTER_STRUCT_TRAITS_H_
