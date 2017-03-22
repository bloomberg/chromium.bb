// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/child_process_launcher_helper_android.h"

#include <memory>

#include "base/android/apk_assets.h"
#include "base/android/context_utils.h"
#include "base/android/jni_array.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "content/browser/child_process_launcher_helper.h"
#include "content/browser/child_process_launcher_helper_posix.h"
#include "content/browser/file_descriptor_info_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_descriptors.h"
#include "content/public/common/content_switches.h"
#include "gin/v8_initializer.h"
#include "jni/ChildProcessLauncher_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaArrayOfStrings;

namespace content {

typedef base::Callback<void(base::ProcessHandle, int /* launch result */)>
    StartChildProcessCallback;

namespace internal {

namespace {

// Starts a process as a child process spawned by the Android ActivityManager.
// The created process handle is returned to the |callback| on success, 0 is
// returned if the process could not be created.
void StartChildProcess(const base::CommandLine::StringVector& argv,
                       int child_process_id,
                       content::FileDescriptorInfo* files_to_register,
                       const StartChildProcessCallback& callback) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(env);

  // Create the Command line String[]
  ScopedJavaLocalRef<jobjectArray> j_argv = ToJavaArrayOfStrings(env, argv);

  size_t file_count = files_to_register->GetMappingSize();
  DCHECK(file_count > 0);

  ScopedJavaLocalRef<jclass> j_file_info_class = base::android::GetClass(
      env, "org/chromium/base/process_launcher/FileDescriptorInfo");
  ScopedJavaLocalRef<jobjectArray> j_file_infos(
      env, env->NewObjectArray(file_count, j_file_info_class.obj(), NULL));
  base::android::CheckException(env);

  for (size_t i = 0; i < file_count; ++i) {
    int fd = files_to_register->GetFDAt(i);
    PCHECK(0 <= fd);
    int id = files_to_register->GetIDAt(i);
    const auto& region = files_to_register->GetRegionAt(i);
    bool auto_close = files_to_register->OwnsFD(fd);
    ScopedJavaLocalRef<jobject> j_file_info =
        Java_ChildProcessLauncher_makeFdInfo(env, id, fd, auto_close,
                                             region.offset, region.size);
    PCHECK(j_file_info.obj());
    env->SetObjectArrayElement(j_file_infos.obj(), i, j_file_info.obj());
    if (auto_close) {
      ignore_result(files_to_register->ReleaseFD(fd).release());
    }
  }

  constexpr int param_key = 0;  // TODO(boliu): Use this.
  Java_ChildProcessLauncher_start(
      env, base::android::GetApplicationContext(), param_key, j_argv,
      child_process_id, j_file_infos,
      reinterpret_cast<intptr_t>(new StartChildProcessCallback(callback)));
}

// Stops a child process based on the handle returned from StartChildProcess.
void StopChildProcess(base::ProcessHandle handle) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(env);
  Java_ChildProcessLauncher_stop(env, static_cast<jint>(handle));
}

bool IsChildProcessOomProtected(base::ProcessHandle handle) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(env);
  return Java_ChildProcessLauncher_isOomProtected(env,
                                                  static_cast<jint>(handle));
}

void SetChildProcessInForeground(base::ProcessHandle handle,
                                 bool in_foreground) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(env);
  return Java_ChildProcessLauncher_setInForeground(
      env, static_cast<jint>(handle), static_cast<jboolean>(in_foreground));
}

// Callback invoked from Java once the process has been started.
void ChildProcessStartedCallback(
    ChildProcessLauncherHelper* helper,
    base::ProcessHandle handle,
    int launch_result) {
  // TODO(jcivelli): Remove this by defining better what happens on what thread
  // in the corresponding Java code.
  ChildProcessLauncherHelper::Process process;
  process.process = base::Process(handle);
  if (BrowserThread::CurrentlyOn(BrowserThread::PROCESS_LAUNCHER)) {
    helper->PostLaunchOnLauncherThread(
        std::move(process),
        launch_result,
        false);  // post_launch_on_client_thread_called
    return;
  }

  bool on_client_thread = BrowserThread::CurrentlyOn(
      static_cast<BrowserThread::ID>(helper->client_thread_id()));
  BrowserThread::PostTask(BrowserThread::PROCESS_LAUNCHER, FROM_HERE,
      base::Bind(&ChildProcessLauncherHelper::PostLaunchOnLauncherThread,
                 helper,
                 base::Passed(std::move(process)),
                 launch_result,
                 on_client_thread));
  if (on_client_thread) {
    ChildProcessLauncherHelper::Process process;
    process.process = base::Process(handle);
    helper->PostLaunchOnClientThread(std::move(process), launch_result);
  }
}

}  // namespace

void ChildProcessLauncherHelper::BeforeLaunchOnClientThread() {
  // Android only supports renderer, sandboxed utility and gpu.
  std::string process_type =
      command_line()->GetSwitchValueASCII(switches::kProcessType);
  CHECK(process_type == switches::kGpuProcess ||
        process_type == switches::kRendererProcess ||
#if BUILDFLAG(ENABLE_PLUGINS)
        process_type == switches::kPpapiPluginProcess ||
#endif
        process_type == switches::kUtilityProcess)
      << "Unsupported process type: " << process_type;

  // Non-sandboxed utility or renderer process are currently not supported.
  DCHECK(process_type == switches::kGpuProcess ||
         !command_line()->HasSwitch(switches::kNoSandbox));
}

