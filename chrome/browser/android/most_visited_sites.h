// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_MOST_VISITED_SITES_H_
#define CHROME_BROWSER_ANDROID_MOST_VISITED_SITES_H_

#include <jni.h>

#include <string>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/top_sites_observer.h"
#include "components/suggestions/proto/suggestions.pb.h"
#include "components/sync_driver/sync_service_observer.h"

namespace suggestions {
class SuggestionsService;
}

class GURL;
class PopularSites;

// Provides the list of most visited sites and their thumbnails to Java.
class MostVisitedSites : public sync_driver::SyncServiceObserver,
                         public history::TopSitesObserver {
 public:
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

  // sync_driver::SyncServiceObserver implementation.
  void OnStateChanged() override;

  // Registers JNI methods.
  static bool Register(JNIEnv* env);

 private:
  friend class MostVisitedSitesTest;

  // The source of the Most Visited sites.
  enum MostVisitedSource {
    TOP_SITES,
    SUGGESTIONS_SERVICE
  };

  ~MostVisitedSites() override;
  void QueryMostVisitedURLs();

  // Initialize the query to Top Sites. Called if the SuggestionsService is not
  // enabled, or if it returns no data.
  void InitiateTopSitesQuery();

  // Callback for when data is available from TopSites.
  void OnMostVisitedURLsAvailable(
      const history::MostVisitedURLList& visited_list);

  // Callback for when data is available from the SuggestionsService.
  void OnSuggestionsProfileAvailable(
      const suggestions::SuggestionsProfile& suggestions_profile);

  // Adds the suggestions from |popular_sites_| into |titles| and |urls|. This
  // might reorder |titles| and |urls| to retain the absolute positions of the
  // popular suggestions.
  void AddPopularSites(std::vector<base::string16>* titles,
                       std::vector<std::string>* urls) const;

  // Workhorse for AddPopularSites above. Implemented as a separate static
  // method for ease of testing.
  static void AddPopularSitesImpl(
      int num_sites,
      std::vector<base::string16>* titles,
      std::vector<std::string>* urls,
      const std::vector<base::string16>& popular_titles,
      const std::vector<std::string>& popular_urls);

  // Notify the Java side observer about the availability of Most Visited Urls.
  void NotifyMostVisitedURLsObserver(const std::vector<base::string16>& titles,
                                     const std::vector<std::string>& urls);

  void OnPopularSitesAvailable(bool success);

  // Runs on the UI Thread.
  void OnLocalThumbnailFetched(
      const GURL& url,
      scoped_ptr<base::android::ScopedJavaGlobalRef<jobject>> j_callback,
      scoped_ptr<SkBitmap> bitmap);

  // Callback for when the thumbnail lookup is complete.
  // Runs on the UI Thread.
  void OnObtainedThumbnail(
      bool is_local_thumbnail,
      scoped_ptr<base::android::ScopedJavaGlobalRef<jobject>> j_callback,
      const GURL& url,
      const SkBitmap* bitmap);

  // Records specific UMA histogram metrics.
  void RecordUMAMetrics();

  // history::TopSitesObserver implementation.
  void TopSitesLoaded(history::TopSites* top_sites) override;
  void TopSitesChanged(history::TopSites* top_sites,
                       ChangeReason change_reason) override;

  // The profile whose most visited sites will be queried.
  Profile* profile_;

  // The observer to be notified when the list of most visited sites changes.
  base::android::ScopedJavaGlobalRef<jobject> observer_;

  // The maximum number of most visited sites to return.
  int num_sites_;

  // Keeps track of whether the initial NTP load has been done.
  bool initial_load_done_;

  // Counters for UMA metrics.

  // Number of tiles using a local thumbnail image for this NTP session.
  int num_local_thumbs_;
  // Number of tiles for which a server thumbnail is provided.
  int num_server_thumbs_;
  // Number of tiles for which no thumbnail is found/specified.
  // In this case a gray tile is used as the main tile.
  int num_empty_thumbs_;

  // Copy of the server suggestions (if enabled). Used for logging.
  suggestions::SuggestionsProfile server_suggestions_;

  ScopedObserver<history::TopSites, history::TopSitesObserver> scoped_observer_;

  MostVisitedSource mv_source_;

  scoped_ptr<PopularSites> popular_sites_;

  // For callbacks may be run after destruction.
  base::WeakPtrFactory<MostVisitedSites> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MostVisitedSites);
};

#endif  // CHROME_BROWSER_ANDROID_MOST_VISITED_SITES_H_
