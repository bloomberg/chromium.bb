// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/invalidation_service_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/callback.h"
#include "components/invalidation/object_id_invalidation_map.h"
#include "google/cacheinvalidation/types.pb.h"
#include "jni/InvalidationService_jni.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;

namespace invalidation {

InvalidationServiceAndroid::InvalidationServiceAndroid(jobject context)
    : invalidator_state_(syncer::INVALIDATIONS_ENABLED),
      logger_() {
  DCHECK(CalledOnValidThread());
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> local_java_ref =
      Java_InvalidationService_create(env,
          context,
          reinterpret_cast<intptr_t>(this));
  java_ref_.Reset(env, local_java_ref.obj());
}

InvalidationServiceAndroid::~InvalidationServiceAndroid() { }

void InvalidationServiceAndroid::RegisterInvalidationHandler(
    syncer::InvalidationHandler* handler) {
  DCHECK(CalledOnValidThread());
  invalidator_registrar_.RegisterHandler(handler);
  logger_.OnRegistration(handler->GetOwnerName());
}

void InvalidationServiceAndroid::UpdateRegisteredInvalidationIds(
    syncer::InvalidationHandler* handler,
    const syncer::ObjectIdSet& ids) {
  DCHECK(CalledOnValidThread());
  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(env);

  invalidator_registrar_.UpdateRegisteredIds(handler, ids);
  const syncer::ObjectIdSet& registered_ids =
      invalidator_registrar_.GetAllRegisteredIds();

  // To call the corresponding method on the Java invalidation service, split
  // the object ids into object source and object name arrays.
  std::vector<int> sources;
  std::vector<std::string> names;
  syncer::ObjectIdSet::const_iterator id;
  for (id = registered_ids.begin(); id != registered_ids.end(); ++id) {
    sources.push_back(id->source());
    names.push_back(id->name());
  }

  Java_InvalidationService_setRegisteredObjectIds(
      env,
      java_ref_.obj(),
      base::android::ToJavaIntArray(env, sources).obj(),
      base::android::ToJavaArrayOfStrings(env, names).obj());

  logger_.OnUpdateIds(invalidator_registrar_.GetSanitizedHandlersIdsMap());
}

void InvalidationServiceAndroid::UnregisterInvalidationHandler(
    syncer::InvalidationHandler* handler) {
  DCHECK(CalledOnValidThread());
  invalidator_registrar_.UnregisterHandler(handler);
  logger_.OnUnregistration(handler->GetOwnerName());
}

syncer::InvalidatorState
InvalidationServiceAndroid::GetInvalidatorState() const {
  DCHECK(CalledOnValidThread());
  return invalidator_state_;
}

std::string InvalidationServiceAndroid::GetInvalidatorClientId() const {
  DCHECK(CalledOnValidThread());
  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(env);

  // Ask the Java code to for the invalidator ID it's currently using.
  base::android::ScopedJavaLocalRef<_jbyteArray*> id_bytes_java =
      Java_InvalidationService_getInvalidatorClientId(env, java_ref_.obj());

  // Convert it into a more convenient format for C++.
  std::vector<uint8_t> id_bytes;
  base::android::JavaByteArrayToByteVector(env, id_bytes_java.obj(), &id_bytes);
  std::string id(id_bytes.begin(), id_bytes.end());

  return id;
}

InvalidationLogger* InvalidationServiceAndroid::GetInvalidationLogger() {
  return &logger_;
}

void InvalidationServiceAndroid::RequestDetailedStatus(
    base::Callback<void(const base::DictionaryValue&)> return_callback) const {
}

IdentityProvider* InvalidationServiceAndroid::GetIdentityProvider() {
  return NULL;
}

void InvalidationServiceAndroid::TriggerStateChangeForTest(
    syncer::InvalidatorState state) {
  DCHECK(CalledOnValidThread());
  invalidator_state_ = state;
  invalidator_registrar_.UpdateInvalidatorState(invalidator_state_);
}

void InvalidationServiceAndroid::RequestSync(JNIEnv* env,
                                             jobject obj,
                                             jint object_source,
                                             jstring java_object_id,
                                             jlong version,
                                             jstring java_state) {
  invalidation::ObjectId object_id(object_source,
      ConvertJavaStringToUTF8(env, java_object_id));

  syncer::ObjectIdInvalidationMap object_ids_with_states;

  if (version == ipc::invalidation::Constants::UNKNOWN) {
    object_ids_with_states.Insert(
        syncer::Invalidation::InitUnknownVersion(object_id));
  } else {
    ObjectIdVersionMap::iterator it =
        max_invalidation_versions_.find(object_id);
    if ((it != max_invalidation_versions_.end()) &&
        (version <= it->second)) {
      DVLOG(1) << "Dropping redundant invalidation with version " << version;
      return;
    }
    max_invalidation_versions_[object_id] = version;
    object_ids_with_states.Insert(
        syncer::Invalidation::Init(object_id, version,
                                   ConvertJavaStringToUTF8(env, java_state)));
  }

  DispatchInvalidations(object_ids_with_states);
}

void InvalidationServiceAndroid::RequestSyncForAllTypes(JNIEnv* env,
                                                        jobject obj) {
  syncer::ObjectIdInvalidationMap object_ids_with_states;
  DispatchInvalidations(object_ids_with_states);
}

void InvalidationServiceAndroid::DispatchInvalidations(
    syncer::ObjectIdInvalidationMap& object_invalidation_map) {
  // An empty map implies that we should invalidate all.
  const syncer::ObjectIdInvalidationMap& effective_invalidation_map =
      object_invalidation_map.Empty() ?
      syncer::ObjectIdInvalidationMap::InvalidateAll(
          invalidator_registrar_.GetAllRegisteredIds()) :
      object_invalidation_map;

  invalidator_registrar_.DispatchInvalidationsToHandlers(
      effective_invalidation_map);
  logger_.OnInvalidation(effective_invalidation_map);
}

// static
bool InvalidationServiceAndroid::RegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace invalidation
