// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_STREAM_NOARGS_UI_POLICY_H_
#define CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_STREAM_NOARGS_UI_POLICY_H_

#include <string>

#include "base/containers/hash_tables.h"
#include "chrome/browser/extensions/activity_log/fullstream_ui_policy.h"

namespace extensions {

// A policy for logging the stream of actions, but without arguments.
class StreamWithoutArgsUIPolicy : public FullStreamUIPolicy {
 public:
  explicit StreamWithoutArgsUIPolicy(Profile* profile);
  virtual ~StreamWithoutArgsUIPolicy();

 protected:
  // Clears the args field of the action (except for some DOM events and a
  // small whitelisted set of api_names).  For WEB_REQUEST actions, keeps keys
  // but removes values from the "web_request" dictionary in other.
  virtual scoped_refptr<Action> ProcessArguments(
      scoped_refptr<Action> action) const OVERRIDE;

 private:
  base::hash_set<std::string> arg_whitelist_api_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_STREAM_NOARGS_UI_POLICY_H_
