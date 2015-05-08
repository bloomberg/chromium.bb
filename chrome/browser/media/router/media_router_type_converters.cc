// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_router_type_converters.h"

namespace mojo {

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
      return media_router::IssueAction::OK;
    case media_router::interfaces::Issue::ActionType::ACTION_TYPE_CANCEL:
      return media_router::IssueAction::CANCEL;
    case media_router::interfaces::Issue::ActionType::ACTION_TYPE_DISMISS:
      return media_router::IssueAction::DISMISS;
    case media_router::interfaces::Issue::ActionType::ACTION_TYPE_LEARN_MORE:
      return media_router::IssueAction::LEARN_MORE;
    default:
      NOTREACHED() << "Unknown issue action type " << action_type;
      return media_router::IssueAction::DISMISS;
  }
}

}  // namespace mojo
