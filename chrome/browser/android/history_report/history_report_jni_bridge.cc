// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/history_report/history_report_jni_bridge.h"
#include <vector>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "chrome/browser/android/history_report/data_observer.h"
#include "chrome/browser/android/history_report/data_provider.h"
#include "chrome/browser/android/history_report/delta_file_commons.h"
#include "chrome/browser/android/history_report/delta_file_service.h"
#include "chrome/browser/android/history_report/usage_report_util.h"
#include "chrome/browser/android/history_report/usage_reports_buffer_service.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/history/core/browser/history_service.h"
#include "content/public/browser/browser_thread.h"
#include "jni/HistoryReportJniBridge_jni.h"

namespace history_report {

static jlong Init(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  HistoryReportJniBridge* bridge = new HistoryReportJniBridge(env, obj);
  return reinterpret_cast<intptr_t>(bridge);
}

bool RegisterHistoryReportJniBridge(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

HistoryReportJniBridge::HistoryReportJniBridge(JNIEnv* env, jobject obj)
    : weak_java_provider_(env, obj) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  Profile* profile = g_browser_process->profile_manager()->
      GetLastUsedProfile()->GetOriginalProfile();

  delta_file_service_.reset(new DeltaFileService(profile->GetPath()));
  usage_reports_buffer_service_.reset(
      new UsageReportsBufferService(profile->GetPath()));
  usage_reports_buffer_service_->Init();
  bookmark_model_.reset(BookmarkModelFactory::GetForProfile(profile));
  base::Callback<void(void)> on_change = base::Bind(
      &history_report::HistoryReportJniBridge::NotifyDataChanged,
      base::Unretained(this));
  base::Callback<void(void)> on_clear = base::Bind(
      &history_report::HistoryReportJniBridge::NotifyDataCleared,
      base::Unretained(this));
  base::Callback<void(void)> stop_reporting = base::Bind(
      &history_report::HistoryReportJniBridge::StopReporting,
      base::Unretained(this));
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  data_observer_.reset(new DataObserver(on_change,
                                        on_clear,
                                        stop_reporting,
                                        delta_file_service_.get(),
                                        usage_reports_buffer_service_.get(),
                                        bookmark_model_.get(),
                                        history_service));
  data_provider_.reset(new DataProvider(profile,
                                        delta_file_service_.get(),
                                        bookmark_model_.get()));
}

HistoryReportJniBridge::~HistoryReportJniBridge() {}

jlong HistoryReportJniBridge::TrimDeltaFile(JNIEnv* env,
                                    jobject obj,
                                    jlong seq_no_lower_bound) {
  return delta_file_service_->Trim(seq_no_lower_bound);
}

base::android::ScopedJavaLocalRef<jobjectArray> HistoryReportJniBridge::Query(
    JNIEnv* env,
    jobject obj,
    jlong last_seq_no,
    jint limit) {
  scoped_ptr<std::vector<DeltaFileEntryWithData> > entries =
      data_provider_->Query(last_seq_no, limit);
  ScopedJavaLocalRef<jobjectArray> jentries_array =
      history_report::Java_HistoryReportJniBridge_createDeltaFileEntriesArray(
          env, entries->size());

  int64 max_seq_no = 0;
  for (size_t i = 0; i < entries->size(); ++i) {
    const DeltaFileEntryWithData& entry = (*entries)[i];
    max_seq_no = max_seq_no < entry.SeqNo() ? entry.SeqNo() : max_seq_no;
    history_report::Java_HistoryReportJniBridge_setDeltaFileEntry(
        env, jentries_array.obj(), i, entry.SeqNo(),
        base::android::ConvertUTF8ToJavaString(env, entry.Type()).obj(),
        base::android::ConvertUTF8ToJavaString(env, entry.Id()).obj(),
        base::android::ConvertUTF8ToJavaString(env, entry.Url()).obj(),
        entry.Score(),
        base::android::ConvertUTF16ToJavaString(env, entry.Title()).obj(),
        base::android::ConvertUTF8ToJavaString(env, entry.IndexedUrl()).obj());
  }

  // Check if all entries from delta file were synced and start reporting usage
  // if it's true.
  if (entries->empty() || delta_file_service_->Query(max_seq_no, 1)->empty())
    StartReporting();

  return jentries_array;
}

base::android::ScopedJavaLocalRef<jobjectArray>
HistoryReportJniBridge::GetUsageReportsBatch(JNIEnv* env,
                                     jobject obj,
                                     jint batch_size) {
  scoped_ptr<std::vector<UsageReport> > reports =
      usage_reports_buffer_service_->GetUsageReportsBatch(batch_size);
  ScopedJavaLocalRef<jobjectArray> jreports_array =
      history_report::Java_HistoryReportJniBridge_createUsageReportsArray(env,
                                                         reports->size());

  for (size_t i = 0; i < reports->size(); ++i) {
    const UsageReport& report = (*reports)[i];
    std::string key = usage_report_util::ReportToKey(report);
    history_report::Java_HistoryReportJniBridge_setUsageReport(
        env, jreports_array.obj(), i,
        base::android::ConvertUTF8ToJavaString(env, key).obj(),
        base::android::ConvertUTF8ToJavaString(env, report.id()).obj(),
        report.timestamp_ms(), report.typed_visit());
  }
  return jreports_array;
}

void HistoryReportJniBridge::RemoveUsageReports(JNIEnv* env,
                                        jobject obj,
                                        jobjectArray batch) {
  std::vector<std::string> to_remove;
  base::android::AppendJavaStringArrayToStringVector(env, batch, &to_remove);
  usage_reports_buffer_service_->Remove(to_remove);
}

void HistoryReportJniBridge::NotifyDataChanged() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_provider_.get(env);
  if (!obj.is_null())
    history_report::Java_HistoryReportJniBridge_onDataChanged(env, obj.obj());
}

void HistoryReportJniBridge::NotifyDataCleared() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_provider_.get(env);
  if (!obj.is_null())
    history_report::Java_HistoryReportJniBridge_onDataCleared(env, obj.obj());
}

void HistoryReportJniBridge::StopReporting() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_provider_.get(env);
  if (!obj.is_null())
    history_report::Java_HistoryReportJniBridge_stopReportingTask(env,
                                                                  obj.obj());
}

void HistoryReportJniBridge::StartReporting() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_provider_.get(env);
  if (!obj.is_null())
    history_report::Java_HistoryReportJniBridge_startReportingTask(env,
                                                                   obj.obj());
}

jboolean HistoryReportJniBridge::AddHistoricVisitsToUsageReportsBuffer(
    JNIEnv* env,
    jobject obj) {
  data_provider_->StartVisitMigrationToUsageBuffer(
      usage_reports_buffer_service_.get());
  // TODO(nileshagrawal): Return true when actually done,
  // or callback on success.
  return true;
}

base::android::ScopedJavaLocalRef<jstring> HistoryReportJniBridge::Dump(
    JNIEnv* env,
    jobject obj) {
  std::string dump;
  dump.append(delta_file_service_->Dump());
  dump.append(usage_reports_buffer_service_->Dump());
  return base::android::ConvertUTF8ToJavaString(env, dump);
}

}  // namespace history_report
