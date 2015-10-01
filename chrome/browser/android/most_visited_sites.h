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
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/top_sites_observer.h"
#include "components/suggestions/proto/suggestions.pb.h"
#include "components/sync_driver/sync_service_observer.h"
#include "url/gurl.h"

namespace suggestions {
class SuggestionsService;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

class PopularSites;
class Profile;

// Provides the list of most visited sites and their thumbnails to Java.
class MostVisitedSites : public sync_driver::SyncServiceObserver,
                         public history::TopSitesObserver {
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
  void RecordTileTypeMetrics(JNIEnv* env,
                             jobject obj,
                             jintArray jtile_types,
                             jboolean is_icon_mode);
  void RecordOpenedMostVisitedItem(JNIEnv* env,
                                   jobject obj,
                                   jint index,
                                   jint tile_type);

  // sync_driver::SyncServiceObserver implementation.
  void OnStateChanged() override;

  // Registers JNI methods.
  static bool Register(JNIEnv* env);

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  friend class MostVisitedSitesTest;

  // The source of the Most Visited sites.
  enum MostVisitedSource { TOP_SITES, SUGGESTIONS_SERVICE, POPULAR };

  struct Suggestion {
    base::string16 title;
    GURL url;
    MostVisitedSource source;
    // Only valid for source == SUGGESTIONS_SERVICE (-1 otherwise).
    int provider_index;

    Suggestion(const base::string16& title,
               const std::string& url,
               MostVisitedSource source);
    Suggestion(const base::string16& title,
               const GURL& url,
               MostVisitedSource source);
    Suggestion(const base::string16& title,
               const std::string& url,
               MostVisitedSource source,
               int provider_index);
    ~Suggestion();

    // Get the Histogram name associated with the source.
    std::string GetSourceHistogramName() const;

   private:
    DISALLOW_COPY_AND_ASSIGN(Suggestion);
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

  // Takes the personal suggestions and adds popular suggestions if necessary
  // and reorders the suggestions based on the previously displayed order.
  void AddPopularSites(ScopedVector<Suggestion>* suggestions);

  // Workhorse for AddPopularSites above. Implemented as a separate static
  // method for ease of testing.
  static ScopedVector<Suggestion> MergeSuggestions(
      ScopedVector<Suggestion>* personal_suggestions,
      ScopedVector<Suggestion>* popular_suggestions,
      const std::vector<std::string>& old_sites_url,
      const std::vector<bool>& old_sites_is_personal);

  void GetPreviousNTPSites(size_t num_tiles,
                           std::vector<std::string>* old_sites_url,
                           std::vector<bool>* old_sites_source) const;

  void SaveCurrentNTPSites();

  // Takes suggestions from |src_suggestions| and moves them to
  // |dst_suggestions| if the suggestion's url/host matches
  // |match_urls|/|match_hosts| respectively. Unmatched suggestion indices from
  // |src_suggestions| are returned for ease of insertion later.
  static std::vector<size_t> InsertMatchingSuggestions(
      ScopedVector<Suggestion>* src_suggestions,
      ScopedVector<Suggestion>* dst_suggestions,
      const std::vector<std::string>& match_urls,
      const std::vector<std::string>& match_hosts);

  // Inserts suggestions from |src_suggestions| at positions |insert_positions|
  // into |dst_suggestions| where ever empty starting from |start_position|.
  // Returns the last filled position so that future insertions can start from
  // there.
  static size_t InsertAllSuggestions(
      size_t start_position,
      const std::vector<size_t>& insert_positions,
      ScopedVector<Suggestion>* src_suggestions,
      ScopedVector<Suggestion>* dst_suggestions);

  // Notify the Java side observer about the availability of Most Visited Urls.
  void NotifyMostVisitedURLsObserver();

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

  // Records thumbnail-related UMA histogram metrics.
  void RecordThumbnailUMAMetrics();

  // Records UMA histogram metrics related to the number of impressions.
  void RecordImpressionUMAMetrics();

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

  // Whether we have received an initial set of most visited sites (from either
  // TopSites or the SuggestionsService).
  bool received_most_visited_sites_;

  // Whether we have received the set of popular sites. Immediately set to true
  // if popular sites are disabled.
  bool received_popular_sites_;

  // Whether we have recorded one-shot UMA metrics such as impressions. They are
  // recorded once both the previous flags are true.
  bool recorded_uma_;

  ScopedObserver<history::TopSites, history::TopSitesObserver> scoped_observer_;

  MostVisitedSource mv_source_;

  scoped_ptr<PopularSites> popular_sites_;

  ScopedVector<Suggestion> current_suggestions_;

  // For callbacks may be run after destruction.
  base::WeakPtrFactory<MostVisitedSites> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MostVisitedSites);
};

#endif  // CHROME_BROWSER_ANDROID_MOST_VISITED_SITES_H_
