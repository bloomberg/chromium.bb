// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_MOST_VISITED_SITES_H_
#define CHROME_BROWSER_ANDROID_MOST_VISITED_SITES_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_observer.h"
#include "components/suggestions/proto/suggestions.pb.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace suggestions {
class SuggestionsService;
}

// Provides the list of most visited sites and their thumbnails to Java.
class MostVisitedSites : public ProfileSyncServiceObserver,
                         public content::NotificationObserver {
 public:
  typedef base::Callback<
      void(base::android::ScopedJavaGlobalRef<jobject>* bitmap,
           base::android::ScopedJavaGlobalRef<jobject>* j_callback)>
      LookupSuccessCallback;

  explicit MostVisitedSites(Profile* profile);
  void Destroy(JNIEnv* env, jobject obj);
  void OnLoadingComplete(JNIEnv* env, jobject obj);
  void SetMostVisitedURLsObserver(JNIEnv* env,
                                  jobject obj,
                                  jobject j_observer,
                                  jint num_sites);
  void GetURLThumbnail(JNIEnv* env,
                       jobject obj,
                       jstring url,
                       jobject j_callback);
  void BlacklistUrl(JNIEnv* env, jobject obj, jstring j_url);
  void RecordOpenedMostVisitedItem(JNIEnv* env, jobject obj, jint index);

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // ProfileSyncServiceObserver implementation.
  virtual void OnStateChanged() OVERRIDE;

  // Registers JNI methods.
  static bool Register(JNIEnv* env);

 private:
  virtual ~MostVisitedSites();
  void QueryMostVisitedURLs();

  // Initialize the query to Top Sites. Called if the SuggestionsService is not
  // enabled, or if it returns no data.
  void InitiateTopSitesQuery();

  // Callback for when data is available from TopSites.
  void OnMostVisitedURLsAvailable(
      base::android::ScopedJavaGlobalRef<jobject>* j_observer,
      int num_sites,
      const history::MostVisitedURLList& visited_list);

  // Callback for when data is available from the SuggestionsService.
  void OnSuggestionsProfileAvailable(
      base::android::ScopedJavaGlobalRef<jobject>* j_observer,
      const suggestions::SuggestionsProfile& suggestions_profile);

  // Callback for when the local thumbnail lookup is complete.
  void OnObtainedThumbnail(
      base::android::ScopedJavaGlobalRef<jobject>* bitmap,
      base::android::ScopedJavaGlobalRef<jobject>* j_callback);

  // Requests a server thumbnail from the |suggestions_service|.
  void GetSuggestionsThumbnailOnUIThread(
      suggestions::SuggestionsService* suggestions_service,
      const std::string& url_string,
      base::android::ScopedJavaGlobalRef<jobject>* j_callback);

  // Callback from the SuggestionsServer regarding the server thumbnail lookup.
  void OnSuggestionsThumbnailAvailable(
      base::android::ScopedJavaGlobalRef<jobject>* j_callback,
      const GURL& url,
      const SkBitmap* bitmap);

  // Records specific UMA histogram metrics.
  void RecordUMAMetrics();

  // The profile whose most visited sites will be queried.
  Profile* profile_;

  // The observer to be notified when the list of most visited sites changes.
  base::android::ScopedJavaGlobalRef<jobject> observer_;

  // The maximum number of most visited sites to return.
  int num_sites_;

  // Whether the user is in a control group for the purposes of logging.
  bool is_control_group_;

  // Counters for UMA metrics.

  // Number of tiles using a local thumbnail image for this NTP session.
  int num_local_thumbs_;
  // Number of tiles for which a server thumbnail is provided.
  int num_server_thumbs_;
  // Number of tiles for which no thumbnail is found/specified and a gray tile
  // is used as the main tile.
  int num_empty_thumbs_;

  // Copy of the server suggestions (if enabled). Used for logging.
  suggestions::SuggestionsProfile server_suggestions_;

  // For callbacks may be run after destruction.
  base::WeakPtrFactory<MostVisitedSites> weak_ptr_factory_;

  content::NotificationRegistrar registrar_;

  // The source of the Most Visited sites.
  enum MostVisitedSource {
    TOP_SITES,
    SUGGESTIONS_SERVICE
  };
  MostVisitedSource mv_source_;

  DISALLOW_COPY_AND_ASSIGN(MostVisitedSites);
};

#endif  // CHROME_BROWSER_ANDROID_MOST_VISITED_SITES_H_
