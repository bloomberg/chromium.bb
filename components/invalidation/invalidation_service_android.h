// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_INVALIDATION_SERVICE_ANDROID_H_
#define COMPONENTS_INVALIDATION_INVALIDATION_SERVICE_ANDROID_H_

#include <jni.h>
#include <map>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "components/invalidation/invalidation_logger.h"
#include "components/invalidation/invalidation_service.h"
#include "components/invalidation/invalidator_registrar.h"
#include "components/keyed_service/core/keyed_service.h"

namespace invalidation {

class InvalidationLogger;

// This InvalidationService is used to deliver invalidations on Android.  The
// Android operating system has its own mechanisms for delivering invalidations.
class InvalidationServiceAndroid
    : public base::NonThreadSafe,
      public InvalidationService {
 public:
  InvalidationServiceAndroid(jobject context);
  ~InvalidationServiceAndroid() override;

  // InvalidationService implementation.
  //
  // Note that this implementation does not properly support Ack-tracking,
  // fetching the invalidator state, or querying the client's ID.  Support for
  // exposing the client ID should be available soon; see crbug.com/172391.
  void RegisterInvalidationHandler(
      syncer::InvalidationHandler* handler) override;
  void UpdateRegisteredInvalidationIds(syncer::InvalidationHandler* handler,
                                       const syncer::ObjectIdSet& ids) override;
  void UnregisterInvalidationHandler(
      syncer::InvalidationHandler* handler) override;
  syncer::InvalidatorState GetInvalidatorState() const override;
  std::string GetInvalidatorClientId() const override;
  InvalidationLogger* GetInvalidationLogger() override;
  void RequestDetailedStatus(
      base::Callback<void(const base::DictionaryValue&)> caller) const override;
  IdentityProvider* GetIdentityProvider() override;

  void RequestSync(JNIEnv* env,
                   jobject obj,
                   jint object_source,
                   jstring object_id,
                   jlong version,
                   jstring state);

  void RequestSyncForAllTypes(JNIEnv* env, jobject obj);

  // The InvalidationServiceAndroid always reports that it is enabled.
  // This is used only by unit tests.
  void TriggerStateChangeForTest(syncer::InvalidatorState state);

  static bool RegisterJni(JNIEnv* env);

 private:
  typedef std::map<invalidation::ObjectId,
                   int64,
                   syncer::ObjectIdLessThan> ObjectIdVersionMap;

  // Friend class so that InvalidationServiceFactoryAndroid has access to
  // private member object java_ref_.
  friend class InvalidationServiceFactoryAndroid;

  // Points to a Java instance of InvalidationService.
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;

  syncer::InvalidatorRegistrar invalidator_registrar_;
  syncer::InvalidatorState invalidator_state_;

  // The invalidation API spec allows for the possibility of redundant
  // invalidations, so keep track of the max versions and drop
  // invalidations with old versions.
  ObjectIdVersionMap max_invalidation_versions_;

  // The invalidation logger object we use to record state changes
  // and invalidations.
  InvalidationLogger logger_;

  void DispatchInvalidations(
      syncer::ObjectIdInvalidationMap& object_invalidation_map);

  DISALLOW_COPY_AND_ASSIGN(InvalidationServiceAndroid);
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_INVALIDATION_SERVICE_ANDROID_H_
