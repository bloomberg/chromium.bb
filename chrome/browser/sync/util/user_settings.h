// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_UTIL_USER_SETTINGS_H_
#define CHROME_BROWSER_SYNC_UTIL_USER_SETTINGS_H_

#include <map>
#include <set>
#include <string>

#include "base/file_path.h"
#include "base/lock.h"
#include "build/build_config.h"
#include "chrome/browser/sync/util/signin.h"
#include "chrome/browser/sync/util/sync_types.h"

extern "C" struct sqlite3;

namespace browser_sync {

class URLFactory;

class UserSettings {
 public:
  // db_path is used for the main user settings.
  // passwords_file contains hashes of passwords.
  UserSettings();
  ~UserSettings();
  // Returns false (failure) if the db is a newer version.
  bool Init(const FilePath& settings_path);
  void StoreHashedPassword(const std::string& email,
                           const std::string& password);
  bool VerifyAgainstStoredHash(const std::string& email,
                               const std::string& password);

  // Set the username.
  void SwitchUser(const std::string& email);

  // Saves the email address and the named service token for the given user.
  // Call this multiple times with the same email parameter to save multiple
  // service tokens.
  void SetAuthTokenForService(const std::string& email,
                              const std::string& service_name,
                              const std::string& long_lived_service_token);
  // Erases all saved service tokens.
  void ClearAllServiceTokens();

  // Returns the user name whose credentials have been persisted.
  bool GetLastUser(std::string* username);

  // Returns the user name whose credentials have been persisted as well as a
  // service token for the named service
  bool GetLastUserAndServiceToken(const std::string& service_name,
                                  std::string* username,
                                  std::string* service_token);

  void RememberSigninType(const std::string& signin, SignIn signin_type);
  SignIn RecallSigninType(const std::string& signin, SignIn default_type);

  void RemoveAllGuestSettings();

  void StoreEmailForSignin(const std::string& signin,
                           const std::string& primary_email);

  // Multiple email addresses can map to the same Google Account.  This method
  // returns the primary Google Account email associated with |signin|, which
  // is used as both input and output.
  bool GetEmailForSignin(std::string* signin);

  std::string email() const;

  // Get a unique ID suitable for use as the client ID.  This ID has the
  // lifetime of the user settings database.  You may use this ID if your
  // operating environment does not provide its own unique client ID.
  std::string GetClientId();

 protected:
  struct ScopedDBHandle {
    explicit ScopedDBHandle(UserSettings* settings);
    inline sqlite3* get() const { return *handle_; }
    AutoLock mutex_lock_;
    sqlite3** const handle_;
  };

  friend struct ScopedDBHandle;
  friend class URLFactory;

  void MigrateOldVersionsAsNeeded(sqlite3* const handle, int current_version);

 private:
  std::string email_;
  mutable Lock mutex_;  // protects email_.

  // We keep a single dbhandle.
  sqlite3* dbhandle_;
  Lock dbhandle_mutex_;

  // TODO(sync): Use in-memory cache for service auth tokens on posix.
  // Have someone competent in Windows switch it over to not use Sqlite in the
  // future.
#ifndef OS_WIN
  typedef std::map<std::string, std::string> ServiceTokenMap;
  ServiceTokenMap service_tokens_;
#endif  // OS_WIN

  DISALLOW_COPY_AND_ASSIGN(UserSettings);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_UTIL_USER_SETTINGS_H_
