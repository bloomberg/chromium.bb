// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/java/jni_helper.h"

#include <map>

#include "base/android/jni_android.h"
#include "base/lazy_instance.h"
#include "base/threading/platform_thread.h"

namespace content {

namespace {

struct MethodIdentifier {
  const char* class_name;
  const char* method;
  const char* jni_signature;

  bool operator<(const MethodIdentifier& other) const {
    int r = strcmp(class_name, other.class_name);
    if (r < 0) {
      return true;
    } else if (r > 0) {
      return false;
    }

    r = strcmp(method, other.method);
    if (r < 0) {
      return true;
    } else if (r > 0) {
      return false;
    }

    return strcmp(jni_signature, other.jni_signature) < 0;
  }
};

typedef std::map<MethodIdentifier, jmethodID> MethodIDMap;

const base::subtle::AtomicWord kUnlocked = 0;
const base::subtle::AtomicWord kLocked = 1;
base::subtle::AtomicWord g_method_id_map_lock = kUnlocked;

base::LazyInstance<MethodIDMap> g_method_id_map = LAZY_INSTANCE_INITIALIZER;

}  // namespace

jmethodID GetMethodIDFromClassName(JNIEnv* env,
                                   const char* class_name,
                                   const char* method,
                                   const char* jni_signature) {
  MethodIdentifier key;
  key.class_name = class_name;
  key.method = method;
  key.jni_signature = jni_signature;

  MethodIDMap* map = g_method_id_map.Pointer();
  bool found = false;

  while (base::subtle::Acquire_CompareAndSwap(&g_method_id_map_lock,
                                              kUnlocked,
                                              kLocked) != kUnlocked) {
    base::PlatformThread::YieldCurrentThread();
  }
  MethodIDMap::const_iterator iter = map->find(key);
  if (iter != map->end()) {
    found = true;
  }
  base::subtle::Release_Store(&g_method_id_map_lock, kUnlocked);

  // Addition to the map does not invalidate this iterator.
  if (found) {
    return iter->second;
  }

  base::android::ScopedJavaLocalRef<jclass> clazz(
      env, env->FindClass(class_name));
  jmethodID id = base::android::MethodID::Get<
      base::android::MethodID::TYPE_INSTANCE>(
          env, clazz.obj(), method, jni_signature);

  while (base::subtle::Acquire_CompareAndSwap(&g_method_id_map_lock,
                                              kUnlocked,
                                              kLocked) != kUnlocked) {
    base::PlatformThread::YieldCurrentThread();
  }
  // Another thread may have populated the map already.
  std::pair<MethodIDMap::const_iterator, bool> result =
      map->insert(std::make_pair(key, id));
  DCHECK_EQ(id, result.first->second);
  base::subtle::Release_Store(&g_method_id_map_lock, kUnlocked);

  return id;
}

}  // namespace content
