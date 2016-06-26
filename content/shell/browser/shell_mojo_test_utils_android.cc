// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_mojo_test_utils_android.h"

#include <utility>

#include "base/location.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/browser/android/service_registry_android.h"
#include "jni/ShellMojoTestUtils_jni.h"
#include "services/shell/public/cpp/interface_provider.h"
#include "services/shell/public/cpp/interface_registry.h"

namespace {

struct TestEnvironment {
  base::MessageLoop message_loop;
  std::vector<std::unique_ptr<shell::InterfaceRegistry>> registries;
  std::vector<std::unique_ptr<shell::InterfaceProvider>> providers;
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

  std::unique_ptr<shell::InterfaceRegistry> registry_a(
      new shell::InterfaceRegistry(nullptr));
  std::unique_ptr<shell::InterfaceRegistry> registry_b(
      new shell::InterfaceRegistry(nullptr));
  std::unique_ptr<shell::InterfaceProvider> provider_a(
      new shell::InterfaceProvider);
  std::unique_ptr<shell::InterfaceProvider> provider_b(
      new shell::InterfaceProvider);

  shell::mojom::InterfaceProviderPtr a_to_b;
  shell::mojom::InterfaceProviderRequest a_to_b_request =
      mojo::GetProxy(&a_to_b);
  provider_a->Bind(std::move(a_to_b));
  registry_b->Bind(std::move(a_to_b_request));

  shell::mojom::InterfaceProviderPtr b_to_a;
  shell::mojom::InterfaceProviderRequest b_to_a_request =
      mojo::GetProxy(&b_to_a);
  provider_b->Bind(std::move(b_to_a));
  registry_a->Bind(std::move(b_to_a_request));

  content::ServiceRegistryAndroid* wrapper_a =
      ServiceRegistryAndroid::Create(registry_a.get(),
                                     provider_a.get()).release();
  test_environment->wrappers.push_back(wrapper_a);
  content::ServiceRegistryAndroid* wrapper_b =
      ServiceRegistryAndroid::Create(registry_b.get(),
                                     provider_b.get()).release();
  test_environment->wrappers.push_back(wrapper_b);

  test_environment->registries.push_back(std::move(registry_a));
  test_environment->providers.push_back(std::move(provider_a));
  test_environment->registries.push_back(std::move(registry_b));
  test_environment->providers.push_back(std::move(provider_b));

  return Java_ShellMojoTestUtils_makePair(env, wrapper_a->GetObj().obj(),
                                          wrapper_b->GetObj().obj());
}

static void RunLoop(JNIEnv* env,
                    const JavaParamRef<jclass>& jcaller,
                    jlong timeout_ms) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure(),
      base::TimeDelta::FromMilliseconds(timeout_ms));
  base::RunLoop run_loop;
  run_loop.Run();
}

bool RegisterShellMojoTestUtils(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace content
