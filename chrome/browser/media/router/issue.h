// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_ISSUE_H_
#define CHROME_BROWSER_MEDIA_ROUTER_ISSUE_H_

#include <string>
#include <vector>

#include "base/logging.h"
#include "chrome/browser/media/router/media_route.h"

namespace media_router {

// The text that corresponds with an action a user can take for a Issue.
class IssueAction {
 public:
  enum Type {
    TYPE_DISMISS,
    TYPE_LEARN_MORE,
    TYPE_MAX /* Denotes enum value boundary. */
  };

  explicit IssueAction(const Type type);
  ~IssueAction();

  Type type() const { return type_; }

  bool Equals(const IssueAction& other) const { return type_ == other.type_; }

 private:
  Type type_;
};

// Contains the information relevant to an issue.
class Issue {
 public:
  using Id = std::string;

  enum Severity { FATAL, WARNING, NOTIFICATION };

  // Creates a custom issue.
  // |title|: The title for the issue.
  // |message|: The optional description message for the issue.
  // |default_action|: Default action user can take to resolve the issue.
  // |secondary_actions|: Array of options user can take to resolve the
  //                      issue. Can be empty. Currently only one secondary
  //                      action is supported.
  // |route_id|: The route id, or empty if global.
  // |severity|: The severity of the issue.
  // |is_blocking|: True if the issue needs to be resolved before continuing.
  // |help_url|: Required if one of the actions is learn-more.
  Issue(const std::string& title,
        const std::string& message,
        const IssueAction& default_action,
        const std::vector<IssueAction>& secondary_actions,
        const MediaRoute::Id& route_id,
        const Severity severity,
        bool is_blocking,
        const std::string& helpUrl);

  ~Issue();

  // See constructor comments for more information about these fields.
  const std::string& title() const { return title_; }
  const std::string& message() const { return message_; }
  IssueAction default_action() const { return default_action_; }
  const std::vector<IssueAction>& secondary_actions() const {
    return secondary_actions_;
  }
  MediaRoute::Id route_id() const { return route_id_; }
  Severity severity() const { return severity_; }
  const Issue::Id& id() const { return id_; }
  bool is_blocking() const { return is_blocking_; }
  bool is_global() const { return route_id_.empty(); }
  const GURL& help_url() const { return help_url_; }

  bool Equals(const Issue& other) const;

 private:
  std::string title_;
  std::string message_;
  IssueAction default_action_;
  std::vector<IssueAction> secondary_actions_;
  std::string route_id_;
  Severity severity_;
  Issue::Id id_;
  bool is_blocking_;
  GURL help_url_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_ISSUE_H_
