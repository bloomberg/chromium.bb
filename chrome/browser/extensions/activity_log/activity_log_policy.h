// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_ACTIVITY_LOG_POLICY_H_
#define CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_ACTIVITY_LOG_POLICY_H_

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer.h"
#include "base/values.h"
#include "chrome/browser/extensions/activity_log/activity_actions.h"
#include "content/public/browser/browser_thread.h"
#include "googleurl/src/gurl.h"

class Profile;
class GURL;

namespace extensions {

class Extension;

// Abstract class for summarizing data and storing it into the database.  Any
// subclass should implement the following functionality:
//
// (1) Summarization (and possibly) compression of data
// (2) Periodical saving of the in-memory state with the database
// (3) Periodical database cleanup
//
// Since every policy implementation might summarize data differently, the
// database implementation is policy-specific and therefore completely
// encapsulated in the policy class.  All the member functions can be called
// on the UI thread, because all DB operations are dispatched via the
// ActivityDatabase.
class ActivityLogPolicy {
 public:
  enum PolicyType {
    POLICY_FULLSTREAM,
    POLICY_NOARGS,
    POLICY_INVALID,
  };

  enum ActionType {
    ACTION_API,
    ACTION_EVENT,
    ACTION_BLOCKED,
    ACTION_DOM,
    ACTION_WEB_REQUEST,
  };

  // For all subclasses, add all the key types they might support here.
  // The actual key is returned by calling GetKey(KeyType).  The subclasses
  // are free to return an empty string for keys they don't support.
  // For every key added here, you should update the GetKey member function
  // for at least one policy.
  enum KeyType {
    PARAM_KEY_REASON,      // Why an action was blocked
    PARAM_KEY_DOM_ACTION,  // Getter, Setter, Method,...
    PARAM_KEY_URL_TITLE,
    PARAM_KEY_DETAILS_STRING,
    PARAM_KEY_EXTRA,
  };

  // Parameters are the profile and the thread that will be used to execute
  // the callback when ReadData is called.
  // TODO(felt,dbabic)  Since only ReadData uses thread_id, it would be
  // cleaner to pass thread_id as a param of ReadData directly.
  explicit ActivityLogPolicy(Profile* profile);
  virtual ~ActivityLogPolicy();

  // Updates the internal state of the model summarizing actions and possibly
  // writes to the database.  Implements the default policy storing internal
  // state to memory every 5 min.
  virtual void ProcessAction(
      ActionType action_type,
      const std::string& extension_id,
      const std::string& name,                    // action name
      const GURL& gurl,                           // target URL
      const base::ListValue* args,                // arguments
      const base::DictionaryValue* details) = 0;  // details

  // Saves the internal state in the memory into the database.  Must be
  // written so as to be thread-safe, as it can be called from a timer that
  // saves state periodically and explicitly.
  virtual void SaveState() { }

  // Pass the parameters as a set of key-value pairs and return data back via
  // a callback passing results as a set of key-value pairs.  The keys are
  // policy-specific.
  virtual void ReadData(
      const base::DictionaryValue& parameters,
      const base::Callback
          <void(scoped_ptr<base::DictionaryValue>)>& callback) const {}

  // TODO(felt,dbabic) This is overly specific to the current implementation
  // of the FullStreamUIPolicy.  We should refactor it to use the above
  // more general member function.
  virtual void ReadData(
      const std::string& extension_id,
      const int day,
      const base::Callback
          <void(scoped_ptr<std::vector<scoped_refptr<Action> > >)>& callback)
      const {}

  // For testing purposes --- disables periodic state saving, making the
  // behavior reproducible.
  virtual void SetSaveStateOnRequestOnly();

  virtual std::string GetKey(KeyType key_id) const;

 protected:
  // The Schedule methods dispatch the calls to the database on a
  // separate thread. We dispatch to the UI thread if the DB thread doesn't
  // exist, which should only happen in tests where there is no DB thread.
  template<typename DatabaseType, typename DatabaseFunc>
  void ScheduleAndForget(DatabaseType db, DatabaseFunc func) {
    content::BrowserThread::PostTask(
        content::BrowserThread::DB,
        FROM_HERE,
        base::Bind(func, base::Unretained(db)));
  }

  template<typename DatabaseType, typename DatabaseFunc, typename ArgA>
  void ScheduleAndForget(DatabaseType db, DatabaseFunc func, ArgA a) {
    content::BrowserThread::PostTask(
        content::BrowserThread::DB,
        FROM_HERE,
        base::Bind(func, base::Unretained(db), a));
  }

  template<typename DatabaseType, typename DatabaseFunc,
      typename ArgA, typename ArgB>
  void ScheduleAndForget(DatabaseType db, DatabaseFunc func, ArgA a, ArgB b) {
    content::BrowserThread::PostTask(
        content::BrowserThread::DB,
        FROM_HERE,
        base::Bind(func, base::Unretained(db), a, b));
  }

  base::FilePath profile_base_path_;
  base::RepeatingTimer<ActivityLogPolicy> timer_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_ACTIVITY_LOG_POLICY_H_
