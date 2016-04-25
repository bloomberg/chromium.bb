// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_NTP_NTP_SNIPPETS_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_NTP_NTP_SNIPPETS_BRIDGE_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/scoped_observer.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/history/core/browser/history_service.h"
#include "components/ntp_snippets/ntp_snippets_service.h"

// The C++ counterpart to SnippetsBridge.java. Enables Java code to access
// the list of snippets to show on the NTP
class NTPSnippetsBridge : public ntp_snippets::NTPSnippetsServiceObserver {
 public:
  NTPSnippetsBridge(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& j_profile);
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  void SetObserver(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj,
                   const base::android::JavaParamRef<jobject>& j_observer);

  // Discards the snippet with the given URL.
  void DiscardSnippet(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj,
                      const base::android::JavaParamRef<jstring>& url);

  // Checks if the URL has been visited.
  void SnippetVisited(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj,
                      const base::android::JavaParamRef<jobject>& callback,
                      const base::android::JavaParamRef<jstring>& jurl);

  static bool Register(JNIEnv* env);

 private:
  ~NTPSnippetsBridge() override;

  // NTPSnippetsServiceObserver overrides
  void NTPSnippetsServiceLoaded() override;
  void NTPSnippetsServiceShutdown() override;

  ntp_snippets::NTPSnippetsService* ntp_snippets_service_;
  history::HistoryService* history_service_;
  base::CancelableTaskTracker tracker_;

  // Used to notify the Java side when new snippets have been fetched.
  base::android::ScopedJavaGlobalRef<jobject> observer_;
  ScopedObserver<ntp_snippets::NTPSnippetsService,
                 ntp_snippets::NTPSnippetsServiceObserver>
      snippet_service_observer_;

  DISALLOW_COPY_AND_ASSIGN(NTPSnippetsBridge);
};

#endif  // CHROME_BROWSER_ANDROID_NTP_NTP_SNIPPETS_BRIDGE_H_
