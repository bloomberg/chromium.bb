// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/most_visited_sites.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/android/popular_sites.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/search/suggestions/suggestions_service_factory.h"
#include "chrome/browser/search/suggestions/suggestions_source.h"
#include "chrome/browser/search/suggestions/suggestions_utils.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/thumbnails/thumbnail_list_source.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/history/core/browser/top_sites.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/suggestions/suggestions_service.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/url_data_source.h"
#include "jni/MostVisitedSites_jni.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "url/gurl.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaArrayOfStrings;
using content::BrowserThread;
using history::TopSites;
using suggestions::ChromeSuggestion;
using suggestions::SuggestionsProfile;
using suggestions::SuggestionsService;
using suggestions::SuggestionsServiceFactory;

namespace {

// Identifiers for the various tile sources.
const char kHistogramClientName[] = "client";
const char kHistogramServerName[] = "server";
const char kHistogramServerFormat[] = "server%d";
const char kHistogramPopularName[] = "popular";

const char kPopularSitesFieldTrialName[] = "NTPPopularSites";

// The visual type of a most visited tile.
//
// These values must stay in sync with the MostVisitedTileType enum
// in histograms.xml.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.ntp
enum MostVisitedTileType {
    // The icon or thumbnail hasn't loaded yet.
    NONE,
    // The item displays a site's actual favicon or touch icon.
    ICON_REAL,
    // The item displays a color derived from the site's favicon or touch icon.
    ICON_COLOR,
    // The item displays a default gray box in place of an icon.
    ICON_DEFAULT,
    NUM_TILE_TYPES,
};

scoped_ptr<SkBitmap> MaybeFetchLocalThumbnail(
    const GURL& url,
    const scoped_refptr<TopSites>& top_sites) {
  DCHECK_CURRENTLY_ON(BrowserThread::DB);
  scoped_refptr<base::RefCountedMemory> image;
  scoped_ptr<SkBitmap> bitmap;
  if (top_sites && top_sites->GetPageThumbnail(url, false, &image))
    bitmap.reset(gfx::JPEGCodec::Decode(image->front(), image->size()));
  return bitmap;
}

// Log an event for a given |histogram| at a given element |position|. This
// routine exists because regular histogram macros are cached thus can't be used
// if the name of the histogram will change at a given call site.
void LogHistogramEvent(const std::string& histogram,
                       int position,
                       int num_sites) {
  base::HistogramBase* counter = base::LinearHistogram::FactoryGet(
      histogram,
      1,
      num_sites,
      num_sites + 1,
      base::Histogram::kUmaTargetedHistogramFlag);
  if (counter)
    counter->Add(position);
}

bool ShouldShowPopularSites() {
  // Note: It's important to query the field trial state first, to ensure that
  // UMA reports the correct group.
  const std::string group_name =
      base::FieldTrialList::FindFullName(kPopularSitesFieldTrialName);
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kDisableNTPPopularSites))
    return false;
  if (cmd_line->HasSwitch(switches::kEnableNTPPopularSites))
    return true;
  return base::StartsWith(group_name, "Enabled",
                          base::CompareCase::INSENSITIVE_ASCII);
}

std::string GetPopularSitesCountry() {
  return variations::GetVariationParamValue(kPopularSitesFieldTrialName,
                                            "country");
}

std::string GetPopularSitesVersion() {
  return variations::GetVariationParamValue(kPopularSitesFieldTrialName,
                                            "version");
}

// Determine whether we need any popular suggestions to fill up a grid of
// |num_tiles| tiles.
bool NeedPopularSites(const PrefService* prefs, size_t num_tiles) {
  const base::ListValue* source_list =
      prefs->GetList(prefs::kNTPSuggestionsIsPersonal);
  // If there aren't enough previous suggestions to fill the grid, we need
  // popular suggestions.
  if (source_list->GetSize() < num_tiles)
    return true;
  // Otherwise, if any of the previous suggestions is not personal, then also
  // get popular suggestions.
  for (size_t i = 0; i < num_tiles; ++i) {
    bool is_personal = false;
    if (source_list->GetBoolean(i, &is_personal) && !is_personal)
      return true;
  }
  // The whole grid is already filled with personal suggestions, no point in
  // bothering with popular ones.
  return false;
}

}  // namespace

