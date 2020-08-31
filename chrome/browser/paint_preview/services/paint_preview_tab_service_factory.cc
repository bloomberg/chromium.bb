// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/paint_preview/services/paint_preview_tab_service_factory.h"

#include "build/build_config.h"
#include "chrome/browser/paint_preview/services/paint_preview_tab_service.h"
#include "components/keyed_service/core/simple_dependency_manager.h"
#include "components/keyed_service/core/simple_factory_key.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/android/chrome_jni_headers/PaintPreviewTabServiceFactory_jni.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/profiles/profile_manager.h"
#endif  // defined(OS_ANDROID)

namespace paint_preview {

namespace {

constexpr char kFeatureDirname[] = "tab_service";

}  // namespace

// static
PaintPreviewTabServiceFactory* PaintPreviewTabServiceFactory::GetInstance() {
  return base::Singleton<PaintPreviewTabServiceFactory>::get();
}

// static
paint_preview::PaintPreviewTabService*
PaintPreviewTabServiceFactory::GetServiceInstance(SimpleFactoryKey* key) {
  return static_cast<paint_preview::PaintPreviewTabService*>(
      GetInstance()->GetServiceForKey(key, true));
}

PaintPreviewTabServiceFactory::PaintPreviewTabServiceFactory()
    : SimpleKeyedServiceFactory("PaintPreviewTabService",
                                SimpleDependencyManager::GetInstance()) {}

PaintPreviewTabServiceFactory::~PaintPreviewTabServiceFactory() = default;

std::unique_ptr<KeyedService>
PaintPreviewTabServiceFactory::BuildServiceInstanceFor(
    SimpleFactoryKey* key) const {
  // Prevent this working off the record.
  if (key->IsOffTheRecord())
    return nullptr;

  // TODO(crbug/1060556): Inject a useful policy.
  return std::make_unique<paint_preview::PaintPreviewTabService>(
      key->GetPath(), kFeatureDirname, nullptr, key->IsOffTheRecord());
}

SimpleFactoryKey* PaintPreviewTabServiceFactory::GetKeyToUse(
    SimpleFactoryKey* key) const {
  return key;
}

#if defined(OS_ANDROID)
base::android::ScopedJavaLocalRef<jobject>
JNI_PaintPreviewTabServiceFactory_GetServiceInstanceForCurrentProfile(
    JNIEnv* env) {
  ProfileKey* profile_key =
      ProfileManager::GetLastUsedProfile()->GetProfileKey();
  base::android::ScopedJavaGlobalRef<jobject> java_ref =
      PaintPreviewTabServiceFactory::GetServiceInstance(profile_key)
          ->GetJavaRef();
  return base::android::ScopedJavaLocalRef<jobject>(java_ref);
}
#endif  // defined(OS_ANDROID)

}  // namespace paint_preview
