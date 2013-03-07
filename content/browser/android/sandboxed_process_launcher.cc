// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/sandboxed_process_launcher.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/android/media_player_manager_android.h"
#include "content/browser/renderer_host/compositor_impl_android.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/common/android/scoped_java_surface.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "jni/SandboxedProcessLauncher_jni.h"
#include "media/base/android/media_player_bridge.h"

using base::android::AttachCurrentThread;
using base::android::ToJavaArrayOfStrings;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using content::StartSandboxedProcessCallback;

namespace content {

namespace {

// Pass a java surface object to the MediaPlayerBridge object
// identified by render process handle, render view ID and player ID.
static void SetSurfacePeer(
    const base::android::JavaRef<jobject>& surface,
    base::ProcessHandle render_process_handle,
    int render_view_id,
    int player_id) {
  int renderer_id = 0;
  RenderProcessHost::iterator it = RenderProcessHost::AllHostsIterator();
  while (!it.IsAtEnd()) {
    if (it.GetCurrentValue()->GetHandle() == render_process_handle) {
      renderer_id = it.GetCurrentValue()->GetID();
      break;
    }
    it.Advance();
  }

  if (renderer_id) {
    RenderViewHostImpl* host = RenderViewHostImpl::FromID(
        renderer_id, render_view_id);
    if (host) {
      media::MediaPlayerBridge* player =
          host->media_player_manager()->GetPlayer(player_id);
      if (player &&
          player != host->media_player_manager()->GetFullscreenPlayer()) {
        ScopedJavaSurface scoped_surface(surface);
        player->SetVideoSurface(scoped_surface.j_surface().obj());
      }
    }
  }
}

}  // anonymous namespace

// Called from SandboxedProcessLauncher.java when the SandboxedProcess was
// started.
// |client_context| is the pointer to StartSandboxedProcessCallback which was
// passed in from StartSandboxedProcess.
// |handle| is the processID of the child process as originated in Java, 0 if
// the SandboxedProcess could not be created.
static void OnSandboxedProcessStarted(JNIEnv*,
                                      jclass,
                                      jint client_context,
                                      jint handle) {
  StartSandboxedProcessCallback* callback =
      reinterpret_cast<StartSandboxedProcessCallback*>(client_context);
  if (handle)
    callback->Run(static_cast<base::ProcessHandle>(handle));
  delete callback;
}

void StartSandboxedProcess(
    const CommandLine::StringVector& argv,
    const std::vector<content::FileDescriptorInfo>& files_to_register,
    const StartSandboxedProcessCallback& callback) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(env);

  // Create the Command line String[]
  ScopedJavaLocalRef<jobjectArray> j_argv = ToJavaArrayOfStrings(env, argv);

  size_t file_count = files_to_register.size();
  DCHECK(file_count > 0);

  ScopedJavaLocalRef<jintArray> j_file_ids(env, env->NewIntArray(file_count));
  base::android::CheckException(env);
  jint* file_ids = env->GetIntArrayElements(j_file_ids.obj(), NULL);
  base::android::CheckException(env);
  ScopedJavaLocalRef<jintArray> j_file_fds(env, env->NewIntArray(file_count));
  base::android::CheckException(env);
  jint* file_fds = env->GetIntArrayElements(j_file_fds.obj(), NULL);
  base::android::CheckException(env);
  ScopedJavaLocalRef<jbooleanArray> j_file_auto_close(
      env, env->NewBooleanArray(file_count));
  base::android::CheckException(env);
  jboolean* file_auto_close =
      env->GetBooleanArrayElements(j_file_auto_close.obj(), NULL);
  base::android::CheckException(env);
  for (size_t i = 0; i < file_count; ++i) {
    const content::FileDescriptorInfo& fd_info = files_to_register[i];
    file_ids[i] = fd_info.id;
    file_fds[i] = fd_info.fd.fd;
    file_auto_close[i] = fd_info.fd.auto_close;
  }
  env->ReleaseIntArrayElements(j_file_ids.obj(), file_ids, 0);
  env->ReleaseIntArrayElements(j_file_fds.obj(), file_fds, 0);
  env->ReleaseBooleanArrayElements(j_file_auto_close.obj(), file_auto_close, 0);

  Java_SandboxedProcessLauncher_start(env,
      base::android::GetApplicationContext(),
      j_argv.obj(),
      j_file_ids.obj(),
      j_file_fds.obj(),
      j_file_auto_close.obj(),
      reinterpret_cast<jint>(new StartSandboxedProcessCallback(callback)));
}

void StopSandboxedProcess(base::ProcessHandle handle) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(env);
  Java_SandboxedProcessLauncher_stop(env, static_cast<jint>(handle));
}

void EstablishSurfacePeer(
    JNIEnv* env, jclass clazz,
    jint pid, jobject surface, jint primary_id, jint secondary_id) {
  ScopedJavaGlobalRef<jobject> jsurface;
  jsurface.Reset(env, surface);
  if (jsurface.is_null())
    return;

  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(
      &SetSurfacePeer, jsurface, pid, primary_id, secondary_id));
}

jobject GetViewSurface(JNIEnv* env, jclass clazz, jint surface_id) {
  // This is a synchronous call from the GPU process and is expected to be
  // handled on a binder thread. Handling this on the UI thread will lead
  // to deadlocks.
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
  return CompositorImpl::GetSurface(surface_id);
}

bool RegisterSandboxedProcessLauncher(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace content
