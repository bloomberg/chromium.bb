// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_HISTORY_REPORT_HISTORY_REPORT_JNI_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_HISTORY_REPORT_HISTORY_REPORT_JNI_BRIDGE_H_

#include <jni.h>
#include <string>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/scoped_ptr.h"

namespace bookmarks {
class BookmarkModel;
}

namespace history_report {

class DataObserver;
class DataProvider;
class DeltaFileService;
class UsageReportsBufferService;

bool RegisterHistoryReportJniBridge(JNIEnv* env);

// JNI Bridge which connects native and java parts of Icing integration.
class HistoryReportJniBridge {
 public:
  HistoryReportJniBridge(JNIEnv* env, jobject obj);
  ~HistoryReportJniBridge();

  // Removes entries with seqno <= seq_no_lower_bound from delta file.
  // Returns biggest seqno in delta file.
  jlong TrimDeltaFile(JNIEnv* env, jobject obj, jlong seq_no_lower_bound);
  // Queries delta file for up to limit entries with seqno > last_seq_no.
  base::android::ScopedJavaLocalRef<jobjectArray> Query(JNIEnv* env,
                                                        jobject obj,
                                                        jlong last_seq_no,
                                                        jint limit);
  // Queries usage reports buffer for a batch of reports to be reported to
  // Icing.
  base::android::ScopedJavaLocalRef<jobjectArray> GetUsageReportsBatch(
      JNIEnv* env,
      jobject obj,
      jint batch_size);
  // Removes a batch of usage reports from a usage reports buffer.
  void RemoveUsageReports(JNIEnv* env,
                          jobject obj,
                          jobjectArray batch);
  // Populates the usage reports buffer with historic visits.
  // This should happen only once per corpus registration.
  jboolean AddHistoricVisitsToUsageReportsBuffer(JNIEnv* env, jobject obj);
  // Dumps internal state.
  base::android::ScopedJavaLocalRef<jstring> Dump(JNIEnv* env, jobject obj);

  void NotifyDataChanged();
  void NotifyDataCleared();
  void StopReporting();
  void StartReporting();

 private:
  JavaObjectWeakGlobalRef weak_java_provider_;
  scoped_ptr<DataObserver> data_observer_;
  scoped_ptr<DataProvider> data_provider_;
  scoped_ptr<DeltaFileService> delta_file_service_;
  scoped_ptr<bookmarks::BookmarkModel> bookmark_model_;
  scoped_ptr<UsageReportsBufferService> usage_reports_buffer_service_;

  DISALLOW_COPY_AND_ASSIGN(HistoryReportJniBridge);
};

}  // namespace history_report

#endif  // CHROME_BROWSER_ANDROID_HISTORY_REPORT_HISTORY_REPORT_JNI_BRIDGE_H_
