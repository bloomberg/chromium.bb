// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_MOST_VISITED_SITES_H_
#define CHROME_BROWSER_ANDROID_MOST_VISITED_SITES_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace suggestions {
class SuggestionsProfile;
}

// Provides the list of most visited sites and their thumbnails to Java.
class MostVisitedSites : public content::NotificationObserver {
 public:
  explicit MostVisitedSites(Profile* profile);
  void Destroy(JNIEnv* env, jobject obj);
  void SetMostVisitedURLsObserver(JNIEnv* env,
                                  jobject obj,
                                  jobject j_observer,
                                  jint num_sites);
  void GetURLThumbnail(JNIEnv* env,
                       jobject obj,
                       jstring url,
                       jobject j_callback);
  void BlacklistUrl(JNIEnv* env, jobject obj, jstring j_url);

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Registers JNI methods.
  static bool Register(JNIEnv* env);

 private:
  virtual ~MostVisitedSites();
  void QueryMostVisitedURLs();

  // Initialize the query to Top Sites. Called if the SuggestionsService is not
  // enabled, or if it returns no data.
  void InitiateTopSitesQuery();

  // Callback for when data is available from the SuggestionsService.
  void OnSuggestionsProfileAvailable(
      base::android::ScopedJavaGlobalRef<jobject>* j_observer,
      const suggestions::SuggestionsProfile& suggestions_profile);

  // The profile whose most visited sites will be queried.
  Profile* profile_;

  // The observer to be notified when the list of most visited sites changes.
  base::android::ScopedJavaGlobalRef<jobject> observer_;

  // The maximum number of most visited sites to return.
  int num_sites_;

  // For callbacks may be run after destruction.
  base::WeakPtrFactory<MostVisitedSites> weak_ptr_factory_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(MostVisitedSites);
};

#endif  // CHROME_BROWSER_ANDROID_MOST_VISITED_SITES_H_
