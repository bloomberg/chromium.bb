// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_CONSUMER_BASE_SUPPORTS_USER_DATA_H_
#define IOS_PUBLIC_CONSUMER_BASE_SUPPORTS_USER_DATA_H_

namespace ios {

class SupportsUserDataInternal;

// This is a helper for classes that want to allow users to stash random data by
// key. At destruction all the objects will be destructed.
class SupportsUserData {
 public:
  SupportsUserData();

  // Derive from this class and add your own data members to associate extra
  // information with this object. Alternatively, add this as a public base
  // class to any class with a virtual destructor.
  class Data {
   public:
    virtual ~Data() {}
  };

  // The user data allows the clients to associate data with this object.
  // Multiple user data values can be stored under different keys.
  // This object will TAKE OWNERSHIP of the given data pointer, and will
  // delete the object if it is changed or the object is destroyed.
  Data* GetUserData(const void* key) const;
  void SetUserData(const void* key, Data* data);
  void RemoveUserData(const void* key);

  // SupportsUserData is not thread-safe, and on debug build will assert it is
  // only used on one thread. Calling this method allows the caller to hand
  // the SupportsUserData instance across threads. Use only if you are taking
  // full control of the synchronization of that handover.
  void DetachUserDataThread();

 protected:
  virtual ~SupportsUserData();

 private:
  // Owned by this object and scoped to its lifetime.
  SupportsUserDataInternal* internal_helper_;
};

}  // namespace ios

#endif  // IOS_PUBLIC_CONSUMER_BASE_SUPPORTS_USER_DATA_H_