MostVisitedSites::Suggestion::Suggestion(const base::string16& title,
                                         const std::string& url,
                                         MostVisitedSource source)
    : title(title), url(url), source(source), provider_index(-1) {}

MostVisitedSites::Suggestion::Suggestion(const base::string16& title,
                                         const GURL& url,
                                         MostVisitedSource source)
    : title(title), url(url), source(source), provider_index(-1) {}

MostVisitedSites::Suggestion::Suggestion(const base::string16& title,
                                         const std::string& url,
                                         MostVisitedSource source,
                                         int provider_index)
    : title(title), url(url), source(source), provider_index(provider_index) {
  DCHECK_EQ(MostVisitedSites::SUGGESTIONS_SERVICE, source);
}

MostVisitedSites::Suggestion::~Suggestion() {}

std::string MostVisitedSites::Suggestion::GetSourceHistogramName() const {
  switch (source) {
    case MostVisitedSites::TOP_SITES:
      return kHistogramClientName;
    case MostVisitedSites::POPULAR:
      return kHistogramPopularName;
    case MostVisitedSites::SUGGESTIONS_SERVICE:
      return provider_index >= 0
                 ? base::StringPrintf(kHistogramServerFormat, provider_index)
                 : kHistogramServerName;
  }
  NOTREACHED();
  return std::string();
}

MostVisitedSites::MostVisitedSites(Profile* profile)
    : profile_(profile), num_sites_(0), received_most_visited_sites_(false),
      received_popular_sites_(false), recorded_uma_(false),
      scoped_observer_(this), weak_ptr_factory_(this) {
  // Register the debugging page for the Suggestions Service and the thumbnails
  // debugging page.
  content::URLDataSource::Add(profile_,
                              new suggestions::SuggestionsSource(profile_));
  content::URLDataSource::Add(profile_, new ThumbnailListSource(profile_));

  // Register this class as an observer to the sync service. It is important to
  // be notified of changes in the sync state such as initialization, sync
  // being enabled or disabled, etc.
  ProfileSyncService* profile_sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  if (profile_sync_service)
    profile_sync_service->AddObserver(this);
}

MostVisitedSites::~MostVisitedSites() {
  ProfileSyncService* profile_sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  if (profile_sync_service && profile_sync_service->HasObserver(this))
    profile_sync_service->RemoveObserver(this);
}

void MostVisitedSites::Destroy(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  delete this;
}

void MostVisitedSites::SetMostVisitedURLsObserver(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_observer,
    jint num_sites) {
  observer_.Reset(env, j_observer);
  num_sites_ = num_sites;

  if (ShouldShowPopularSites() &&
      NeedPopularSites(profile_->GetPrefs(), num_sites_)) {
    popular_sites_.reset(new PopularSites(
        profile_,
        GetPopularSitesCountry(),
        GetPopularSitesVersion(),
        false,
        base::Bind(&MostVisitedSites::OnPopularSitesAvailable,
                   base::Unretained(this))));
  } else {
    received_popular_sites_ = true;
  }

  QueryMostVisitedURLs();

  scoped_refptr<history::TopSites> top_sites =
      TopSitesFactory::GetForProfile(profile_);
  if (top_sites) {
    // TopSites updates itself after a delay. To ensure up-to-date results,
    // force an update now.
    top_sites->SyncWithHistory();

    // Register as TopSitesObserver so that we can update ourselves when the
    // TopSites changes.
    scoped_observer_.Add(top_sites.get());
  }
}

void MostVisitedSites::GetURLThumbnail(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& j_url,
    const JavaParamRef<jobject>& j_callback_obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  scoped_ptr<ScopedJavaGlobalRef<jobject>> j_callback(
      new ScopedJavaGlobalRef<jobject>());
  j_callback->Reset(env, j_callback_obj);

  GURL url(ConvertJavaStringToUTF8(env, j_url));
  scoped_refptr<TopSites> top_sites(TopSitesFactory::GetForProfile(profile_));

  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::DB, FROM_HERE,
      base::Bind(&MaybeFetchLocalThumbnail, url, top_sites),
      base::Bind(&MostVisitedSites::OnLocalThumbnailFetched,
                 weak_ptr_factory_.GetWeakPtr(), url,
                 base::Passed(&j_callback)));
}

