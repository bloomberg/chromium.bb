// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_ACTIONS_H_
#define CHROME_BROWSER_EXTENSIONS_API_ACTIONS_H_

#include <string>
#include "base/time.h"
#include "chrome/browser/extensions/activity_actions.h"

namespace extensions {

// This class describes API calls that did not run into permissions or quota
// problems.  See BlockedActions for API calls that did not succeed.
class APIAction : public Action {
 public:
  // TODO(felt): I'll finalize this list when making the UI.
  enum APIActionType {
    READ,
    MODIFIED,
    DELETED,
    ADDED,
    ENABLED,
    DISABLED,
    CREATED,
    UNKNOWN_ACTION
  };

  // TODO(felt): I'll finalize this list when making the UI.
  enum APITargetType {
    BOOKMARK,
    TABS,
    HISTORY,
    COOKIES,
    BROWSER_ACTION,
    NOTIFICATION,
    OMNIBOX,
    UNKNOWN_TARGET
  };

  static const char* kTableName;
  static const char* kTableStructure;

  // Create a new APIAction to describe a successful API call.  All
  // parameters are required.
  APIAction(const std::string& extension_id,
            const base::Time& time,
            const APIActionType verb,         // e.g. "ADDED"
            const APITargetType target,       // e.g. "BOOKMARK"
            const std::string& api_call,      // full method signature incl args
            const std::string& extra);        // any extra logging info

  // Record the action in the database.
  virtual void Record(sql::Connection* db) OVERRIDE;

  // Print a APIAction with il8n substitutions for display.
  virtual std::string PrettyPrintFori18n() OVERRIDE;

  // Print a APIAction as a regular string for debugging purposes.
  virtual std::string PrettyPrintForDebug() OVERRIDE;

  // Helper methods for recording the values into the db.
  const std::string& extension_id() const { return extension_id_; }
  const base::Time& time() const { return time_; }
  const std::string& api_call() const { return api_call_; }
  std::string VerbAsString() const;
  std::string TargetAsString() const;
  std::string extra() const { return extra_; }

  // Helper methods for creating a APIAction.
  static APIActionType StringAsActionType(const std::string& str);
  static APITargetType StringAsTargetType(const std::string& str);

 protected:
  virtual ~APIAction();

 private:
  std::string extension_id_;
  base::Time time_;
  APIActionType verb_;
  APITargetType target_;
  std::string api_call_;
  std::string extra_;

  DISALLOW_COPY_AND_ASSIGN(APIAction);
};

}  // namespace

#endif  // CHROME_BROWSER_EXTENSIONS_API_ACTIONS_H_

