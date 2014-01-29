// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/user_list.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

namespace gcm {
const int64 kInvalidSerialNumber = -1;

UserList::UserInfo::UserInfo()
    : serial_number(kInvalidSerialNumber),
      delegate(NULL) {
}

UserList::UserInfo::UserInfo(int64 serial_number)
    : serial_number(serial_number),
      delegate(NULL) {
}

UserList::UserInfo::UserInfo(GCMClient::Delegate* delegate,
                             const SetDelegateCallback& callback)
    : serial_number(kInvalidSerialNumber),
      delegate(delegate),
      callback(callback) {
}

UserList::UserInfo::~UserInfo() {
}

UserList::UserList(GCMStore* gcm_store)
    : initialized_(false),
      next_serial_number_(kInvalidSerialNumber),
      gcm_store_(gcm_store) {
}

UserList::~UserList() {
}

void UserList::Initialize(const GCMStore::SerialNumberMappings& result) {
  DCHECK(!initialized_);
  DCHECK_GT(result.next_serial_number, 0);

  initialized_ = true;
  next_serial_number_ = result.next_serial_number;
  for (std::map<std::string, int64>::const_iterator iter =
           result.user_serial_numbers.begin();
       iter != result.user_serial_numbers.end();
       ++iter) {
    SetSerialNumber(iter->first, iter->second);
  }

  for (UserInfoMap::iterator iter = delegates_.begin();
       iter != delegates_.end();
       ++iter) {
    // Process only users with delegate and callback present.
    if (iter->second.delegate && !iter->second.callback.is_null()) {
      if (iter->second.serial_number == kInvalidSerialNumber)
        AssignSerialNumber(iter->first);
      else
        OnSerialNumberReady(iter);
    }
  }
}

void UserList::SetDelegate(const std::string& username,
                           GCMClient::Delegate* delegate,
                           const SetDelegateCallback& callback) {
  DCHECK(!username.empty());
  DCHECK(delegate);

  UserInfoMap::iterator iter = delegates_.find(username);
  if (iter != delegates_.end()) {
    DCHECK(initialized_);
    DCHECK(!iter->second.delegate);
    iter->second.delegate = delegate;
    iter->second.callback = callback;
  } else {
    delegates_.insert(
        iter,
        UserInfoMap::value_type(username, UserInfo(delegate, callback)));
  }

  if (initialized_) {
    UserInfoMap::iterator iter = delegates_.find(username);
    if (iter->second.serial_number == kInvalidSerialNumber)
      AssignSerialNumber(iter->first);
    else
      OnSerialNumberReady(iter);
  }
}

void UserList::AssignSerialNumber(const std::string& username) {
  DCHECK(initialized_);
  DCHECK_EQ(delegates_.count(username), 1UL);
  DCHECK_EQ(delegates_[username].serial_number, kInvalidSerialNumber);

  // Start by incrementing the |next_serial_number_| and persist it in the GCM
  // store.
  int64 serial_number = next_serial_number_++;
  gcm_store_->SetNextSerialNumber(
      serial_number,
      base::Bind(&UserList::IncrementSerialNumberCompleted,
                 base::Unretained(this),
                 username,
                 serial_number));
}

void UserList::IncrementSerialNumberCompleted(const std::string& username,
                                              int64 user_serial_number,
                                              bool success) {
  // TODO(fgorski): Introduce retry logic.
  if (!success)
    DCHECK(success) << "Updating the next serial number failed.";

  SetSerialNumber(username, user_serial_number);
  gcm_store_->AddUserSerialNumber(
      username,
      user_serial_number,
      base::Bind(&UserList::AssignSerialNumberCompleted,
                 base::Unretained(this),
                 username));
}

void UserList::AssignSerialNumberCompleted(const std::string& username,
                                           bool success) {
  // TODO(fgorski): Introduce retry logic.
  if (!success) {
    DVLOG(1) << "It was not possible to persist username to serial number"
             << " mapping for username: " << username;
    return;
  }

  UserInfoMap::iterator iter = delegates_.find(username);
  DCHECK(iter != delegates_.end());
  OnSerialNumberReady(iter);
}

void UserList::OnSerialNumberReady(const UserInfoMap::iterator& iter) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(iter->second.callback,
                 iter->first,
                 iter->second.serial_number));
  iter->second.callback.Reset();
}

void UserList::SetSerialNumber(const std::string& username,
                               int64 serial_number) {
  DCHECK(!username.empty());
  DCHECK_GT(serial_number, 0L);

  UserInfoMap::iterator iter = delegates_.find(username);
  if (iter != delegates_.end()) {
    DCHECK_EQ(iter->second.serial_number, kInvalidSerialNumber);
    iter->second.serial_number = serial_number;
  } else {
    delegates_.insert(
        iter, UserInfoMap::value_type(username, UserInfo(serial_number)));
  }
}

GCMClient::Delegate* UserList::GetDelegateBySerialNumber(int64 serial_number)
    const {
  for (UserInfoMap::const_iterator iter = delegates_.begin();
       iter != delegates_.end();
       ++iter) {
    if (iter->second.serial_number == serial_number)
      return iter->second.delegate;
  }
  return NULL;
}

std::vector<GCMClient::Delegate*> UserList::GetAllDelegates() const {
  std::vector<GCMClient::Delegate*> delegates;
  for (UserInfoMap::const_iterator iter = delegates_.begin();
       iter != delegates_.end(); ++iter) {
    if (iter->second.delegate)
      delegates.push_back(iter->second.delegate);
  }
  return delegates;
}

GCMClient::Delegate* UserList::GetDelegateByUsername(
    const std::string& username) const {
  UserInfoMap::const_iterator iter = delegates_.find(username);
  return iter != delegates_.end() ? iter->second.delegate : NULL;
}

int64 UserList::GetSerialNumberForUsername(const std::string& username) const {
  UserInfoMap::const_iterator iter = delegates_.find(username);
  return iter != delegates_.end() ? iter->second.serial_number
                                  : kInvalidSerialNumber;
}

}  // namespace gcm