void MostVisitedSites::OnLocalThumbnailFetched(
    const GURL& url,
    scoped_ptr<ScopedJavaGlobalRef<jobject>> j_callback,
    scoped_ptr<SkBitmap> bitmap) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!bitmap.get()) {
    // A thumbnail is not locally available for |url|. Make sure it is put in
    // the list to be fetched at the next visit to this site.
    scoped_refptr<TopSites> top_sites(TopSitesFactory::GetForProfile(profile_));
    if (top_sites)
      top_sites->AddForcedURL(url, base::Time::Now());
    // Also fetch a remote thumbnail if possible. PopularSites or the
    // SuggestionsService can supply a thumbnail download URL.
    SuggestionsService* suggestions_service =
        SuggestionsServiceFactory::GetForProfile(profile_);
    if (suggestions_service) {
      if (popular_sites_) {
        const std::vector<PopularSites::Site>& sites = popular_sites_->sites();
        auto it = std::find_if(sites.begin(), sites.end(),
                               [&url](const PopularSites::Site& site) {
                                 return site.url == url;
                               });
        if (it != sites.end() && it->thumbnail_url.is_valid()) {
          return suggestions_service->GetPageThumbnailWithURL(
              url, it->thumbnail_url,
              base::Bind(&MostVisitedSites::OnObtainedThumbnail,
                         weak_ptr_factory_.GetWeakPtr(), false,
                         base::Passed(&j_callback)));
        }
      }
      if (mv_source_ == SUGGESTIONS_SERVICE) {
        return suggestions_service->GetPageThumbnail(
            url, base::Bind(&MostVisitedSites::OnObtainedThumbnail,
                            weak_ptr_factory_.GetWeakPtr(), false,
                            base::Passed(&j_callback)));
      }
    }
  }
  OnObtainedThumbnail(true, std::move(j_callback), url, bitmap.get());
}

void MostVisitedSites::OnObtainedThumbnail(
    bool is_local_thumbnail,
    scoped_ptr<ScopedJavaGlobalRef<jobject>> j_callback,
    const GURL& url,
    const SkBitmap* bitmap) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_bitmap;
  if (bitmap)
    j_bitmap = gfx::ConvertToJavaBitmap(bitmap);
  Java_ThumbnailCallback_onMostVisitedURLsThumbnailAvailable(
      env, j_callback->obj(), j_bitmap.obj(), is_local_thumbnail);
}

void MostVisitedSites::AddOrRemoveBlacklistedUrl(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& j_url,
    jboolean add_url) {
  GURL url(ConvertJavaStringToUTF8(env, j_url));

  // Always blacklist in the local TopSites.
  scoped_refptr<TopSites> top_sites = TopSitesFactory::GetForProfile(profile_);
  if (top_sites) {
    if (add_url) {
      top_sites->AddBlacklistedURL(url);
    } else {
      top_sites->RemoveBlacklistedURL(url);
    }
  }

  // Only blacklist in the server-side suggestions service if it's active.
  if (mv_source_ == SUGGESTIONS_SERVICE) {
    SuggestionsService* suggestions_service =
        SuggestionsServiceFactory::GetForProfile(profile_);
    DCHECK(suggestions_service);
    suggestions::SuggestionsService::ResponseCallback callback(
        base::Bind(&MostVisitedSites::OnSuggestionsProfileAvailable,
                   weak_ptr_factory_.GetWeakPtr()));
    if (add_url) {
      suggestions_service->BlacklistURL(url, callback, base::Closure());
    } else {
      suggestions_service->UndoBlacklistURL(url, callback, base::Closure());
    }
  }
}

