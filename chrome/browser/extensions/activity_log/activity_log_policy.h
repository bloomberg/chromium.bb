// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_ACTIVITY_LOG_POLICY_H_
#define CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_ACTIVITY_LOG_POLICY_H_

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/extensions/activity_log/activity_actions.h"
#include "chrome/browser/extensions/activity_log/activity_database.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

class Profile;
class GURL;

namespace base {
class FilePath;
}

namespace extensions {

class Extension;

// An abstract class for processing and summarizing activity log data.
// Subclasses will generally store data in an SQLite database (the
// ActivityLogDatabasePolicy subclass includes some helper methods to assist
// with this case), but this is not absolutely required.
//
// Implementations should support:
// (1) Receiving Actions to process, and summarizing, compression, and storing
//     these as appropriate.
// (2) Reading Actions back from storage.
//
// Implementations based on a database should likely implement
// ActivityDatabase::Delegate, which provides hooks on database events and
// allows the database to periodically request that actions (which the policy
// is responsible for queueing) be flushed to storage.
//
// Since every policy implementation might summarize data differently, the
// database implementation is policy-specific and therefore completely
// encapsulated in the policy class.  All the member functions can be called
// on the UI thread.
class ActivityLogPolicy {
 public:
  enum PolicyType {
    POLICY_FULLSTREAM,
    POLICY_NOARGS,
    POLICY_INVALID,
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

  // Instead of a public destructor, ActivityLogPolicy objects have a Close()
  // method which will cause the object to be deleted (but may do so on another
  // thread or in a deferred fashion).
  virtual void Close() = 0;

  // Updates the internal state of the model summarizing actions and possibly
  // writes to the database.  Implements the default policy storing internal
  // state to memory every 5 min.
  virtual void ProcessAction(scoped_refptr<Action> action) = 0;

  // Gets all actions for a given extension for the specified day. 0 = today,
  // 1 = yesterday, etc. Only returns 1 day at a time. Actions are sorted from
  // newest to oldest. Results as passed to the specified callback when
  // available.
  //
  // TODO(felt,dbabic) This is overly specific to the current implementation of
  // the FullStreamUIPolicy.  We should refactor it to use a more generic read
  // function, for example one that takes a dictionary of query parameters
  // (extension_id, time range, etc.).
  virtual void ReadData(
      const std::string& extension_id,
      const int day,
      const base::Callback
          <void(scoped_ptr<Action::ActionVector>)>& callback) = 0;

  virtual std::string GetKey(KeyType key_id) const;

  // For unit testing only.
  void SetClockForTesting(base::Clock* clock) { testing_clock_ = clock; }

 protected:
  // An ActivityLogPolicy is not directly destroyed.  Instead, call Close()
  // which will cause the object to be deleted when it is safe.
  virtual ~ActivityLogPolicy();

  // Returns Time::Now() unless a mock clock has been installed with
  // SetClockForTesting, in which case the time according to that clock is used
  // instead.
  base::Time Now() const;

 private:
  // Support for a mock clock for testing purposes.  This is used by ReadData
  // to determine the date for "today" when when interpreting date ranges to
  // fetch.  This has no effect on batching of writes to the database.
  base::Clock* testing_clock_;

  DISALLOW_COPY_AND_ASSIGN(ActivityLogPolicy);
};

// A subclass of ActivityLogPolicy which is designed for policies that use
// database storage; it contains several useful helper methods.
class ActivityLogDatabasePolicy : public ActivityLogPolicy,
                                  public ActivityDatabase::Delegate {
 public:
  ActivityLogDatabasePolicy(Profile* profile,
                            const base::FilePath& database_name);

 protected:
  // The Schedule methods dispatch the calls to the database on a
  // separate thread.
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

  // Access to the underlying ActivityDatabase.
  ActivityDatabase* activity_database() const { return db_; }

  // Access to the SQL connection in the ActivityDatabase.  This should only be
  // called from the database thread.  May return NULL if the database is not
  // valid.
  sql::Connection* GetDatabaseConnection() const;

 private:
  // See the comments for the ActivityDatabase class for a discussion of how
  // database cleanup runs.
  ActivityDatabase* db_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_ACTIVITY_LOG_POLICY_H_
