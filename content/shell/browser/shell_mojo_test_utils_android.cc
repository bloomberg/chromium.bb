// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_mojo_test_utils_android.h"

#include <utility>

#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "content/public/browser/android/service_registry_android.h"
#include "content/public/common/service_registry.h"
#include "jni/ShellMojoTestUtils_jni.h"

namespace {

struct TestEnvironment {
  base::MessageLoop message_loop;
  ScopedVector<content::ServiceRegistry> registries;
  ScopedVector<content::ServiceRegistryAndroid> wrappers;
};

}  // namespace

namespace content {

static jlong SetupTestEnvironment(JNIEnv* env,
                                  const JavaParamRef<jclass>& jcaller) {
  return reinterpret_cast<intptr_t>(new TestEnvironment());
}

static void TearDownTestEnvironment(JNIEnv* env,
                                    const JavaParamRef<jclass>& jcaller,
                                    jlong test_environment) {
  delete reinterpret_cast<TestEnvironment*>(test_environment);
}

static ScopedJavaLocalRef<jobject> CreateServiceRegistryPair(
    JNIEnv* env,
    const JavaParamRef<jclass>& jcaller,
    jlong native_test_environment) {
  TestEnvironment* test_environment =
      reinterpret_cast<TestEnvironment*>(native_test_environment);

  content::ServiceRegistry* registry_a = ServiceRegistry::Create();
  test_environment->registries.push_back(registry_a);
  content::ServiceRegistry* registry_b = ServiceRegistry::Create();
  test_environment->registries.push_back(registry_b);

  shell::mojom::InterfaceProviderPtr exposed_services_a;
  registry_a->Bind(GetProxy(&exposed_services_a));
  registry_b->BindRemoteServiceProvider(std::move(exposed_services_a));

  shell::mojom::InterfaceProviderPtr exposed_services_b;
  registry_b->Bind(GetProxy(&exposed_services_b));
  registry_a->BindRemoteServiceProvider(std::move(exposed_services_b));

  content::ServiceRegistryAndroid* wrapper_a =
      ServiceRegistryAndroid::Create(registry_a).release();
  test_environment->wrappers.push_back(wrapper_a);
  content::ServiceRegistryAndroid* wrapper_b =
      ServiceRegistryAndroid::Create(registry_b).release();
  test_environment->wrappers.push_back(wrapper_b);

  return Java_ShellMojoTestUtils_makePair(env, wrapper_a->GetObj().obj(),
                                          wrapper_b->GetObj().obj());
}

static void RunLoop(JNIEnv* env,
                    const JavaParamRef<jclass>& jcaller,
                    jlong timeout_ms) {
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure(),
      base::TimeDelta::FromMilliseconds(timeout_ms));
  base::RunLoop run_loop;
  run_loop.Run();
}

bool RegisterShellMojoTestUtils(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace content
