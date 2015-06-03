// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/guid.h"
#include "chrome/browser/media/router/issue.h"

namespace media_router {

IssueAction::IssueAction(const IssueAction::Type type) : type_(type) {
}

IssueAction::~IssueAction() {
}

Issue::Issue(const std::string& title,
             const std::string& message,
             const IssueAction& default_action,
             const std::vector<IssueAction>& secondary_actions,
             const MediaRoute::Id& route_id,
             const Issue::Severity severity,
             bool is_blocking,
             const std::string& help_url)
    : title_(title),
      message_(message),
      default_action_(default_action),
      secondary_actions_(secondary_actions),
      route_id_(route_id),
      severity_(severity),
      id_(base::GenerateGUID()),
      is_blocking_(is_blocking),
      help_url_(GURL(help_url)) {
  DCHECK(!title_.empty());
  DCHECK(severity_ != FATAL || is_blocking_) << "Severity is " << severity_;

  // Check that the default and secondary actions are not of the same type.
  if (!secondary_actions_.empty())
    DCHECK_NE(default_action_.type(), secondary_actions_[0].type());
}

Issue::~Issue() {
}

bool Issue::Equals(const Issue& other) const {
  return id_ == other.id_;
}

}  // namespace media_router
