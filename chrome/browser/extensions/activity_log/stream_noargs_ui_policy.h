// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_STREAM_NOARGS_UI_POLICY_H_
#define CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_STREAM_NOARGS_UI_POLICY_H_

#include <string>
#include "base/containers/hash_tables.h"
#include "chrome/browser/extensions/activity_log/api_actions.h"
#include "chrome/browser/extensions/activity_log/fullstream_ui_policy.h"

namespace extensions {

// A policy for logging the stream of actions, but without arguments.
class StreamWithoutArgsUIPolicy : public FullStreamUIPolicy {
 public:
  explicit StreamWithoutArgsUIPolicy(Profile* profile);
  virtual ~StreamWithoutArgsUIPolicy();

 protected:
  virtual std::string ProcessArguments(ActionType action_type,
                                       const std::string& name,
                                       const base::ListValue* args)
                                       const OVERRIDE;

  virtual void ProcessWebRequestModifications(
      base::DictionaryValue& details,
      std::string& details_string) const OVERRIDE;
 private:
  base::hash_set<std::string> arg_whitelist_api_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_STREAM_NOARGS_UI_POLICY_H_
