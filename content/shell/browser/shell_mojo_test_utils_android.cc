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
#include "content/public/browser/android/interface_provider_android.h"
#include "content/public/browser/android/interface_registry_android.h"
#include "jni/ShellMojoTestUtils_jni.h"
#include "services/shell/public/cpp/interface_provider.h"
#include "services/shell/public/cpp/interface_registry.h"

namespace {

struct TestEnvironment {
  base::MessageLoop message_loop;
  std::vector<std::unique_ptr<shell::InterfaceRegistry>> registries;
  std::vector<std::unique_ptr<shell::InterfaceProvider>> providers;
  std::vector<std::unique_ptr<content::InterfaceRegistryAndroid>>
      registry_wrappers;
  std::vector<std::unique_ptr<content::InterfaceProviderAndroid>>
      provider_wrappers;
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

static ScopedJavaLocalRef<jobject> CreateInterfaceRegistryAndProvider(
    JNIEnv* env,
    const JavaParamRef<jclass>& jcaller,
    jlong native_test_environment) {
  TestEnvironment* test_environment =
      reinterpret_cast<TestEnvironment*>(native_test_environment);

  std::unique_ptr<shell::InterfaceRegistry> registry(
      new shell::InterfaceRegistry(nullptr));
  std::unique_ptr<shell::InterfaceProvider> provider(
      new shell::InterfaceProvider);

  shell::mojom::InterfaceProviderPtr provider_proxy;
  shell::mojom::InterfaceProviderRequest provider_request =
      mojo::GetProxy(&provider_proxy);
  provider->Bind(std::move(provider_proxy));
  registry->Bind(std::move(provider_request));

  std::unique_ptr<content::InterfaceRegistryAndroid> registry_android(
      InterfaceRegistryAndroid::Create(registry.get()));
  std::unique_ptr<content::InterfaceProviderAndroid> provider_android(
      InterfaceProviderAndroid::Create(provider.get()));

  ScopedJavaLocalRef<jobject> obj = Java_ShellMojoTestUtils_makePair(
      env, registry_android->GetObj().obj(), provider_android->GetObj().obj());

  test_environment->registry_wrappers.push_back(std::move(registry_android));
  test_environment->provider_wrappers.push_back(std::move(provider_android));
  test_environment->registries.push_back(std::move(registry));
  test_environment->providers.push_back(std::move(provider));

  return obj;
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
