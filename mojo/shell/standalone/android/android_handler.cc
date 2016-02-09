// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/standalone/android/android_handler.h"

#include <stddef.h>
#include <utility>

#include "base/android/context_utils.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/scoped_native_library.h"
#include "jni/AndroidHandler_jni.h"
#include "mojo/common/data_pipe_utils.h"
#include "mojo/public/c/system/main.h"
#include "mojo/shell/public/cpp/shell.h"
#include "mojo/shell/runner/host/native_application_support.h"
#include "mojo/shell/standalone/android/run_android_application_function.h"
#include "mojo/util/filename_util.h"
#include "url/gurl.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::GetApplicationContext;

namespace mojo {
namespace shell {

namespace {

// This function loads the application library, sets the application context and
// thunks and calls into the application MojoMain. To ensure that the thunks are
// set correctly we keep it in the Mojo shell .so and pass the function pointer
// to the helper libbootstrap.so.
void RunAndroidApplication(JNIEnv* env,
                           jobject j_context,
                           const base::FilePath& app_path,
                           jint j_handle) {
  InterfaceRequest<mojom::ShellClient> request =
      MakeRequest<mojom::ShellClient>(
          MakeScopedHandle(MessagePipeHandle(j_handle)));

  // Load the library, so that we can set the application context there if
  // needed.
  // TODO(vtl): We'd use a ScopedNativeLibrary, but it doesn't have .get()!
  base::NativeLibrary app_library = LoadNativeApplication(app_path);
  if (!app_library)
    return;

  // Set the application context if needed. Most applications will need to
  // access the Android ApplicationContext in which they are run. If the
  // application library exports the InitApplicationContext function, we will
  // set it there.
  const char* init_application_context_name = "InitApplicationContext";
  typedef void (*InitApplicationContextFn)(
      const base::android::JavaRef<jobject>&);
  InitApplicationContextFn init_application_context =
      reinterpret_cast<InitApplicationContextFn>(
          base::GetFunctionPointerFromNativeLibrary(
              app_library, init_application_context_name));
  if (init_application_context) {
    base::android::ScopedJavaLocalRef<jobject> scoped_context(env, j_context);
    init_application_context(scoped_context);
  }

  // Run the application.
  RunNativeApplication(app_library, std::move(request));
  // TODO(vtl): See note about unloading and thread-local destructors above
  // declaration of |LoadNativeApplication()|.
  base::UnloadNativeLibrary(app_library);
}

// Returns true if |url| denotes a cached app. If true |app_dir| is set to the
// path of the directory for the app and |path_to_mojo| the path of the app's
// .mojo file.
bool IsCachedApp(JNIEnv* env,
                 const GURL& url,
                 base::FilePath* app_dir,
                 base::FilePath* path_to_mojo) {
  ScopedJavaLocalRef<jstring> j_cached_apps_dir =
      Java_AndroidHandler_getCachedAppsDir(env, GetApplicationContext());
  const base::FilePath cached_apps_fp(
      ConvertJavaStringToUTF8(env, j_cached_apps_dir.obj()));
  const std::string cached_apps(util::FilePathToFileURL(cached_apps_fp).spec());
  const std::string response_url(GURL(url).spec());
  if (response_url.size() <= cached_apps.size() ||
      cached_apps.compare(0u, cached_apps.size(), response_url, 0u,
                          cached_apps.size()) != 0) {
    return false;
  }

  const std::string mojo_suffix(".mojo");
  // app_rel_path is either something like html_viewer/html_viewer.mojo, or
  // html_viewer.mojo, depending upon whether the app has a package.
  const std::string app_rel_path(response_url.substr(cached_apps.size() + 1));
  const size_t slash_index = app_rel_path.find('/');
  if (slash_index != std::string::npos) {
    const std::string tail =
        app_rel_path.substr(slash_index + 1, std::string::npos);
    const std::string head = app_rel_path.substr(0, slash_index);
    if (head.find('/') != std::string::npos ||
        tail.size() <= mojo_suffix.size() ||
        tail.compare(tail.size() - mojo_suffix.size(), tail.size(),
                     mojo_suffix) != 0) {
      return false;
    }
    *app_dir = cached_apps_fp.Append(head);
    *path_to_mojo = app_dir->Append(tail);
    return true;
  }
  if (app_rel_path.find('/') != std::string::npos ||
      app_rel_path.size() <= mojo_suffix.size() ||
      app_rel_path.compare(app_rel_path.size() - mojo_suffix.size(),
                           mojo_suffix.size(), mojo_suffix) != 0) {
    return false;
  }

  *app_dir = cached_apps_fp.Append(
      app_rel_path.substr(0, app_rel_path.size() - mojo_suffix.size()));
  *path_to_mojo = cached_apps_fp.Append(app_rel_path);
  return true;
}

}  // namespace

AndroidHandler::AndroidHandler() : content_handler_factory_(this) {}

AndroidHandler::~AndroidHandler() {}

void AndroidHandler::RunApplication(
    InterfaceRequest<mojom::ShellClient> request,
    URLResponsePtr response) {
  JNIEnv* env = AttachCurrentThread();
  RunAndroidApplicationFn run_android_application_fn = &RunAndroidApplication;
  if (!response->url.is_null()) {
    base::FilePath internal_app_path;
    base::FilePath path_to_mojo;
    if (IsCachedApp(env, GURL(response->url.get()), &internal_app_path,
                    &path_to_mojo)) {
      ScopedJavaLocalRef<jstring> j_internal_app_path(
          ConvertUTF8ToJavaString(env, internal_app_path.value()));
      ScopedJavaLocalRef<jstring> j_path_to_mojo(
          ConvertUTF8ToJavaString(env, path_to_mojo.value()));
      Java_AndroidHandler_bootstrapCachedApp(
          env, GetApplicationContext(), j_path_to_mojo.obj(),
          j_internal_app_path.obj(),
          request.PassMessagePipe().release().value(),
          reinterpret_cast<jlong>(run_android_application_fn));
      return;
    }
  }
  ScopedJavaLocalRef<jstring> j_archive_path =
      Java_AndroidHandler_getNewTempArchivePath(env, GetApplicationContext());
  base::FilePath archive_path(
      ConvertJavaStringToUTF8(env, j_archive_path.obj()));

  common::BlockingCopyToFile(std::move(response->body), archive_path);
  Java_AndroidHandler_bootstrap(
      env, GetApplicationContext(), j_archive_path.obj(),
      request.PassMessagePipe().release().value(),
      reinterpret_cast<jlong>(run_android_application_fn));
}

void AndroidHandler::Initialize(Shell* shell,
                                const std::string& url,
                                uint32_t id) {}

bool AndroidHandler::AcceptConnection(Connection* connection) {
  connection->AddInterface(&content_handler_factory_);
  return true;
}

bool RegisterAndroidHandlerJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace shell
}  // namespace mojo
