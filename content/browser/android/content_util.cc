// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/content_util.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "content/browser/web_contents/web_contents_impl.h"

using base::android::AppendJavaStringArrayToStringVector;
using base::android::CheckException;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;
using base::android::GetClass;
using base::android::GetFieldID;
using base::android::GetMethodID;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaArrayOfStrings;

namespace {

const char kFileChooserParamsClassName[] =
    "org/chromium/content/browser/FileChooserParams";

}

namespace content {

ScopedJavaLocalRef<jobject> ToJavaFileChooserParams(
    JNIEnv* env, const FileChooserParams& file_chooser_params) {
  ScopedJavaLocalRef<jclass> clazz = GetClass(env, kFileChooserParamsClassName);
  jmethodID constructor_id = GetMethodID(env, clazz, "<init>",
      "(ILjava/lang/String;Ljava/lang/String;[Ljava/lang/String;"
      "Ljava/lang/String;)V");

  ScopedJavaLocalRef<jstring> title =
      ConvertUTF16ToJavaString(env, file_chooser_params.title);
  ScopedJavaLocalRef<jstring> default_file_name =
      ConvertUTF8ToJavaString(env,
                              file_chooser_params.default_file_name.value());
  ScopedJavaLocalRef<jobjectArray> accept_types =
      ToJavaArrayOfStrings(env, file_chooser_params.accept_types);
  ScopedJavaLocalRef<jstring> capture =
      ConvertUTF16ToJavaString(env, file_chooser_params.capture);

  ScopedJavaLocalRef<jobject> java_params(env,
      env->NewObject(clazz.obj(),
                     constructor_id,
                     static_cast<jint>(file_chooser_params.mode),
                     title.obj(),
                     default_file_name.obj(),
                     accept_types.obj(),
                     capture.obj()));
  CheckException(env);

  return java_params;
}

FileChooserParams ToNativeFileChooserParams(
    JNIEnv* env, jint mode, jstring title, jstring default_file,
    jobjectArray accept_types, jstring capture) {
  FileChooserParams file_chooser_params;
  file_chooser_params.mode =
      static_cast<FileChooserParams::Mode>(mode);
  file_chooser_params.title = ConvertJavaStringToUTF16(env, title);
  file_chooser_params.default_file_name =
      FilePath(ConvertJavaStringToUTF8(env, default_file));
  AppendJavaStringArrayToStringVector(env, accept_types,
                                      &file_chooser_params.accept_types);
  file_chooser_params.capture = ConvertJavaStringToUTF16(env, capture);

  return file_chooser_params;
}

}  // namespace content

