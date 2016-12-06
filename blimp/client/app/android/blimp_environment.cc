// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/app/android/blimp_environment.h"

#include <memory>

#include "base/android/apk_assets.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/memory_mapped_file.h"
#include "base/threading/thread.h"
#include "blimp/client/app/android/blimp_client_context_delegate_android.h"
#include "blimp/client/app/blimp_startup.h"
#include "blimp/client/public/blimp_client_context.h"
#include "blimp/client/support/compositor/compositor_dependencies_impl.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/command_line_pref_store.h"
#include "components/prefs/in_memory_pref_store.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "jni/BlimpEnvironment_jni.h"
#include "ui/base/resource/resource_bundle.h"

namespace blimp {
namespace client {
namespace {

class BlimpShellCommandLinePrefStore : public CommandLinePrefStore {
 public:
  explicit BlimpShellCommandLinePrefStore(const base::CommandLine* command_line)
      : CommandLinePrefStore(command_line) {
    BlimpClientContext::ApplyBlimpSwitches(this);
  }

 protected:
  ~BlimpShellCommandLinePrefStore() override = default;
};

void InitializeResourceBundle() {
  base::MemoryMappedFile::Region pak_region;
  int pak_fd =
      base::android::OpenApkAsset("assets/blimp_shell.pak", &pak_region);
  DCHECK_GE(pak_fd, 0);
  ui::ResourceBundle::InitSharedInstanceWithPakFileRegion(base::File(pak_fd),
                                                          pak_region);
}

}  // namespace

// DelegatingCompositorDependencies delegates all calls to the public API
// to its delegate. When it is destructed it informs the BlimpEnvironment that
// it is going away to ensure that the BlimpEnvironment can know how many
// DelegatingCompositorDependencies are outstanding.
class DelegatingCompositorDependencies : public CompositorDependencies {
 public:
  DelegatingCompositorDependencies(
      CompositorDependencies* delegate_compositor_dependencies,
      BlimpEnvironment* blimp_environment)
      : delegate_compositor_dependencies_(delegate_compositor_dependencies),
        blimp_environment_(blimp_environment) {}

  ~DelegatingCompositorDependencies() override {
    blimp_environment_->DecrementOutstandingCompositorDependencies();
  }

  gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() override {
    return delegate_compositor_dependencies_->GetGpuMemoryBufferManager();
  }

  cc::SurfaceManager* GetSurfaceManager() override {
    return delegate_compositor_dependencies_->GetSurfaceManager();
  }

  cc::FrameSinkId AllocateFrameSinkId() override {
    return delegate_compositor_dependencies_->AllocateFrameSinkId();
  }

  void GetContextProviders(const ContextProviderCallback& callback) override {
    delegate_compositor_dependencies_->GetContextProviders(callback);
  }

 private:
  CompositorDependencies* delegate_compositor_dependencies_;
  BlimpEnvironment* blimp_environment_;

  DISALLOW_COPY_AND_ASSIGN(DelegatingCompositorDependencies);
};

BlimpEnvironment::BlimpEnvironment() {
  InitializeResourceBundle();

  io_thread_ = base::MakeUnique<base::Thread>("BlimpIOThread");
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  io_thread_->StartWithOptions(options);

  // Create PrefRegistry and register blimp preferences with it.
  scoped_refptr<user_prefs::PrefRegistrySyncable> pref_registry =
      new ::user_prefs::PrefRegistrySyncable();
  BlimpClientContext::RegisterPrefs(pref_registry.get());

  // Create command line and user preference stores.
  PrefServiceFactory pref_service_factory;
  pref_service_factory.set_command_line_prefs(
      make_scoped_refptr(new BlimpShellCommandLinePrefStore(
          base::CommandLine::ForCurrentProcess())));
  pref_service_factory.set_user_prefs(new InMemoryPrefStore());

  // Create a PrefService binding the PrefRegistry to the pref stores.
  // The PrefService owns the PrefRegistry and pref stores.
  std::unique_ptr<PrefService> pref_service =
      pref_service_factory.Create(pref_registry.get());

  // Create the real CompositorDependencies. This is used for minting
  // CompositorDependencies to callers of CreateCompositorDepencencies().
  compositor_dependencies_ = base::MakeUnique<CompositorDependenciesImpl>();

  context_ = base::WrapUnique<BlimpClientContext>(BlimpClientContext::Create(
      io_thread_->task_runner(), io_thread_->task_runner(),
      CreateCompositorDepencencies(), pref_service.get()));

  context_delegate_ =
      base::MakeUnique<BlimpClientContextDelegateAndroid>(context_.get());
}

BlimpEnvironment::~BlimpEnvironment() {
  DCHECK_EQ(0, outstanding_compositor_dependencies_);
  context_.reset();
  compositor_dependencies_.reset();
}

void BlimpEnvironment::Destroy(JNIEnv*,
                               const base::android::JavaParamRef<jobject>&) {
  delete this;
}

// static
BlimpEnvironment* BlimpEnvironment::FromJavaObject(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jobj) {
  return reinterpret_cast<BlimpEnvironment*>(
      Java_BlimpEnvironment_getNativePtr(env, jobj));
}

base::android::ScopedJavaLocalRef<jobject>
BlimpEnvironment::GetBlimpClientContext(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobj) {
  DCHECK(context_);
  return BlimpClientContext::GetJavaObject(context_.get());
}

std::unique_ptr<CompositorDependencies>
BlimpEnvironment::CreateCompositorDepencencies() {
  outstanding_compositor_dependencies_++;
  return base::MakeUnique<DelegatingCompositorDependencies>(
      compositor_dependencies_.get(), this);
}

void BlimpEnvironment::DecrementOutstandingCompositorDependencies() {
  outstanding_compositor_dependencies_--;
  DCHECK_GE(0, outstanding_compositor_dependencies_);
}

static jlong Init(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& obj) {
  BlimpEnvironment* blimp_environment = new BlimpEnvironment(/*env, obj*/);
  return reinterpret_cast<intptr_t>(blimp_environment);
}

// static
bool BlimpEnvironment::RegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace client
}  // namespace blimp
