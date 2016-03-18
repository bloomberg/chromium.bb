// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_NTP_NTP_SNIPPETS_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_NTP_NTP_SNIPPETS_BRIDGE_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/scoped_observer.h"
#include "components/ntp_snippets/ntp_snippets_service.h"

// The C++ counterpart to NTPSnippetsBridge.java. Enables Java code to access
// the list of snippets to show on the NTP
class NTPSnippetsBridge : public ntp_snippets::NTPSnippetsServiceObserver {
 public:
  NTPSnippetsBridge(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& j_profile);
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  void SetObserver(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj,
                   const base::android::JavaParamRef<jobject>& j_observer);

  static bool Register(JNIEnv* env);

 private:
  ~NTPSnippetsBridge() override;

  // NTPSnippetsServiceObserver overrides
  void NTPSnippetsServiceLoaded(
      ntp_snippets::NTPSnippetsService* service) override;
  void NTPSnippetsServiceShutdown(
      ntp_snippets::NTPSnippetsService* service) override;

  ntp_snippets::NTPSnippetsService* ntp_snippets_service_;

  base::android::ScopedJavaGlobalRef<jobject> observer_;
  ScopedObserver<ntp_snippets::NTPSnippetsService,
                 ntp_snippets::NTPSnippetsServiceObserver>
      snippet_service_observer_;

  DISALLOW_COPY_AND_ASSIGN(NTPSnippetsBridge);
};

#endif  // CHROME_BROWSER_ANDROID_NTP_NTP_SNIPPETS_BRIDGE_H_
