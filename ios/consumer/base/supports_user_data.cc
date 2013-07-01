// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/public/consumer/base/supports_user_data.h"

#include "base/memory/scoped_ptr.h"
#include "base/supports_user_data.h"

namespace ios {

// Class that wraps a ios::SupportsUserData::Data object in a
// base::SupportsUserData::Data object. The wrapper object takes ownership of
// the wrapped object and will delete it on destruction.
class DataAdaptor : public base::SupportsUserData::Data {
 public:
  DataAdaptor(SupportsUserData::Data* data);
  virtual ~DataAdaptor();

  SupportsUserData::Data* data() { return data_.get(); }

 private:
  scoped_ptr<SupportsUserData::Data> data_;
};

DataAdaptor::DataAdaptor(SupportsUserData::Data* data)
    : data_(data) {}

DataAdaptor::~DataAdaptor() {}

// Class that subclasses base::SupportsUserData in order to enable it to
// support ios::SupportsUserData::Data objects. It accomplishes this by
// wrapping these objects internally in ios::DataAdaptor objects.
class SupportsUserDataInternal : public base::SupportsUserData {
 public:
  // Returns the data that is associated with |key|, or NULL if there is no
  // such associated data.
  ios::SupportsUserData::Data* GetIOSUserData(const void* key);

  // Associates |data| with |key|. Takes ownership of |data| and will delete it
  // on either a call to |RemoveUserData(key)| or otherwise on destruction.
  void SetIOSUserData(const void* key, ios::SupportsUserData::Data* data);

 private:
  SupportsUserDataInternal() {}
  virtual ~SupportsUserDataInternal() {}

  friend class ios::SupportsUserData;
};

ios::SupportsUserData::Data* SupportsUserDataInternal::GetIOSUserData(
    const void* key) {
  DataAdaptor* adaptor = static_cast<DataAdaptor*>(
      base::SupportsUserData::GetUserData(key));
  if (!adaptor)
    return NULL;
  return adaptor->data();
}

void SupportsUserDataInternal::SetIOSUserData(
    const void* key, ios::SupportsUserData::Data* data) {
  base::SupportsUserData::SetUserData(key, new DataAdaptor(data));
}

// ios::SupportsUserData implementation.
SupportsUserData::SupportsUserData()
    : internal_helper_(new SupportsUserDataInternal()) {
}

SupportsUserData::~SupportsUserData() {
  delete internal_helper_;
}

SupportsUserData::Data* SupportsUserData::GetUserData(const void* key) const {
  return internal_helper_->GetIOSUserData(key);
}

void SupportsUserData::SetUserData(const void* key, Data* data) {
  internal_helper_->SetIOSUserData(key, data);
}

void SupportsUserData::RemoveUserData(const void* key) {
  internal_helper_->RemoveUserData(key);
}

void SupportsUserData::DetachUserDataThread() {
  internal_helper_->DetachUserDataThread();
}

}  // namespace ios
