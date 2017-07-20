// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENGAGEMENT_SITE_ENGAGEMENT_SERVICE_ANDROID_H_
#define CHROME_BROWSER_ENGAGEMENT_SITE_ENGAGEMENT_SERVICE_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "chrome/browser/engagement/site_engagement_service.h"

// Wrapper class to expose the Site Engagement Service to Java. This object is
// owned by the |service_| which it wraps, and is lazily created when
// a Java-side SiteEngagementService is constructed. Once created, all future
// Java-side requests for a SiteEngagementService will use the same native
// object.
//
// This class may only be used on the UI thread.
class SiteEngagementServiceAndroid {
 public:
  // Returns the Java-side SiteEngagementService object corresponding to
  // |service|.
  static const base::android::ScopedJavaGlobalRef<jobject>& GetOrCreate(
      JNIEnv* env,
      SiteEngagementService* service);

  SiteEngagementServiceAndroid(JNIEnv* env, SiteEngagementService* service);

  ~SiteEngagementServiceAndroid();

  double GetScore(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& caller,
                  const base::android::JavaParamRef<jstring>& jurl) const;

  void ResetBaseScoreForURL(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& caller,
                            const base::android::JavaParamRef<jstring>& jurl,
                            double score);

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_service_;
  SiteEngagementService* service_;

  DISALLOW_COPY_AND_ASSIGN(SiteEngagementServiceAndroid);
};

#endif  // CHROME_BROWSER_ENGAGEMENT_SITE_ENGAGEMENT_SERVICE_ANDROID_H_
