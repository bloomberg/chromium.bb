// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_ANDROID_TEST_TEST_UPLOAD_DATA_STREAM_HANDLER_H_
#define COMPONENTS_CRONET_ANDROID_TEST_TEST_UPLOAD_DATA_STREAM_HANDLER_H_

#include <jni.h>

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "net/base/io_buffer.h"
#include "net/base/upload_data_stream.h"

namespace cronet {

/**
 * This class allows a net::UploadDataStream to be driven directly from
 * Java, for use in tests.
 */
class TestUploadDataStreamHandler {
 public:
  TestUploadDataStreamHandler(
      std::unique_ptr<net::UploadDataStream> upload_data_stream,
      JNIEnv* env,
      jobject jtest_upload_data_stream_handler,
      jlong jcontext_adapter);

  ~TestUploadDataStreamHandler();

  // Destroys |network_thread_| created by this class.
  void Destroy(JNIEnv* env,
               const base::android::JavaParamRef<jobject>& jcaller);

  // Posts a task to |network_thread_| to call the corresponding method of
  // net::UploadDataStream on |upload_data_stream_|.

  void Init(JNIEnv* env, const base::android::JavaParamRef<jobject>& jcaller);
  void Read(JNIEnv* env, const base::android::JavaParamRef<jobject>& jcaller);
  void Reset(JNIEnv* env, const base::android::JavaParamRef<jobject>& jcaller);

  // Posts a task to |network_thread_| to check whether init complete callback
  // has been invoked by net::UploadDataStream asynchronously, and notifies the
  // Java side of the result.
  void CheckInitCallbackNotInvoked(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller);
  // Posts a task to |network_thread_| to check whether read complete callback
  // has been invoked by net::UploadDataStream asynchronously, and notifies the
  // Java side of the result.
  void CheckReadCallbackNotInvoked(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller);

 private:
  // Complete callbacks that are passed to the |upload_data_stream_|.
  void OnInitCompleted(int res);
  void OnReadCompleted(int res);

  // Helper methods that run corresponding task on |network_thread_|.

  void InitOnNetworkThread();
  void ReadOnNetworkThread();
  void ResetOnNetworkThread();
  void CheckInitCallbackNotInvokedOnNetworkThread();
  void CheckReadCallbackNotInvokedOnNetworkThread();

  // Notify the Java TestUploadDataStreamHandler that read has completed.
  void NotifyJavaReadCompleted();

  // True if |OnInitCompleted| callback has been invoked. It is set to false
  // when init or reset is called again. Created on a Java thread, but is only
  // accessed from |network_thread_|.
  bool init_callback_invoked_;
  // True if |OnReadCompleted| callback has been invoked. It is set to false
  // when init or reset is called again. Created on a Java thread, but is only
  // accessed from |network_thread_|.
  bool read_callback_invoked_;
  // Indicates the number of bytes read. It is reset to 0 when init, reset, or
  // read is called again. Created on a Java thread, but is only accessed from
  // |network_thread_|.
  int bytes_read_;

  // Created and destroyed on the same Java thread. This is where methods of
  // net::UploadDataStream run on.
  scoped_refptr<base::SingleThreadTaskRunner> network_thread_;
  // Created on a Java thread. Accessed only on |network_thread_|.
  std::unique_ptr<net::UploadDataStream> upload_data_stream_;
  // Created and accessed only on |network_thread_|.
  scoped_refptr<net::IOBufferWithSize> read_buffer_;
  // A Java reference pointer for calling methods on the Java
  // TestUploadDataStreamHandler object. Initialized during construction.
  base::android::ScopedJavaGlobalRef<jobject> jtest_upload_data_stream_handler_;

  DISALLOW_COPY_AND_ASSIGN(TestUploadDataStreamHandler);
};

}  // namespace cronet

#endif  // COMPONENTS_CRONET_ANDROID_TEST_TEST_UPLOAD_DATA_STREAM_HANDLER_H_