void MostVisitedSites::RecordTileTypeMetrics(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jintArray>& jtile_types) {
  std::vector<int> tile_types;
  base::android::JavaIntArrayToIntVector(env, jtile_types, &tile_types);
  DCHECK_EQ(current_suggestions_.size(), tile_types.size());

  int counts_per_type[NUM_TILE_TYPES] = {0};
  for (size_t i = 0; i < tile_types.size(); ++i) {
    int tile_type = tile_types[i];
    ++counts_per_type[tile_type];
    std::string histogram = base::StringPrintf(
        "NewTabPage.TileType.%s",
        current_suggestions_[i]->GetSourceHistogramName().c_str());
    LogHistogramEvent(histogram, tile_type, NUM_TILE_TYPES);
  }

  UMA_HISTOGRAM_SPARSE_SLOWLY("NewTabPage.IconsReal",
                              counts_per_type[ICON_REAL]);
  UMA_HISTOGRAM_SPARSE_SLOWLY("NewTabPage.IconsColor",
                              counts_per_type[ICON_COLOR]);
  UMA_HISTOGRAM_SPARSE_SLOWLY("NewTabPage.IconsGray",
                              counts_per_type[ICON_DEFAULT]);
}

void MostVisitedSites::RecordOpenedMostVisitedItem(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint index,
    jint tile_type) {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, static_cast<int>(current_suggestions_.size()));
  std::string histogram = base::StringPrintf(
      "NewTabPage.MostVisited.%s",
      current_suggestions_[index]->GetSourceHistogramName().c_str());
  LogHistogramEvent(histogram, index, num_sites_);

  histogram = base::StringPrintf(
      "NewTabPage.TileTypeClicked.%s",
      current_suggestions_[index]->GetSourceHistogramName().c_str());
  LogHistogramEvent(histogram, tile_type, NUM_TILE_TYPES);
}

void MostVisitedSites::OnStateChanged() {
  // There have been changes to the sync state. This class cares about a few
  // (just initialized, enabled/disabled or history sync state changed). Re-run
  // the query code which will use the proper state.
  QueryMostVisitedURLs();
}

// static
bool MostVisitedSites::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
void MostVisitedSites::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kNTPSuggestionsURL);
  registry->RegisterListPref(prefs::kNTPSuggestionsIsPersonal);
}

void MostVisitedSites::QueryMostVisitedURLs() {
  SuggestionsService* suggestions_service =
      SuggestionsServiceFactory::GetForProfile(profile_);
  if (suggestions_service) {
    // Suggestions service is enabled, initiate a query.
    suggestions_service->FetchSuggestionsData(
        suggestions::GetSyncState(profile_),
        base::Bind(&MostVisitedSites::OnSuggestionsProfileAvailable,
                   weak_ptr_factory_.GetWeakPtr()));
  } else {
    InitiateTopSitesQuery();
  }
}

void MostVisitedSites::InitiateTopSitesQuery() {
  scoped_refptr<TopSites> top_sites = TopSitesFactory::GetForProfile(profile_);
  if (!top_sites)
    return;

  top_sites->GetMostVisitedURLs(
      base::Bind(&MostVisitedSites::OnMostVisitedURLsAvailable,
                 weak_ptr_factory_.GetWeakPtr()),
      false);
}

void MostVisitedSites::OnMostVisitedURLsAvailable(
    const history::MostVisitedURLList& visited_list) {
  ScopedVector<Suggestion> suggestions;
  size_t num_tiles =
      std::min(visited_list.size(), static_cast<size_t>(num_sites_));
  for (size_t i = 0; i < num_tiles; ++i) {
    const history::MostVisitedURL& visited = visited_list[i];
    if (visited.url.is_empty()) {
      num_tiles = i;
      break;  // This is the signal that there are no more real visited sites.
    }
    suggestions.push_back(
        new Suggestion(visited.title, visited.url.spec(), TOP_SITES));
  }

  received_most_visited_sites_ = true;
  mv_source_ = TOP_SITES;
  AddPopularSites(&suggestions);
  NotifyMostVisitedURLsObserver();
}

