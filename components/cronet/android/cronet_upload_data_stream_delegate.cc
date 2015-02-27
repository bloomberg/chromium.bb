// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/android/cronet_upload_data_stream_delegate.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/single_thread_task_runner.h"
#include "components/cronet/android/cronet_url_request_adapter.h"
#include "jni/CronetUploadDataStream_jni.h"

using base::android::ConvertUTF8ToJavaString;

namespace cronet {

CronetUploadDataStreamDelegate::CronetUploadDataStreamDelegate(
    JNIEnv* env,
    jobject jupload_data_stream) {
  jupload_data_stream_.Reset(env, jupload_data_stream);
}

CronetUploadDataStreamDelegate::~CronetUploadDataStreamDelegate() {
}

void CronetUploadDataStreamDelegate::InitializeOnNetworkThread(
    base::WeakPtr<CronetUploadDataStreamAdapter> adapter) {
  DCHECK(!adapter_);
  DCHECK(!network_task_runner_.get());

  adapter_ = adapter;
  network_task_runner_ = base::MessageLoopProxy::current();
  DCHECK(network_task_runner_);
}

void CronetUploadDataStreamDelegate::Read(net::IOBuffer* buffer, int buf_len) {
  DCHECK(adapter_);
  DCHECK(network_task_runner_);
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  DCHECK_GT(buf_len, 0);
  DCHECK(!buffer_.get());
  buffer_ = buffer;

  // TODO(mmenke):  Consider preserving the java buffer across reads, when the
  // IOBuffer's data pointer and its length are unchanged.
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> java_buffer(
      env, env->NewDirectByteBuffer(buffer->data(), buf_len));
  Java_CronetUploadDataStream_readData(env, jupload_data_stream_.obj(),
                                       java_buffer.obj());
}

void CronetUploadDataStreamDelegate::Rewind() {
  DCHECK(adapter_);
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CronetUploadDataStream_rewind(env, jupload_data_stream_.obj());
}

void CronetUploadDataStreamDelegate::OnAdapterDestroyed() {
  DCHECK(adapter_);
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CronetUploadDataStream_onAdapterDestroyed(env,
                                                 jupload_data_stream_.obj());
}

void CronetUploadDataStreamDelegate::OnReadSucceeded(JNIEnv* env,
                                                     jobject jcaller,
                                                     int bytes_read,
                                                     bool final_chunk) {
  DCHECK(!network_task_runner_->BelongsToCurrentThread());
  DCHECK(bytes_read > 0 || (final_chunk && bytes_read == 0));

  buffer_ = nullptr;
  network_task_runner_->PostTask(
      FROM_HERE, base::Bind(&CronetUploadDataStreamAdapter::OnReadSuccess,
                            adapter_, bytes_read, final_chunk));
}

void CronetUploadDataStreamDelegate::OnRewindSucceeded(JNIEnv* env,
                                                       jobject jcaller) {
  DCHECK(!network_task_runner_->BelongsToCurrentThread());

  network_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&CronetUploadDataStreamAdapter::OnRewindSuccess, adapter_));
}

bool CronetUploadDataStreamDelegateRegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

static jlong AttachUploadDataToRequest(JNIEnv* env,
                                       jobject jupload_data_stream,
                                       jlong jcronet_url_request_adapter,
                                       jlong jlength) {
  CronetURLRequestAdapter* request_adapter =
      reinterpret_cast<CronetURLRequestAdapter*>(jcronet_url_request_adapter);
  DCHECK(request_adapter != nullptr);

  CronetUploadDataStreamDelegate* delegate =
      new CronetUploadDataStreamDelegate(env, jupload_data_stream);

  scoped_ptr<CronetUploadDataStreamAdapter> upload_adapter(
      new CronetUploadDataStreamAdapter(delegate, jlength));

  request_adapter->SetUpload(upload_adapter.Pass());

  return reinterpret_cast<jlong>(delegate);
}

static jlong CreateDelegateForTesting(JNIEnv* env,
                                      jobject jupload_data_stream) {
  CronetUploadDataStreamDelegate* delegate =
      new CronetUploadDataStreamDelegate(env, jupload_data_stream);
  return reinterpret_cast<jlong>(delegate);
}

static jlong CreateAdapterForTesting(JNIEnv* env,
                                     jobject jupload_data_stream,
                                     jlong jlength,
                                     jlong jdelegate) {
  CronetUploadDataStreamDelegate* delegate =
      reinterpret_cast<CronetUploadDataStreamDelegate*>(jdelegate);
  CronetUploadDataStreamAdapter* upload_adapter =
      new CronetUploadDataStreamAdapter(delegate, jlength);
  return reinterpret_cast<jlong>(upload_adapter);
}

static void DestroyDelegate(JNIEnv* env,
                            jclass jcronet_url_request_adapter,
                            jlong jupload_data_stream_delegate) {
  CronetUploadDataStreamDelegate* delegate =
      reinterpret_cast<CronetUploadDataStreamDelegate*>(
          jupload_data_stream_delegate);
  DCHECK(delegate != nullptr);
  delete delegate;
}

}  // namespace cronet
