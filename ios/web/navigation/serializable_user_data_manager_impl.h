// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_SERIALIZABLE_USER_DATA_MANAGER_IMPL_H_
#define IOS_WEB_NAVIGATION_SERIALIZABLE_USER_DATA_MANAGER_IMPL_H_

#import "base/mac/scoped_nsobject.h"
#import "ios/web/public/serializable_user_data_manager.h"

namespace web {

class SerializableUserDataImpl : public SerializableUserData {
 public:
  SerializableUserDataImpl();
  ~SerializableUserDataImpl();

  // Constructor taking the NSDictionary holding the serializable data.
  explicit SerializableUserDataImpl(NSDictionary* data);

  // SerializableUserData:
  void Encode(NSCoder* coder) override;
  void Decode(NSCoder* coder) override;

  // Returns the serializable data.
  NSDictionary* data() { return data_; };

 private:
  // Decodes the values that were previously encoded using CRWSessionStorage's
  // NSCoding implementation and returns an NSDictionary using the new
  // serialization keys.
  // TODO(crbug.com/691800): Remove legacy support.
  NSDictionary* GetDecodedLegacyValues(NSCoder* coder);

  // The dictionary passed on initialization.  After calling Decode(), this will
  // contain the data that is decoded from the NSCoder.
  base::scoped_nsobject<NSDictionary> data_;
  // Some values that were previously persisted directly in CRWSessionStorage
  // are now serialized using SerializableUserData, and this dictionary is used
  // to decode these values. The keys are the legacy encoding keys, and the
  // values are their corresponding serializable user data keys.
  base::scoped_nsobject<NSDictionary> legacy_key_conversions_;
};

class SerializableUserDataManagerImpl : public SerializableUserDataManager {
 public:
  SerializableUserDataManagerImpl();
  ~SerializableUserDataManagerImpl();

  // SerializableUserDataManager:
  void AddSerializableData(id<NSCoding> data, NSString* key) override;
  id<NSCoding> GetValueForSerializationKey(NSString* key) override;
  std::unique_ptr<SerializableUserData> CreateSerializableUserData()
      const override;
  void AddSerializableUserData(SerializableUserData* data) override;

 private:
  // The dictionary that stores serializable user data.
  base::scoped_nsobject<NSMutableDictionary> data_;
};

}  // namespace web

#endif  // IOS_WEB_NAVIGATION_SERIALIZABLE_USER_DATA_MANAGER_IMPL_H_