void MostVisitedSites::OnSuggestionsProfileAvailable(
    const SuggestionsProfile& suggestions_profile) {
  int num_tiles = suggestions_profile.suggestions_size();
  // With no server suggestions, fall back to local Most Visited.
  if (num_tiles == 0) {
    InitiateTopSitesQuery();
    return;
  }
  if (num_sites_ < num_tiles)
    num_tiles = num_sites_;

  ScopedVector<Suggestion> suggestions;
  for (int i = 0; i < num_tiles; ++i) {
    const ChromeSuggestion& suggestion = suggestions_profile.suggestions(i);
    suggestions.push_back(new Suggestion(
        base::UTF8ToUTF16(suggestion.title()), suggestion.url(),
        SUGGESTIONS_SERVICE,
        suggestion.providers_size() > 0 ? suggestion.providers(0) : -1));
  }

  received_most_visited_sites_ = true;
  mv_source_ = SUGGESTIONS_SERVICE;
  AddPopularSites(&suggestions);
  NotifyMostVisitedURLsObserver();
}

void MostVisitedSites::AddPopularSites(
    ScopedVector<Suggestion>* personal_suggestions) {
  size_t num_personal_suggestions = personal_suggestions->size();
  DCHECK_LE(num_personal_suggestions, static_cast<size_t>(num_sites_));

  // Collect non-blacklisted popular suggestions, skipping those already present
  // in the personal suggestions.
  size_t num_popular_suggestions = num_sites_ - num_personal_suggestions;
  ScopedVector<Suggestion> popular_suggestions;
  popular_suggestions.reserve(num_popular_suggestions);

  if (num_popular_suggestions > 0 && popular_sites_) {
    std::set<std::string> personal_hosts;
    for (const Suggestion* suggestion : *personal_suggestions)
      personal_hosts.insert(suggestion->url.host());
    scoped_refptr<TopSites> top_sites(TopSitesFactory::GetForProfile(profile_));
    for (const PopularSites::Site& popular_site : popular_sites_->sites()) {
      // Skip blacklisted sites.
      if (top_sites && top_sites->IsBlacklisted(popular_site.url))
        continue;
      std::string host = popular_site.url.host();
      // Skip suggestions already present in personal.
      if (personal_hosts.find(host) != personal_hosts.end())
        continue;

      popular_suggestions.push_back(
          new Suggestion(popular_site.title, popular_site.url, POPULAR));
      if (popular_suggestions.size() >= num_popular_suggestions)
        break;
    }
  }
  num_popular_suggestions = popular_suggestions.size();
  size_t num_actual_tiles = num_personal_suggestions + num_popular_suggestions;
  std::vector<std::string> old_sites_url;
  std::vector<bool> old_sites_is_personal;
  GetPreviousNTPSites(num_actual_tiles, &old_sites_url, &old_sites_is_personal);
  ScopedVector<Suggestion> merged_suggestions =
      MergeSuggestions(personal_suggestions, &popular_suggestions,
                       old_sites_url, old_sites_is_personal);
  DCHECK_EQ(num_actual_tiles, merged_suggestions.size());
  current_suggestions_.swap(merged_suggestions);
  if (received_popular_sites_)
    SaveCurrentNTPSites();
}

