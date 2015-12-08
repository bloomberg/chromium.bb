// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/data_usage/external_data_use_observer_bridge.h"

#include <vector>

#include "base/android/context_utils.h"
#include "base/android/jni_string.h"
#include "base/memory/scoped_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/android/data_usage/external_data_use_observer.h"
#include "content/public/browser/browser_thread.h"
#include "jni/ExternalDataUseObserver_jni.h"

using base::android::ConvertUTF8ToJavaString;

namespace chrome {

namespace android {

ExternalDataUseObserverBridge::ExternalDataUseObserverBridge() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  // Detach from IO thread since rest of ExternalDataUseObserverBridge lives on
  // the UI thread.
  thread_checker_.DetachFromThread();
}

ExternalDataUseObserverBridge::~ExternalDataUseObserverBridge() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (j_external_data_use_observer_.is_null())
    return;
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ExternalDataUseObserver_onDestroy(env,
                                         j_external_data_use_observer_.obj());
}

void ExternalDataUseObserverBridge::Init(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    base::WeakPtr<ExternalDataUseObserver> external_data_use_observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(io_task_runner);
  DCHECK(j_external_data_use_observer_.is_null());

  external_data_use_observer_ = external_data_use_observer;
  io_task_runner_ = io_task_runner;

  JNIEnv* env = base::android::AttachCurrentThread();
  j_external_data_use_observer_.Reset(Java_ExternalDataUseObserver_create(
      env, base::android::GetApplicationContext(),
      reinterpret_cast<intptr_t>(this)));
  DCHECK(!j_external_data_use_observer_.is_null());

  FetchMatchingRules();
}

void ExternalDataUseObserverBridge::FetchMatchingRules() const {
  DCHECK(thread_checker_.CalledOnValidThread());

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ExternalDataUseObserver_fetchMatchingRules(
      env, j_external_data_use_observer_.obj());
}

void ExternalDataUseObserverBridge::FetchMatchingRulesDone(
    JNIEnv* env,
    jobject obj,
    const base::android::JavaParamRef<jobjectArray>& app_package_name,
    const base::android::JavaParamRef<jobjectArray>& domain_path_regex,
    const base::android::JavaParamRef<jobjectArray>& label) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Convert to native objects.
  scoped_ptr<std::vector<std::string>> app_package_name_native(
      new std::vector<std::string>());
  scoped_ptr<std::vector<std::string>> domain_path_regex_native(
      new std::vector<std::string>());
  scoped_ptr<std::vector<std::string>> label_native(
      new std::vector<std::string>());

  if (app_package_name && domain_path_regex && label) {
    base::android::AppendJavaStringArrayToStringVector(
        env, app_package_name, app_package_name_native.get());
    base::android::AppendJavaStringArrayToStringVector(
        env, domain_path_regex, domain_path_regex_native.get());
    base::android::AppendJavaStringArrayToStringVector(env, label,
                                                       label_native.get());
  }

  io_task_runner_->PostTask(
      FROM_HERE, base::Bind(&ExternalDataUseObserver::FetchMatchingRulesDone,
                            external_data_use_observer_,
                            base::Owned(app_package_name_native.release()),
                            base::Owned(domain_path_regex_native.release()),
                            base::Owned(label_native.release())));
}

void ExternalDataUseObserverBridge::ReportDataUse(
    const std::string& label,
    net::NetworkChangeNotifier::ConnectionType connection_type,
    const std::string& mcc_mnc,
    const base::Time& start_time,
    const base::Time& end_time,
    int64_t bytes_downloaded,
    int64_t bytes_uploaded) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(!j_external_data_use_observer_.is_null());

  // End time should be greater than start time.
  int64_t start_time_milliseconds = start_time.ToJavaTime();
  int64_t end_time_milliseconds = end_time.ToJavaTime();
  if (start_time_milliseconds >= end_time_milliseconds)
    start_time_milliseconds = end_time_milliseconds - 1;

  Java_ExternalDataUseObserver_reportDataUse(
      env, j_external_data_use_observer_.obj(),
      ConvertUTF8ToJavaString(env, label).obj(), connection_type,
      ConvertUTF8ToJavaString(env, mcc_mnc).obj(), start_time_milliseconds,
      end_time_milliseconds, bytes_downloaded, bytes_uploaded);
}

void ExternalDataUseObserverBridge::OnReportDataUseDone(JNIEnv* env,
                                                        jobject obj,
                                                        bool success) {
  DCHECK(thread_checker_.CalledOnValidThread());

  io_task_runner_->PostTask(
      FROM_HERE, base::Bind(&ExternalDataUseObserver::OnReportDataUseDone,
                            external_data_use_observer_, success));
}

bool RegisterExternalDataUseObserver(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android

}  // namespace chrome
