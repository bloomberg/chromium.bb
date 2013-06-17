// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_FULLSTREAM_UI_POLICY_H_
#define CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_FULLSTREAM_UI_POLICY_H_

#include <string>
#include <vector>
#include "chrome/browser/extensions/activity_log/activity_log_policy.h"

class GURL;

namespace extensions {

class ActivityDatabase;

// A policy for logging the full stream of actions, including all arguments.
// It's mostly intended to be used in testing and analysis.
class FullStreamUIPolicy : public ActivityLogPolicy {
 public:
  // For more info about these member functions, see the super class.
  explicit FullStreamUIPolicy(Profile* profile);

  virtual ~FullStreamUIPolicy();

  virtual void ProcessAction(ActionType action_type,
                             const std::string& extension_id,
                             const std::string& name, const GURL& gurl,
                             const base::ListValue* args,
                             const base::DictionaryValue* details) OVERRIDE;

  virtual void SaveState() OVERRIDE {}

  // TODO(felt,dbabic) This is overly specific to FullStreamUIPolicy.
  // It assumes that the callback can return a sorted vector of actions.  Some
  // policies might not do that.  For instance, imagine a trivial policy that
  // just counts the frequency of certain actions within some time period,
  // this call would be meaningless, as it couldn't return anything useful.
  virtual void ReadData(
      const std::string& extension_id,
      const int day,
      const base::Callback
          <void(scoped_ptr<std::vector<scoped_refptr<Action> > >)>& callback)
      const OVERRIDE;

  virtual void SetSaveStateOnRequestOnly() OVERRIDE;

  // Returns the actual key for a given key type
  virtual std::string GetKey(ActivityLogPolicy::KeyType key_id) const OVERRIDE;

 protected:
  // Concatenates arguments
  virtual std::string ProcessArguments(ActionType action_type,
                                       const std::string& name,
                                       const base::ListValue* args) const;

  virtual void ProcessWebRequestModifications(
      base::DictionaryValue& details,
      std::string& details_string) const;

  // We initialize this on the same thread as the ActivityLog and policy, but
  // then subsequent operations occur on the DB thread. Instead of destructing
  // the ActivityDatabase, we call its Close() method on the DB thread and it
  // commits suicide.
  ActivityDatabase* db_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_FULLSTREAM_UI_POLICY_H_
