// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_ENGINE_USER_LIST_H_
#define GOOGLE_APIS_GCM_ENGINE_USER_LIST_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "google_apis/gcm/engine/gcm_store.h"
#include "google_apis/gcm/gcm_client.h"

namespace gcm {

GCM_EXPORT extern const int64 kInvalidSerialNumber;

// UserList stores mappings between usernames, serial numbers and delegates to
// enable dispatching messages to applications.
class GCM_EXPORT UserList {
 public:
  // A callback used by SetDelegate method to return a |user_serial_number|
  // assigned to the delegate identified by |username|.
  typedef base::Callback<void(const std::string& username,
                              int64 user_serial_number)> SetDelegateCallback;

  explicit UserList(GCMStore* gcm_store);
  ~UserList();

  // Initializes the User List with a set of mappings and next serial number.
  void Initialize(const GCMStore::SerialNumberMappings& result);

  // Sets a user delegate for |username|. It will create a new entry for the
  // user if one does not exist.
  void SetDelegate(const std::string& username,
                   GCMClient::Delegate* delegate,
                   const SetDelegateCallback& callback);

  // Returns a delegate for the user identified by |serial_number| or NULL, if
  // a matching delegate was not found.
  GCMClient::Delegate* GetDelegateBySerialNumber(int64 serial_number) const;

  // Returns a delegate for the user identified by |username| or NULL, if a
  // matching delegate was not found.
  GCMClient::Delegate* GetDelegateByUsername(const std::string& username) const;

  // Returns all delegates.
  std::vector<GCMClient::Delegate*> GetAllDelegates() const;

  // Gets the serial number assigned to a specified |username|, if one is
  // assigned, the value will be positive. If there is no matching delegate or
  // it is not yet assigned a serial number, the result will be equal to
  // kInvalidSerialNumber.
  int64 GetSerialNumberForUsername(const std::string& username) const;

 private:
  friend class UserListTest;

  struct UserInfo {
    UserInfo();
    explicit UserInfo(int64 serial_number);
    UserInfo(GCMClient::Delegate* delegate,
             const SetDelegateCallback& callback);
    ~UserInfo();

    int64 serial_number;
    // Delegate related to the username. Not owned by the UserDelegate.
    GCMClient::Delegate* delegate;
    SetDelegateCallback callback;
  };
  typedef std::map<std::string, UserInfo> UserInfoMap;

  // Assigns a serial number to the user identitified by |username|.
  void AssignSerialNumber(const std::string& username);

  // A callback invoked once the Backend is done updating the next serial
  // number.
  void IncrementSerialNumberCompleted(const std::string& username,
                                      int64 user_serial_number,
                                      bool success);

  // Callback for serial number completion.
  void AssignSerialNumberCompleted(const std::string& username, bool success);

  // Concludes the process of setting a delegate by running a callback with
  // |username| and |serial_number| assigned to that |username|. It will also
  // reset the callback, so that it is not called again.
  void OnSerialNumberReady(const UserInfoMap::iterator& iter);

  // Sets the serial number related to the username. It expects the entry to not
  // exist yet and will create it.
  void SetSerialNumber(const std::string& username, int64 serial_number);

  bool initialized_;
  int64 next_serial_number_;
  UserInfoMap delegates_;
  GCMStore* gcm_store_;

  DISALLOW_COPY_AND_ASSIGN(UserList);
};

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_ENGINE_USER_LIST_H_
