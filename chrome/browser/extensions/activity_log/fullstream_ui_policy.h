// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_FULLSTREAM_UI_POLICY_H_
#define CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_FULLSTREAM_UI_POLICY_H_

#include <string>
#include <vector>
#include "chrome/browser/extensions/activity_log/activity_database.h"
#include "chrome/browser/extensions/activity_log/activity_log_policy.h"

class GURL;

namespace extensions {

// A policy for logging the full stream of actions, including all arguments.
// It's mostly intended to be used in testing and analysis.
class FullStreamUIPolicy : public ActivityLogPolicy,
                           public ActivityDatabase::Delegate {
 public:
  // For more info about these member functions, see the super class.
  explicit FullStreamUIPolicy(Profile* profile);

  virtual void ProcessAction(scoped_refptr<Action> action) OVERRIDE;

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

  // Returns the actual key for a given key type
  virtual std::string GetKey(ActivityLogPolicy::KeyType key_id) const OVERRIDE;

  virtual void Close() OVERRIDE;

  // Database table schema.
  static const char* kTableName;
  static const char* kTableContentFields[];
  static const char* kTableFieldTypes[];
  static const int kTableFieldCount;

 protected:
  // Only ever run by OnDatabaseClose() below; see the comments on the
  // ActivityDatabase class for an overall discussion of how cleanup works.
  virtual ~FullStreamUIPolicy() {}

  // The ActivityDatabase::PolicyDelegate interface.  These are always called
  // from the database thread.
  virtual bool OnDatabaseInit(sql::Connection* db) OVERRIDE;
  virtual void OnDatabaseClose() OVERRIDE;

  // Strips arguments if needed by policy.
  virtual void ProcessArguments(scoped_refptr<Action> action) const;

  // Concatenates arguments.
  virtual std::string JoinArguments(ActionType action_type,
                                    const std::string& name,
                                    const base::ListValue* args) const;

  virtual void ProcessWebRequestModifications(
      base::DictionaryValue& details,
      std::string& details_string) const;

  // See the comments for the ActivityDatabase class for a discussion of how
  // cleanup runs.
  ActivityDatabase* db_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_FULLSTREAM_UI_POLICY_H_