// static
ScopedVector<MostVisitedSites::Suggestion> MostVisitedSites::MergeSuggestions(
    ScopedVector<Suggestion>* personal_suggestions,
    ScopedVector<Suggestion>* popular_suggestions,
    const std::vector<std::string>& old_sites_url,
    const std::vector<bool>& old_sites_is_personal) {
  size_t num_personal_suggestions = personal_suggestions->size();
  size_t num_popular_suggestions = popular_suggestions->size();
  size_t num_tiles = num_popular_suggestions + num_personal_suggestions;
  ScopedVector<Suggestion> merged_suggestions;
  merged_suggestions.resize(num_tiles);

  size_t num_old_tiles = old_sites_url.size();
  DCHECK_LE(num_old_tiles, num_tiles);
  DCHECK_EQ(num_old_tiles, old_sites_is_personal.size());
  std::vector<std::string> old_sites_host;
  old_sites_host.reserve(num_old_tiles);
  // Only populate the hosts for popular suggestions as only they can be
  // replaced by host. Personal suggestions require an exact url match to be
  // replaced.
  for (size_t i = 0; i < num_old_tiles; ++i) {
    old_sites_host.push_back(old_sites_is_personal[i]
                                 ? std::string()
                                 : GURL(old_sites_url[i]).host());
  }

  // Insert personal suggestions if they existed previously.
  std::vector<size_t> new_personal_suggestions = InsertMatchingSuggestions(
      personal_suggestions, &merged_suggestions, old_sites_url, old_sites_host);
  // Insert popular suggestions if they existed previously.
  std::vector<size_t> new_popular_suggestions = InsertMatchingSuggestions(
      popular_suggestions, &merged_suggestions, old_sites_url, old_sites_host);
  // Insert leftover personal suggestions.
  size_t filled_so_far = InsertAllSuggestions(
      0, new_personal_suggestions, personal_suggestions, &merged_suggestions);
  // Insert leftover popular suggestions.
  InsertAllSuggestions(filled_so_far, new_popular_suggestions,
                       popular_suggestions, &merged_suggestions);
  return merged_suggestions;
}

void MostVisitedSites::GetPreviousNTPSites(
    size_t num_tiles,
    std::vector<std::string>* old_sites_url,
    std::vector<bool>* old_sites_is_personal) const {
  const PrefService* prefs = profile_->GetPrefs();
  const base::ListValue* url_list = prefs->GetList(prefs::kNTPSuggestionsURL);
  const base::ListValue* source_list =
      prefs->GetList(prefs::kNTPSuggestionsIsPersonal);
  DCHECK_EQ(url_list->GetSize(), source_list->GetSize());
  if (url_list->GetSize() < num_tiles)
    num_tiles = url_list->GetSize();
  if (num_tiles == 0) {
    // No fallback required as Personal suggestions take precedence anyway.
    return;
  }
  old_sites_url->reserve(num_tiles);
  old_sites_is_personal->reserve(num_tiles);
  for (size_t i = 0; i < num_tiles; ++i) {
    std::string url_string;
    bool success = url_list->GetString(i, &url_string);
    DCHECK(success);
    old_sites_url->push_back(url_string);
    bool is_personal;
    success = source_list->GetBoolean(i, &is_personal);
    DCHECK(success);
    old_sites_is_personal->push_back(is_personal);
  }
}

void MostVisitedSites::SaveCurrentNTPSites() {
  base::ListValue url_list;
  base::ListValue source_list;
  for (const Suggestion* suggestion : current_suggestions_) {
    url_list.AppendString(suggestion->url.spec());
    source_list.AppendBoolean(suggestion->source != MostVisitedSites::POPULAR);
  }
  PrefService* prefs = profile_->GetPrefs();
  prefs->Set(prefs::kNTPSuggestionsIsPersonal, source_list);
  prefs->Set(prefs::kNTPSuggestionsURL, url_list);
}

// static
std::vector<size_t> MostVisitedSites::InsertMatchingSuggestions(
    ScopedVector<Suggestion>* src_suggestions,
    ScopedVector<Suggestion>* dst_suggestions,
    const std::vector<std::string>& match_urls,
    const std::vector<std::string>& match_hosts) {
  std::vector<size_t> unmatched_suggestions;
  size_t num_src_suggestions = src_suggestions->size();
  size_t num_matchers = match_urls.size();
  for (size_t i = 0; i < num_src_suggestions; ++i) {
    size_t position;
    for (position = 0; position < num_matchers; ++position) {
      if ((*dst_suggestions)[position] != nullptr)
        continue;
      if (match_urls[position] == (*src_suggestions)[i]->url.spec())
        break;
      // match_hosts is only populated for suggestions which can be replaced by
      // host matching like popular suggestions.
      if (match_hosts[position] == (*src_suggestions)[i]->url.host())
        break;
    }
    if (position == num_matchers) {
      unmatched_suggestions.push_back(i);
    } else {
      // A move is required as the source and destination containers own the
      // elements.
      std::swap((*dst_suggestions)[position], (*src_suggestions)[i]);
    }
  }
  return unmatched_suggestions;
}

