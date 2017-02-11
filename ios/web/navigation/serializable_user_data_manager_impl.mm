// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/serializable_user_data_manager_impl.h"

#import "base/mac/foundation_util.h"
#import "ios/web/public/web_state/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {
namespace {
// The key under which SerializableUserDataMangerWrapper are stored in the
// WebState's user data.
const void* const kSerializableUserDataManagerKey =
    &kSerializableUserDataManagerKey;
// The key under which SerializableUserDataImpl's data is encoded.
NSString* const kSerializedUserDataKey = @"serializedUserData";

// Wrapper class used to associate SerializableUserDataManagerImpls with its
// associated WebState.
class SerializableUserDataManagerWrapper : base::SupportsUserData::Data {
 public:
  // Returns the SerializableUserDataMangerWrapper associated with |web_state|,
  // creating one if necessary.
  static SerializableUserDataManagerWrapper* FromWebState(
      web::WebState* web_state) {
    DCHECK(web_state);
    SerializableUserDataManagerWrapper* wrapper =
        static_cast<SerializableUserDataManagerWrapper*>(
            web_state->GetUserData(kSerializableUserDataManagerKey));
    if (!wrapper)
      wrapper = new SerializableUserDataManagerWrapper(web_state);
    return wrapper;
  }

  // Returns the manager owned by this wrapper.
  SerializableUserDataManagerImpl* manager() { return &manager_; }

 private:
  // The SerializableUserDataMangerWrapper owned by this object.
  SerializableUserDataManagerImpl manager_;

  // Private constructor.  The created object will be added to |web_state|'s
  // user data.
  SerializableUserDataManagerWrapper(web::WebState* web_state) {
    DCHECK(web_state);
    web_state->SetUserData(kSerializableUserDataManagerKey, this);
  }
};
}  // namespace

// static
std::unique_ptr<SerializableUserData> SerializableUserData::Create() {
  return std::unique_ptr<SerializableUserData>(new SerializableUserDataImpl());
}

SerializableUserDataImpl::SerializableUserDataImpl()
    : data_([[NSDictionary alloc] init]) {}

SerializableUserDataImpl::~SerializableUserDataImpl() {}

SerializableUserDataImpl::SerializableUserDataImpl(NSDictionary* data)
    : data_([data copy]) {}

void SerializableUserDataImpl::Encode(NSCoder* coder) {
  [coder encodeObject:data_ forKey:kSerializedUserDataKey];
}

void SerializableUserDataImpl::Decode(NSCoder* coder) {
  NSDictionary* data = base::mac::ObjCCastStrict<NSDictionary>(
      [coder decodeObjectForKey:kSerializedUserDataKey]);
  data_.reset([data mutableCopy]);
}

// static
SerializableUserDataManager* SerializableUserDataManager::FromWebState(
    web::WebState* web_state) {
  DCHECK(web_state);
  return SerializableUserDataManagerWrapper::FromWebState(web_state)->manager();
}

SerializableUserDataManagerImpl::SerializableUserDataManagerImpl()
    : data_([[NSMutableDictionary alloc] init]) {}

SerializableUserDataManagerImpl::~SerializableUserDataManagerImpl() {}

void SerializableUserDataManagerImpl::AddSerializableData(id<NSCoding> data,
                                                          NSString* key) {
  DCHECK(data);
  DCHECK(key.length);
  [data_ setObject:data forKey:key];
}

id<NSCoding> SerializableUserDataManagerImpl::GetValueForSerializationKey(
    NSString* key) {
  return [data_ objectForKey:key];
}

std::unique_ptr<SerializableUserData>
SerializableUserDataManagerImpl::CreateSerializableUserData() const {
  return std::unique_ptr<SerializableUserData>(
      new SerializableUserDataImpl(data_));
}

void SerializableUserDataManagerImpl::AddSerializableUserData(
    SerializableUserData* data) {
  DCHECK(data);
  SerializableUserDataImpl* data_impl =
      static_cast<SerializableUserDataImpl*>(data);
  data_.reset([data_impl->data() mutableCopy]);
}

}  // namespace web