mojo::edk::ScopedPlatformHandle
ChildProcessLauncherHelper::PrepareMojoPipeHandlesOnClientThread() {
  return mojo::edk::ScopedPlatformHandle();
}

std::unique_ptr<FileDescriptorInfo>
ChildProcessLauncherHelper::GetFilesToMap() {
  DCHECK_CURRENTLY_ON(BrowserThread::PROCESS_LAUNCHER);

  // Android WebView runs in single process, ensure that we never get here when
  // running in single process mode.
  CHECK(!command_line()->HasSwitch(switches::kSingleProcess));

  std::unique_ptr<FileDescriptorInfo> files_to_register =
      CreateDefaultPosixFilesToMap(child_process_id(), mojo_client_handle(),
                                   true /* include_service_required_files */,
                                   GetProcessType(), command_line());

#if ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_FILE
  base::MemoryMappedFile::Region icu_region;
  int fd = base::i18n::GetIcuDataFileHandle(&icu_region);
  files_to_register->ShareWithRegion(kAndroidICUDataDescriptor, fd, icu_region);
#endif  // ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_FILE

  return files_to_register;
}

void ChildProcessLauncherHelper::BeforeLaunchOnLauncherThread(
    const FileDescriptorInfo& files_to_register,
    base::LaunchOptions* options) {
}

ChildProcessLauncherHelper::Process
ChildProcessLauncherHelper::LaunchProcessOnLauncherThread(
    const base::LaunchOptions& options,
    std::unique_ptr<FileDescriptorInfo> files_to_register,
    bool* is_synchronous_launch,
    int* launch_result) {
  *is_synchronous_launch = false;

  StartChildProcess(command_line()->argv(),
                    child_process_id(),
                    files_to_register.get(),
                    base::Bind(&ChildProcessStartedCallback,
                               RetainedRef(this)));

  return Process();
}

void ChildProcessLauncherHelper::AfterLaunchOnLauncherThread(
    const ChildProcessLauncherHelper::Process& process,
    const base::LaunchOptions& options) {
}

// static
base::TerminationStatus ChildProcessLauncherHelper::GetTerminationStatus(
    const ChildProcessLauncherHelper::Process& process,
    bool known_dead,
    int* exit_code) {
  if (IsChildProcessOomProtected(process.process.Handle()))
    return base::TERMINATION_STATUS_OOM_PROTECTED;
  return base::GetTerminationStatus(process.process.Handle(), exit_code);
}

// static
bool ChildProcessLauncherHelper::TerminateProcess(
    const base::Process& process, int exit_code, bool wait) {
  StopChildProcess(process.Handle());
  return true;
}

// static
void ChildProcessLauncherHelper::ForceNormalProcessTerminationSync(
    ChildProcessLauncherHelper::Process process) {
  DCHECK_CURRENTLY_ON(BrowserThread::PROCESS_LAUNCHER);
  VLOG(1) << "ChromeProcess: Stopping process with handle "
          << process.process.Handle();
  StopChildProcess(process.process.Handle());
}

// static
void ChildProcessLauncherHelper::SetProcessBackgroundedOnLauncherThread(
      base::Process process, bool background) {
  SetChildProcessInForeground(process.Handle(), !background);
}

// static
void ChildProcessLauncherHelper::SetRegisteredFilesForService(
    const std::string& service_name,
    catalog::RequiredFileMap required_files) {
  SetFilesToShareForServicePosix(service_name, std::move(required_files));
}

// static
void ChildProcessLauncherHelper::ResetRegisteredFilesForTesting() {
  ResetFilesToShareForTestingPosix();
}

// static
base::File OpenFileToShare(const base::FilePath& path,
                           base::MemoryMappedFile::Region* region) {
  return base::File(base::android::OpenApkAsset(path.value(), region));
}

}  // namespace internal

// Called from ChildProcessLauncher.java when the ChildProcess was
// started.
// |client_context| is the pointer to StartChildProcessCallback which was
// passed in from StartChildProcess.
// |handle| is the processID of the child process as originated in Java, 0 if
// the ChildProcess could not be created.
static void OnChildProcessStarted(JNIEnv*,
                                  const JavaParamRef<jclass>&,
                                  jlong client_context,
                                  jint handle) {
  StartChildProcessCallback* callback =
      reinterpret_cast<StartChildProcessCallback*>(client_context);
  int launch_result = (handle == base::kNullProcessHandle)
                          ? LAUNCH_RESULT_FAILURE
                          : LAUNCH_RESULT_SUCCESS;
  callback->Run(static_cast<base::ProcessHandle>(handle), launch_result);
  delete callback;
}

jboolean IsSingleProcess(JNIEnv* env, const JavaParamRef<jclass>& clazz) {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kSingleProcess);
}

bool RegisterChildProcessLauncher(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace content