// static
size_t MostVisitedSites::InsertAllSuggestions(
    size_t start_position,
    const std::vector<size_t>& insert_positions,
    ScopedVector<Suggestion>* src_suggestions,
    ScopedVector<Suggestion>* dst_suggestions) {
  size_t num_inserts = insert_positions.size();
  size_t num_dests = dst_suggestions->size();

  size_t src_pos = 0;
  size_t i = start_position;
  for (; i < num_dests && src_pos < num_inserts; ++i) {
    if ((*dst_suggestions)[i] != nullptr)
      continue;
    size_t src = insert_positions[src_pos++];
    std::swap((*dst_suggestions)[i], (*src_suggestions)[src]);
  }
  // Return destination postions filled so far which becomes the start_position
  // for future runs.
  return i;
}

void MostVisitedSites::NotifyMostVisitedURLsObserver() {
  size_t num_suggestions = current_suggestions_.size();
  if (received_most_visited_sites_ && received_popular_sites_ &&
      !recorded_uma_) {
    RecordImpressionUMAMetrics();
    UMA_HISTOGRAM_SPARSE_SLOWLY("NewTabPage.NumberOfTiles", num_suggestions);
    recorded_uma_ = true;
  }
  std::vector<base::string16> titles;
  std::vector<std::string> urls;
  titles.reserve(num_suggestions);
  urls.reserve(num_suggestions);
  for (const Suggestion* suggestion : current_suggestions_) {
    titles.push_back(suggestion->title);
    urls.push_back(suggestion->url.spec());
  }
  JNIEnv* env = AttachCurrentThread();
  DCHECK_EQ(titles.size(), urls.size());
  Java_MostVisitedURLsObserver_onMostVisitedURLsAvailable(
      env, observer_.obj(), ToJavaArrayOfStrings(env, titles).obj(),
      ToJavaArrayOfStrings(env, urls).obj());
}

void MostVisitedSites::OnPopularSitesAvailable(bool success) {
  received_popular_sites_ = true;

  if (!success) {
    LOG(WARNING) << "Download of popular sites failed";
    return;
  }

  if (observer_.is_null())
    return;

  std::vector<std::string> urls;
  std::vector<std::string> favicon_urls;
  std::vector<std::string> large_icon_urls;
  for (const PopularSites::Site& popular_site : popular_sites_->sites()) {
    urls.push_back(popular_site.url.spec());
    favicon_urls.push_back(popular_site.favicon_url.spec());
    large_icon_urls.push_back(popular_site.large_icon_url.spec());
  }
  JNIEnv* env = AttachCurrentThread();
  Java_MostVisitedURLsObserver_onPopularURLsAvailable(
      env, observer_.obj(), ToJavaArrayOfStrings(env, urls).obj(),
      ToJavaArrayOfStrings(env, favicon_urls).obj(),
      ToJavaArrayOfStrings(env, large_icon_urls).obj());
  QueryMostVisitedURLs();
}

void MostVisitedSites::RecordImpressionUMAMetrics() {
  for (size_t i = 0; i < current_suggestions_.size(); i++) {
    std::string histogram = base::StringPrintf(
        "NewTabPage.SuggestionsImpression.%s",
        current_suggestions_[i]->GetSourceHistogramName().c_str());
    LogHistogramEvent(histogram, static_cast<int>(i), num_sites_);
  }
}

void MostVisitedSites::TopSitesLoaded(history::TopSites* top_sites) {
}

void MostVisitedSites::TopSitesChanged(history::TopSites* top_sites,
                                       ChangeReason change_reason) {
  if (mv_source_ == TOP_SITES) {
    // The displayed suggestions are invalidated.
    InitiateTopSitesQuery();
  }
}

static jlong Init(JNIEnv* env,
                  const JavaParamRef<jobject>& obj,
                  const JavaParamRef<jobject>& jprofile) {
  MostVisitedSites* most_visited_sites =
      new MostVisitedSites(ProfileAndroid::FromProfileAndroid(jprofile));
  return reinterpret_cast<intptr_t>(most_visited_sites);
}
