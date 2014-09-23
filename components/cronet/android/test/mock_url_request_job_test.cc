// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mock_url_request_job_test.h"

#include "base/android/jni_android.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/threading/sequenced_worker_pool.h"
#include "jni/MockUrlRequestJobTest_jni.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "net/test/url_request/url_request_mock_http_job.h"

namespace cronet {

static void AddUrlInterceptors(JNIEnv* env, jobject jcaller) {
  base::FilePath test_files_root;
  PathService::Get(base::DIR_ANDROID_APP_DATA, &test_files_root);
  net::URLRequestMockHTTPJob::AddUrlHandler(
      test_files_root, new base::SequencedWorkerPool(1, "Worker"));
  net::URLRequestFailedJob::AddUrlHandler();
}

bool RegisterMockUrlRequestJobTest(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace cronet
