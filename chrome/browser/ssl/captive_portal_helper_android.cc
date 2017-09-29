// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ssl/ssl_error_handler.h"
#include "content/public/browser/browser_thread.h"
#include "jni/CaptivePortalHelper_jni.h"

namespace chrome {
namespace android {

void SetCaptivePortalCertificateForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jclass>& jcaller,
    const base::android::JavaParamRef<jstring>& jhash) {
  const std::string hash = ConvertJavaStringToUTF8(env, jhash);
  auto config_proto =
      base::MakeUnique<chrome_browser_ssl::SSLErrorAssistantConfig>();
  config_proto->set_version_id(INT_MAX);
  config_proto->add_captive_portal_cert()->set_sha256_hash(hash);

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::BindOnce(SSLErrorHandler::SetErrorAssistantProto,
                     std::move(config_proto)));
}

void SetOSReportsCaptivePortalForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jclass>& jcaller,
    jboolean os_reports_captive_portal) {
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::BindOnce(SSLErrorHandler::SetOSReportsCaptivePortalForTesting,
                     os_reports_captive_portal));
}

std::string GetCaptivePortalServerUrl(JNIEnv* env) {
  return base::android::ConvertJavaStringToUTF8(
      Java_CaptivePortalHelper_getCaptivePortalServerUrl(env));
}

}  // namespace android
}  // namespace chrome
