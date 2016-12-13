// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTER_STRUCT_TRAITS_H_
#define CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTER_STRUCT_TRAITS_H_

#include <string>

#include "chrome/browser/media/router/issue.h"
#include "chrome/browser/media/router/mojo/media_router.mojom.h"
#include "mojo/common/common_custom_types_struct_traits.h"

namespace mojo {

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

}  // namespace mojo

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTER_STRUCT_TRAITS_H_
