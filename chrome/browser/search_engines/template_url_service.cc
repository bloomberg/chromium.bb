// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/template_url_service.h"

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/environment.h"
#include "base/guid.h"
#include "base/i18n/case_conversion.h"
#include "base/memory/scoped_vector.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/google/google_url_tracker.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/rlz/rlz.h"
#include "chrome/browser/search_engines/search_host_to_urls_map.h"
#include "chrome/browser/search_engines/search_terms_data.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_prepopulate_data.h"
#include "chrome/browser/search_engines/template_url_service_observer.h"
#include "chrome/browser/search_engines/util.h"
#include "chrome/browser/webdata/web_data_service_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/env_vars.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/notification_service.h"
#include "extensions/common/constants.h"
#include "net/base/net_util.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_error_factory.h"
#include "sync/protocol/search_engine_specifics.pb.h"
#include "sync/protocol/sync.pb.h"
#include "ui/base/l10n/l10n_util.h"

typedef SearchHostToURLsMap::TemplateURLSet TemplateURLSet;
typedef TemplateURLService::SyncDataMap SyncDataMap;

namespace {

// String in the URL that is replaced by the search term.
const char kSearchTermParameter[] = "{searchTerms}";

// String in Initializer that is replaced with kSearchTermParameter.
const char kTemplateParameter[] = "%s";

// Term used when generating a search url. Use something obscure so that on
// the rare case the term replaces the URL it's unlikely another keyword would
// have the same url.
const char kReplacementTerm[] = "blah.blah.blah.blah.blah";

// The name of the histogram used to track default search changes. See the
// comment for DefaultSearchChangeOrigin.
const char kDSPChangeHistogramName[] = "Search.DefaultSearchChangeOrigin";

// The name of the histogram used to track whether or not the user has a default
// search provider.
const char kHasDSPHistogramName[] = "Search.HasDefaultSearchProvider";

// The name of the histogram used to store the id of the default search
// provider.
const char kDSPHistogramName[] = "Search.DefaultSearchProvider";

bool TemplateURLsHaveSamePrefs(const TemplateURL* url1,
                               const TemplateURL* url2) {
  if (url1 == url2)
    return true;
  return (url1 != NULL) && (url2 != NULL) &&
      (url1->short_name() == url2->short_name()) &&
      url1->HasSameKeywordAs(*url2) &&
      (url1->url() == url2->url()) &&
      (url1->suggestions_url() == url2->suggestions_url()) &&
      (url1->instant_url() == url2->instant_url()) &&
      (url1->favicon_url() == url2->favicon_url()) &&
      (url1->safe_for_autoreplace() == url2->safe_for_autoreplace()) &&
      (url1->show_in_default_list() == url2->show_in_default_list()) &&
      (url1->input_encodings() == url2->input_encodings()) &&
      (url1->alternate_urls() == url2->alternate_urls()) &&
      (url1->search_terms_replacement_key() ==
          url2->search_terms_replacement_key());
}

const char kFirstPotentialEngineHistogramName[] =
    "Search.FirstPotentialEngineCalled";

// Values for an enumerated histogram used to track whenever
// FirstPotentialDefaultEngine is called, and from where.
enum FirstPotentialEngineCaller {
  FIRST_POTENTIAL_CALLSITE_FIND_NEW_DSP,
  FIRST_POTENTIAL_CALLSITE_FIND_NEW_DSP_PROCESSING_SYNC_CHANGES,
  FIRST_POTENTIAL_CALLSITE_ON_LOAD,
  FIRST_POTENTIAL_CALLSITE_FIND_NEW_DSP_SYNCING,
  FIRST_POTENTIAL_CALLSITE_FIND_NEW_DSP_NOT_SYNCING,
  FIRST_POTENTIAL_CALLSITE_MAX,
};

const char kDeleteSyncedEngineHistogramName[] =
    "Search.DeleteSyncedSearchEngine";

// Values for an enumerated histogram used to track whenever an ACTION_DELETE is
// sent to the server for search engines.
enum DeleteSyncedSearchEngineEvent {
  DELETE_ENGINE_USER_ACTION,
  DELETE_ENGINE_PRE_SYNC,
  DELETE_ENGINE_EMPTY_FIELD,
  DELETE_ENGINE_MAX,
};

TemplateURL* FirstPotentialDefaultEngine(
    const TemplateURLService::TemplateURLVector& template_urls) {
  for (TemplateURLService::TemplateURLVector::const_iterator i(
       template_urls.begin()); i != template_urls.end(); ++i) {
    if ((*i)->ShowInDefaultList()) {
      DCHECK(!(*i)->IsExtensionKeyword());
      return *i;
    }
  }
  return NULL;
}

// Returns true iff the change in |change_list| at index |i| should not be sent
// up to the server based on its GUIDs presence in |sync_data| or when compared
// to changes after it in |change_list|.
// The criteria is:
//  1) It is an ACTION_UPDATE or ACTION_DELETE and the sync_guid associated
//     with it is NOT found in |sync_data|. We can only update and remove
//     entries that were originally from the Sync server.
//  2) It is an ACTION_ADD and the sync_guid associated with it is found in
//     |sync_data|. We cannot re-add entries that Sync already knew about.
//  3) There is an update after an update for the same GUID. We prune earlier
//     ones just to save bandwidth (Sync would normally coalesce them).
bool ShouldRemoveSyncChange(size_t index,
                            syncer::SyncChangeList* change_list,
                            const SyncDataMap* sync_data) {
  DCHECK(index < change_list->size());
  const syncer::SyncChange& change_i = (*change_list)[index];
  const std::string guid = change_i.sync_data().GetSpecifics()
      .search_engine().sync_guid();
  syncer::SyncChange::SyncChangeType type = change_i.change_type();
  if ((type == syncer::SyncChange::ACTION_UPDATE ||
       type == syncer::SyncChange::ACTION_DELETE) &&
       sync_data->find(guid) == sync_data->end())
    return true;
  if (type == syncer::SyncChange::ACTION_ADD &&
      sync_data->find(guid) != sync_data->end())
    return true;
  if (type == syncer::SyncChange::ACTION_UPDATE) {
    for (size_t j = index + 1; j < change_list->size(); j++) {
      const syncer::SyncChange& change_j = (*change_list)[j];
      if ((syncer::SyncChange::ACTION_UPDATE == change_j.change_type()) &&
          (change_j.sync_data().GetSpecifics().search_engine().sync_guid() ==
              guid))
        return true;
    }
  }
  return false;
}

// Remove SyncChanges that should not be sent to the server from |change_list|.
// This is done to eliminate incorrect SyncChanges added by the merge and
// conflict resolution logic when it is unsure of whether or not an entry is new
// from Sync or originally from the local model. This also removes changes that
// would be otherwise be coalesced by Sync in order to save bandwidth.
void PruneSyncChanges(const SyncDataMap* sync_data,
                      syncer::SyncChangeList* change_list) {
  for (size_t i = 0; i < change_list->size(); ) {
    if (ShouldRemoveSyncChange(i, change_list, sync_data))
      change_list->erase(change_list->begin() + i);
    else
      ++i;
  }
}

// Helper class that reports a specific string as the Google base URL.  We use
// this so that code can calculate a TemplateURL's previous search URL after a
// change to the global base URL.
class OldBaseURLSearchTermsData : public UIThreadSearchTermsData {
 public:
  OldBaseURLSearchTermsData(Profile* profile, const std::string& old_base_url);
  virtual ~OldBaseURLSearchTermsData();

  virtual std::string GoogleBaseURLValue() const OVERRIDE;

 private:
  std::string old_base_url;
};

OldBaseURLSearchTermsData::OldBaseURLSearchTermsData(
    Profile* profile,
    const std::string& old_base_url)
    : UIThreadSearchTermsData(profile),
      old_base_url(old_base_url) {
}

OldBaseURLSearchTermsData::~OldBaseURLSearchTermsData() {
}

std::string OldBaseURLSearchTermsData::GoogleBaseURLValue() const {
  return old_base_url;
}

// Returns true if |turl|'s GUID is not found inside |sync_data|. This is to be
// used in MergeDataAndStartSyncing to differentiate between TemplateURLs from
// Sync and TemplateURLs that were initially local, assuming |sync_data| is the
// |initial_sync_data| parameter.
bool IsFromSync(const TemplateURL* turl, const SyncDataMap& sync_data) {
  return !!sync_data.count(turl->sync_guid());
}

// Log the number of instances of a keyword that exist, with zero or more
// underscores, which could occur as the result of conflict resolution.
void LogDuplicatesHistogram(
    const TemplateURLService::TemplateURLVector& template_urls) {
  std::map<std::string, int> duplicates;
  for (TemplateURLService::TemplateURLVector::const_iterator it =
      template_urls.begin(); it != template_urls.end(); ++it) {
    std::string keyword = UTF16ToASCII((*it)->keyword());
    TrimString(keyword, "_", &keyword);
    duplicates[keyword]++;
  }

  // Count the keywords with duplicates.
  int num_dupes = 0;
  for (std::map<std::string, int>::const_iterator it = duplicates.begin();
      it != duplicates.end(); ++it) {
    if (it->second > 1)
      num_dupes++;
  }

  UMA_HISTOGRAM_COUNTS_100("Search.SearchEngineDuplicateCounts", num_dupes);
}

}  // namespace

class TemplateURLService::LessWithPrefix {
 public:
  // We want to find the set of keywords that begin with a prefix.  The STL
  // algorithms will return the set of elements that are "equal to" the
  // prefix, where "equal(x, y)" means "!(cmp(x, y) || cmp(y, x))".  When
  // cmp() is the typical std::less<>, this results in lexicographic equality;
  // we need to extend this to mark a prefix as "not less than" a keyword it
  // begins, which will cause the desired elements to be considered "equal to"
  // the prefix.  Note: this is still a strict weak ordering, as required by
  // equal_range() (though I will not prove that here).
  //
  // Unfortunately the calling convention is not "prefix and element" but
  // rather "two elements", so we pass the prefix as a fake "element" which has
  // a NULL KeywordDataElement pointer.
  bool operator()(const KeywordToTemplateMap::value_type& elem1,
                  const KeywordToTemplateMap::value_type& elem2) const {
    return (elem1.second == NULL) ?
        (elem2.first.compare(0, elem1.first.length(), elem1.first) > 0) :
        (elem1.first < elem2.first);
  }
};

TemplateURLService::TemplateURLService(Profile* profile)
    : provider_map_(new SearchHostToURLsMap),
      profile_(profile),
      loaded_(false),
      load_failed_(false),
      load_handle_(0),
      default_search_provider_(NULL),
      is_default_search_managed_(false),
      next_id_(kInvalidTemplateURLID + 1),
      time_provider_(&base::Time::Now),
      models_associated_(false),
      processing_syncer_changes_(false),
      pending_synced_default_search_(false),
      dsp_change_origin_(DSP_CHANGE_NOT_SYNC) {
  DCHECK(profile_);
  Init(NULL, 0);
}

TemplateURLService::TemplateURLService(const Initializer* initializers,
                                       const int count)
    : provider_map_(new SearchHostToURLsMap),
      profile_(NULL),
      loaded_(false),
      load_failed_(false),
      load_handle_(0),
      service_(NULL),
      default_search_provider_(NULL),
      is_default_search_managed_(false),
      next_id_(kInvalidTemplateURLID + 1),
      time_provider_(&base::Time::Now),
      models_associated_(false),
      processing_syncer_changes_(false),
      pending_synced_default_search_(false),
      dsp_change_origin_(DSP_CHANGE_NOT_SYNC) {
  Init(initializers, count);
}

TemplateURLService::~TemplateURLService() {
  if (load_handle_) {
    DCHECK(service_.get());
    service_->CancelRequest(load_handle_);
  }

  STLDeleteElements(&template_urls_);
}

// static
string16 TemplateURLService::GenerateKeyword(const GURL& url) {
  DCHECK(url.is_valid());
  // Strip "www." off the front of the keyword; otherwise the keyword won't work
  // properly.  See http://code.google.com/p/chromium/issues/detail?id=6984 .
  // Special case: if the host was exactly "www." (not sure this can happen but
  // perhaps with some weird intranet and custom DNS server?), ensure we at
  // least don't return the empty string.
  string16 keyword(net::StripWWWFromHost(url));
  return keyword.empty() ? ASCIIToUTF16("www") : keyword;
}

// static
string16 TemplateURLService::CleanUserInputKeyword(const string16& keyword) {
  // Remove the scheme.
  string16 result(base::i18n::ToLower(keyword));
  TrimWhitespace(result, TRIM_ALL, &result);
  url_parse::Component scheme_component;
  if (url_parse::ExtractScheme(UTF16ToUTF8(keyword).c_str(),
                               static_cast<int>(keyword.length()),
                               &scheme_component)) {
    // If the scheme isn't "http" or "https", bail.  The user isn't trying to
    // type a web address, but rather an FTP, file:, or other scheme URL, or a
    // search query with some sort of initial operator (e.g. "site:").
    if (result.compare(0, scheme_component.end(),
                       ASCIIToUTF16(chrome::kHttpScheme)) &&
        result.compare(0, scheme_component.end(),
                       ASCIIToUTF16(chrome::kHttpsScheme)))
      return string16();

    // Include trailing ':'.
    result.erase(0, scheme_component.end() + 1);
    // Many schemes usually have "//" after them, so strip it too.
    const string16 after_scheme(ASCIIToUTF16("//"));
    if (result.compare(0, after_scheme.length(), after_scheme) == 0)
      result.erase(0, after_scheme.length());
  }

  // Remove leading "www.".
  result = net::StripWWW(result);

  // Remove trailing "/".
  return (result.length() > 0 && result[result.length() - 1] == '/') ?
      result.substr(0, result.length() - 1) : result;
}

// static
GURL TemplateURLService::GenerateSearchURL(TemplateURL* t_url) {
  DCHECK(t_url);
  UIThreadSearchTermsData search_terms_data(t_url->profile());
  return GenerateSearchURLUsingTermsData(t_url, search_terms_data);
}

// static
GURL TemplateURLService::GenerateSearchURLUsingTermsData(
    const TemplateURL* t_url,
    const SearchTermsData& search_terms_data) {
  DCHECK(t_url);
  DCHECK(!t_url->IsExtensionKeyword());

  const TemplateURLRef& search_ref = t_url->url_ref();
  if (!search_ref.IsValidUsingTermsData(search_terms_data))
    return GURL();

  if (!search_ref.SupportsReplacementUsingTermsData(search_terms_data))
    return GURL(t_url->url());

  return GURL(search_ref.ReplaceSearchTermsUsingTermsData(
      TemplateURLRef::SearchTermsArgs(ASCIIToUTF16(kReplacementTerm)),
      search_terms_data));
}

bool TemplateURLService::CanReplaceKeyword(
    const string16& keyword,
    const GURL& url,
    TemplateURL** template_url_to_replace) {
  DCHECK(!keyword.empty());  // This should only be called for non-empty
                             // keywords. If we need to support empty kewords
                             // the code needs to change slightly.
  TemplateURL* existing_url = GetTemplateURLForKeyword(keyword);
  if (template_url_to_replace)
    *template_url_to_replace = existing_url;
  if (existing_url) {
    // We already have a TemplateURL for this keyword. Only allow it to be
    // replaced if the TemplateURL can be replaced.
    return CanReplace(existing_url);
  }

  // We don't have a TemplateURL with keyword. Only allow a new one if there
  // isn't a TemplateURL for the specified host, or there is one but it can
  // be replaced. We do this to ensure that if the user assigns a different
  // keyword to a generated TemplateURL, we won't regenerate another keyword for
  // the same host.
  return !url.is_valid() || url.host().empty() ||
      CanReplaceKeywordForHost(url.host(), template_url_to_replace);
}

void TemplateURLService::FindMatchingKeywords(
    const string16& prefix,
    bool support_replacement_only,
    std::vector<string16>* matches) const {
  // Sanity check args.
  if (prefix.empty())
    return;
  DCHECK(matches != NULL);
  DCHECK(matches->empty());  // The code for exact matches assumes this.

  // Required for VS2010: http://connect.microsoft.com/VisualStudio/feedback/details/520043/error-converting-from-null-to-a-pointer-type-in-std-pair
  TemplateURL* const kNullTemplateURL = NULL;

  // Find matching keyword range.  Searches the element map for keywords
  // beginning with |prefix| and stores the endpoints of the resulting set in
  // |match_range|.
  const std::pair<KeywordToTemplateMap::const_iterator,
                  KeywordToTemplateMap::const_iterator> match_range(
      std::equal_range(
          keyword_to_template_map_.begin(), keyword_to_template_map_.end(),
          KeywordToTemplateMap::value_type(prefix, kNullTemplateURL),
          LessWithPrefix()));

  // Return vector of matching keywords.
  for (KeywordToTemplateMap::const_iterator i(match_range.first);
       i != match_range.second; ++i) {
    if (!support_replacement_only || i->second->url_ref().SupportsReplacement())
      matches->push_back(i->first);
  }
}

TemplateURL* TemplateURLService::GetTemplateURLForKeyword(
    const string16& keyword) {
  KeywordToTemplateMap::const_iterator elem(
      keyword_to_template_map_.find(keyword));
  if (elem != keyword_to_template_map_.end())
    return elem->second;
  return ((!loaded_ || load_failed_) &&
      initial_default_search_provider_.get() &&
      (initial_default_search_provider_->keyword() == keyword)) ?
      initial_default_search_provider_.get() : NULL;
}

TemplateURL* TemplateURLService::GetTemplateURLForGUID(
    const std::string& sync_guid) {
  GUIDToTemplateMap::const_iterator elem(guid_to_template_map_.find(sync_guid));
  if (elem != guid_to_template_map_.end())
    return elem->second;
  return ((!loaded_ || load_failed_) &&
      initial_default_search_provider_.get() &&
      (initial_default_search_provider_->sync_guid() == sync_guid)) ?
      initial_default_search_provider_.get() : NULL;
}

TemplateURL* TemplateURLService::GetTemplateURLForHost(
    const std::string& host) {
  if (loaded_) {
    TemplateURL* t_url = provider_map_->GetTemplateURLForHost(host);
    if (t_url)
      return t_url;
  }
  return ((!loaded_ || load_failed_) &&
      initial_default_search_provider_.get() &&
      (GenerateSearchURL(initial_default_search_provider_.get()).host() ==
          host)) ? initial_default_search_provider_.get() : NULL;
}

void TemplateURLService::Add(TemplateURL* template_url) {
  if (AddNoNotify(template_url, true))
    NotifyObservers();
}

void TemplateURLService::AddAndSetProfile(TemplateURL* template_url,
                                          Profile* profile) {
  template_url->profile_ = profile;
  Add(template_url);
}

void TemplateURLService::AddWithOverrides(TemplateURL* template_url,
                                          const string16& short_name,
                                          const string16& keyword,
                                          const std::string& url) {
  DCHECK(!keyword.empty());
  DCHECK(!url.empty());
  template_url->data_.short_name = short_name;
  template_url->data_.SetKeyword(keyword);
  template_url->SetURL(url);
  Add(template_url);
}

void TemplateURLService::Remove(TemplateURL* template_url) {
  RemoveNoNotify(template_url);
  NotifyObservers();
}

void TemplateURLService::RemoveAutoGeneratedSince(base::Time created_after) {
  RemoveAutoGeneratedBetween(created_after, base::Time());
}

void TemplateURLService::RemoveAutoGeneratedBetween(base::Time created_after,
                                                    base::Time created_before) {
  RemoveAutoGeneratedForOriginBetween(GURL(), created_after, created_before);
}

void TemplateURLService::RemoveAutoGeneratedForOriginBetween(
    const GURL& origin,
    base::Time created_after,
    base::Time created_before) {
  GURL o(origin.GetOrigin());
  bool should_notify = false;
  for (size_t i = 0; i < template_urls_.size();) {
    if (template_urls_[i]->date_created() >= created_after &&
        (created_before.is_null() ||
         template_urls_[i]->date_created() < created_before) &&
        CanReplace(template_urls_[i]) &&
        (o.is_empty() ||
         GenerateSearchURL(template_urls_[i]).GetOrigin() == o)) {
      RemoveNoNotify(template_urls_[i]);
      should_notify = true;
    } else {
      ++i;
    }
  }
  if (should_notify)
    NotifyObservers();
}


void TemplateURLService::RegisterExtensionKeyword(
    const std::string& extension_id,
    const std::string& extension_name,
    const std::string& keyword) {
  DCHECK(loaded_);

  if (!GetTemplateURLForExtension(extension_id)) {
    TemplateURLData data;
    data.short_name = UTF8ToUTF16(extension_name);
    data.SetKeyword(UTF8ToUTF16(keyword));
    // This URL is not actually used for navigation. It holds the extension's
    // ID, as well as forcing the TemplateURL to be treated as a search keyword.
    data.SetURL(std::string(extensions::kExtensionScheme) + "://" +
        extension_id + "/?q={searchTerms}");
    Add(new TemplateURL(profile_, data));
  }
}

void TemplateURLService::UnregisterExtensionKeyword(
    const std::string& extension_id) {
  DCHECK(loaded_);
  TemplateURL* url = GetTemplateURLForExtension(extension_id);
  if (url)
    Remove(url);
}

TemplateURL* TemplateURLService::GetTemplateURLForExtension(
    const std::string& extension_id) {
  for (TemplateURLVector::const_iterator i = template_urls_.begin();
       i != template_urls_.end(); ++i) {
    if ((*i)->IsExtensionKeyword() &&
        ((*i)->url_ref().GetHost() == extension_id))
      return *i;
  }

  return NULL;
}

TemplateURLService::TemplateURLVector TemplateURLService::GetTemplateURLs() {
  return template_urls_;
}

void TemplateURLService::IncrementUsageCount(TemplateURL* url) {
  DCHECK(url);
  if (std::find(template_urls_.begin(), template_urls_.end(), url) ==
      template_urls_.end())
    return;
  ++url->data_.usage_count;
  // Extension keywords are not persisted.
  // TODO(mpcomplete): If we allow editing extension keywords, then those should
  // be persisted to disk and synced.
  if (service_.get() && !url->IsExtensionKeyword())
    service_.get()->UpdateKeyword(url->data());
}

void TemplateURLService::ResetTemplateURL(TemplateURL* url,
                                          const string16& title,
                                          const string16& keyword,
                                          const std::string& search_url) {
  if (!loaded_)
    return;
  DCHECK(!keyword.empty());
  DCHECK(!search_url.empty());
  TemplateURLData data(url->data());
  data.short_name = title;
  data.SetKeyword(keyword);
  if (search_url != data.url()) {
    data.SetURL(search_url);
    // The urls have changed, reset the favicon url.
    data.favicon_url = GURL();
  }
  data.safe_for_autoreplace = false;
  data.last_modified = time_provider_();
  TemplateURL new_url(url->profile(), data);
  UIThreadSearchTermsData search_terms_data(url->profile());
  if (UpdateNoNotify(url, new_url, search_terms_data))
    NotifyObservers();
}

bool TemplateURLService::CanMakeDefault(const TemplateURL* url) {
  return url != GetDefaultSearchProvider() &&
      url->url_ref().SupportsReplacement() && !is_default_search_managed();
}

void TemplateURLService::SetDefaultSearchProvider(TemplateURL* url) {
  DCHECK(!is_default_search_managed_);
  // Extension keywords cannot be made default, as they are inherently async.
  DCHECK(!url || !url->IsExtensionKeyword());

  // Always persist the setting in the database, that way if the backup
  // signature has changed out from under us it gets reset correctly.
  if (SetDefaultSearchProviderNoNotify(url))
    NotifyObservers();
}

TemplateURL* TemplateURLService::GetDefaultSearchProvider() {
  if (loaded_ && !load_failed_)
    return default_search_provider_;
  // We're not loaded, rely on the default search provider stored in prefs.
  return initial_default_search_provider_.get();
}

TemplateURL* TemplateURLService::FindNewDefaultSearchProvider() {
  // See if the prepopulated default still exists.
  scoped_ptr<TemplateURL> prepopulated_default(
      TemplateURLPrepopulateData::GetPrepopulatedDefaultSearch(profile_));
  for (TemplateURLVector::iterator i = template_urls_.begin();
       i != template_urls_.end(); ++i) {
    if ((*i)->prepopulate_id() == prepopulated_default->prepopulate_id())
      return *i;
  }
  // If not, use the first non-extension keyword of the templates that supports
  // search term replacement.
  if (processing_syncer_changes_) {
    UMA_HISTOGRAM_ENUMERATION(
        kFirstPotentialEngineHistogramName,
        FIRST_POTENTIAL_CALLSITE_FIND_NEW_DSP_PROCESSING_SYNC_CHANGES,
        FIRST_POTENTIAL_CALLSITE_MAX);
  } else {
    if (sync_processor_.get()) {
      // We're not currently in a sync cycle, but we're syncing.
      UMA_HISTOGRAM_ENUMERATION(kFirstPotentialEngineHistogramName,
                                FIRST_POTENTIAL_CALLSITE_FIND_NEW_DSP_SYNCING,
                                FIRST_POTENTIAL_CALLSITE_MAX);
    } else {
      // We're not syncing at all.
      UMA_HISTOGRAM_ENUMERATION(
          kFirstPotentialEngineHistogramName,
          FIRST_POTENTIAL_CALLSITE_FIND_NEW_DSP_NOT_SYNCING,
          FIRST_POTENTIAL_CALLSITE_MAX);
    }
  }
  return FirstPotentialDefaultEngine(template_urls_);
}

void TemplateURLService::AddObserver(TemplateURLServiceObserver* observer) {
  model_observers_.AddObserver(observer);
}

void TemplateURLService::RemoveObserver(TemplateURLServiceObserver* observer) {
  model_observers_.RemoveObserver(observer);
}

void TemplateURLService::Load() {
  if (loaded_ || load_handle_)
    return;

  if (!service_.get()) {
    service_ = WebDataServiceFactory::GetForProfile(profile_,
                                                    Profile::EXPLICIT_ACCESS);
  }

  if (service_.get()) {
    load_handle_ = service_->GetKeywords(this);
  } else {
    ChangeToLoadedState();
    NotifyLoaded();
  }
}

void TemplateURLService::OnWebDataServiceRequestDone(
    WebDataService::Handle h,
    const WDTypedResult* result) {
  // Reset the load_handle so that we don't try and cancel the load in
  // the destructor.
  load_handle_ = 0;

  if (!result) {
    // Results are null if the database went away or (most likely) wasn't
    // loaded.
    load_failed_ = true;
    ChangeToLoadedState();
    NotifyLoaded();
    return;
  }

  // initial_default_search_provider_ is only needed before we've finished
  // loading. Now that we've loaded we can nuke it.
  initial_default_search_provider_.reset();
  is_default_search_managed_ = false;

  TemplateURLVector template_urls;
  TemplateURL* default_search_provider = NULL;
  int new_resource_keyword_version = 0;
  GetSearchProvidersUsingKeywordResult(*result, service_.get(), profile_,
      &template_urls, &default_search_provider, &new_resource_keyword_version,
      &pre_sync_deletes_);

  bool database_specified_a_default = (default_search_provider != NULL);

  // Check if default search provider is now managed.
  scoped_ptr<TemplateURL> default_from_prefs;
  LoadDefaultSearchProviderFromPrefs(&default_from_prefs,
                                     &is_default_search_managed_);

  // Remove entries that were created because of policy as they may have
  // changed since the database was saved.
  RemoveProvidersCreatedByPolicy(&template_urls,
                                 &default_search_provider,
                                 default_from_prefs.get());

  PatchMissingSyncGUIDs(&template_urls);

  if (is_default_search_managed_) {
    SetTemplateURLs(template_urls);

    if (TemplateURLsHaveSamePrefs(default_search_provider,
                                  default_from_prefs.get())) {
      // The value from the preferences was previously stored in the database.
      // Reuse it.
    } else {
      // The value from the preferences takes over.
      default_search_provider = NULL;
      if (default_from_prefs.get()) {
        TemplateURLData data(default_from_prefs->data());
        data.created_by_policy = true;
        data.id = kInvalidTemplateURLID;
        default_search_provider = new TemplateURL(profile_, data);
        if (!AddNoNotify(default_search_provider, true))
          default_search_provider = NULL;
      }
    }
    // Note that this saves the default search provider to prefs.
    if (!default_search_provider ||
        (!default_search_provider->IsExtensionKeyword() &&
         default_search_provider->SupportsReplacement())) {
      bool success = SetDefaultSearchProviderNoNotify(default_search_provider);
      DCHECK(success);
    }
  } else {
    // If we had a managed default, replace it with the synced default if
    // applicable, or the first provider of the list.
    TemplateURL* synced_default = GetPendingSyncedDefaultSearchProvider();
    if (synced_default) {
      default_search_provider = synced_default;
      pending_synced_default_search_ = false;
    } else if (database_specified_a_default &&
               default_search_provider == NULL) {
      UMA_HISTOGRAM_ENUMERATION(kFirstPotentialEngineHistogramName,
          FIRST_POTENTIAL_CALLSITE_ON_LOAD, FIRST_POTENTIAL_CALLSITE_MAX);
      default_search_provider = FirstPotentialDefaultEngine(template_urls);
    }

    // If the default search provider existed previously, then just
    // set the member variable. Otherwise, we'll set it using the method
    // to ensure that it is saved properly after its id is set.
    if (default_search_provider &&
        (default_search_provider->id() != kInvalidTemplateURLID)) {
      default_search_provider_ = default_search_provider;
      default_search_provider = NULL;
    }
    SetTemplateURLs(template_urls);

    if (default_search_provider) {
      // Note that this saves the default search provider to prefs.
      SetDefaultSearchProvider(default_search_provider);
    } else {
      // Always save the default search provider to prefs. That way we don't
      // have to worry about it being out of sync.
      if (default_search_provider_)
        SaveDefaultSearchProviderToPrefs(default_search_provider_);
    }
  }

  // This initializes provider_map_ which should be done before
  // calling UpdateKeywordSearchTermsForURL.
  ChangeToLoadedState();

  // Index any visits that occurred before we finished loading.
  for (size_t i = 0; i < visits_to_add_.size(); ++i)
    UpdateKeywordSearchTermsForURL(visits_to_add_[i]);
  visits_to_add_.clear();

  if (new_resource_keyword_version)
    service_->SetBuiltinKeywordVersion(new_resource_keyword_version);

  if (!is_default_search_managed_) {
    bool has_default_search_provider = default_search_provider_ != NULL &&
        default_search_provider_->SupportsReplacement();
    UMA_HISTOGRAM_BOOLEAN(kHasDSPHistogramName,
                          has_default_search_provider);
    // Ensure that default search provider exists. See http://crbug.com/116952.
    if (!has_default_search_provider) {
      bool success =
          SetDefaultSearchProviderNoNotify(FindNewDefaultSearchProvider());
      DCHECK(success);
    }
    // Don't log anything if the user has a NULL default search provider. A
    // logged value of 0 indicates a custom default search provider.
    if (default_search_provider_) {
      UMA_HISTOGRAM_ENUMERATION(
          kDSPHistogramName,
          default_search_provider_->prepopulate_id(),
          TemplateURLPrepopulateData::kMaxPrepopulatedEngineID);
    }
  }

  NotifyObservers();
  NotifyLoaded();
}

string16 TemplateURLService::GetKeywordShortName(const string16& keyword,
                                                 bool* is_extension_keyword) {
  const TemplateURL* template_url = GetTemplateURLForKeyword(keyword);

  // TODO(sky): Once LocationBarView adds a listener to the TemplateURLService
  // to track changes to the model, this should become a DCHECK.
  if (template_url) {
    *is_extension_keyword = template_url->IsExtensionKeyword();
    return template_url->AdjustedShortNameForLocaleDirection();
  }
  *is_extension_keyword = false;
  return string16();
}

void TemplateURLService::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_HISTORY_URL_VISITED) {
    content::Details<history::URLVisitedDetails> visit_details(details);
    if (!loaded_)
      visits_to_add_.push_back(*visit_details.ptr());
    else
      UpdateKeywordSearchTermsForURL(*visit_details.ptr());
  } else if (type == chrome::NOTIFICATION_DEFAULT_SEARCH_POLICY_CHANGED) {
    // Policy has been updated, so the default search prefs may be different.
    // Reload the default search provider from them.
    // TODO(pkasting): Rather than communicating via prefs, we should eventually
    // observe policy changes directly.
    UpdateDefaultSearch();
  } else if (type == chrome::NOTIFICATION_GOOGLE_URL_UPDATED) {
    if (loaded_) {
      GoogleBaseURLChanged(
          content::Details<GoogleURLTracker::UpdatedDetails>(details)->first);
    }
  } else {
    NOTREACHED();
  }
}

void TemplateURLService::OnSyncedDefaultSearchProviderGUIDChanged() {
  // Listen for changes to the default search from Sync.
  PrefService* prefs = GetPrefs();
  TemplateURL* new_default_search = GetTemplateURLForGUID(
      prefs->GetString(prefs::kSyncedDefaultSearchProviderGUID));
  if (new_default_search && !is_default_search_managed_) {
    if (new_default_search != GetDefaultSearchProvider()) {
      base::AutoReset<DefaultSearchChangeOrigin> change_origin(
          &dsp_change_origin_, DSP_CHANGE_SYNC_PREF);
      SetDefaultSearchProvider(new_default_search);
      pending_synced_default_search_ = false;
    }
  } else {
    // If it's not there, or if default search is currently managed, set a
    // flag to indicate that we waiting on the search engine entry to come
    // in through Sync.
    pending_synced_default_search_ = true;
  }
  UpdateDefaultSearch();
}

syncer::SyncDataList TemplateURLService::GetAllSyncData(
    syncer::ModelType type) const {
  DCHECK_EQ(syncer::SEARCH_ENGINES, type);

  syncer::SyncDataList current_data;
  for (TemplateURLVector::const_iterator iter = template_urls_.begin();
      iter != template_urls_.end(); ++iter) {
    // We don't sync extension keywords.
    // TODO(mpcomplete): If we allow editing extension keywords, then those
    // should be persisted to disk and synced.
    if ((*iter)->IsExtensionKeyword())
      continue;
    // We don't sync keywords managed by policy.
    if ((*iter)->created_by_policy())
      continue;
    current_data.push_back(CreateSyncDataFromTemplateURL(**iter));
  }

  return current_data;
}

syncer::SyncError TemplateURLService::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  if (!models_associated_) {
    syncer::SyncError error(FROM_HERE, "Models not yet associated.",
                    syncer::SEARCH_ENGINES);
    return error;
  }
  DCHECK(loaded_);

  base::AutoReset<bool> processing_changes(&processing_syncer_changes_, true);

  // We've started syncing, so set our origin member to the base Sync value.
  // As we move through Sync Code, we may set this to increasingly specific
  // origins so we can tell what exactly caused a DSP change.
  base::AutoReset<DefaultSearchChangeOrigin> change_origin(&dsp_change_origin_,
      DSP_CHANGE_SYNC_UNINTENTIONAL);

  syncer::SyncChangeList new_changes;
  syncer::SyncError error;
  for (syncer::SyncChangeList::const_iterator iter = change_list.begin();
      iter != change_list.end(); ++iter) {
    DCHECK_EQ(syncer::SEARCH_ENGINES, iter->sync_data().GetDataType());

    std::string guid =
        iter->sync_data().GetSpecifics().search_engine().sync_guid();
    TemplateURL* existing_turl = GetTemplateURLForGUID(guid);
    scoped_ptr<TemplateURL> turl(CreateTemplateURLFromTemplateURLAndSyncData(
        profile_, existing_turl, iter->sync_data(), &new_changes));
    if (!turl.get())
      continue;

    // Explicitly don't check for conflicts against extension keywords; in this
    // case the functions which modify the keyword map know how to handle the
    // conflicts.
    // TODO(mpcomplete): If we allow editing extension keywords, then those will
    // need to undergo conflict resolution.
    TemplateURL* existing_keyword_turl =
        FindNonExtensionTemplateURLForKeyword(turl->keyword());
    if (iter->change_type() == syncer::SyncChange::ACTION_DELETE) {
      if (!existing_turl) {
        NOTREACHED() << "Unexpected sync change state.";
        error = sync_error_factory_->CreateAndUploadError(
            FROM_HERE,
            "ProcessSyncChanges failed on ChangeType ACTION_DELETE");
        LOG(ERROR) << "Trying to delete a non-existent TemplateURL.";
        continue;
      }
      if (existing_turl == GetDefaultSearchProvider()) {
        // The only way Sync can attempt to delete the default search provider
        // is if we had changed the kSyncedDefaultSearchProviderGUID
        // preference, but perhaps it has not yet been received. To avoid
        // situations where this has come in erroneously, we will un-delete
        // the current default search from the Sync data. If the pref really
        // does arrive later, then default search will change to the correct
        // entry, but we'll have this extra entry sitting around. The result is
        // not ideal, but it prevents a far more severe bug where the default is
        // unexpectedly swapped to something else. The user can safely delete
        // the extra entry again later, if they choose. Most users who do not
        // look at the search engines UI will not notice this.
        // Note that we append a special character to the end of the keyword in
        // an attempt to avoid a ping-poinging situation where receiving clients
        // may try to continually delete the resurrected entry.
        string16 updated_keyword = UniquifyKeyword(*existing_turl, true);
        TemplateURLData data(existing_turl->data());
        data.SetKeyword(updated_keyword);
        TemplateURL new_turl(existing_turl->profile(), data);
        UIThreadSearchTermsData search_terms_data(existing_turl->profile());
        if (UpdateNoNotify(existing_turl, new_turl, search_terms_data))
          NotifyObservers();

        syncer::SyncData sync_data = CreateSyncDataFromTemplateURL(new_turl);
        new_changes.push_back(syncer::SyncChange(FROM_HERE,
                                                 syncer::SyncChange::ACTION_ADD,
                                                 sync_data));
        // Ignore the delete attempt. This means we never end up reseting the
        // default search provider due to an ACTION_DELETE from sync.
        continue;
      }

      Remove(existing_turl);
    } else if (iter->change_type() == syncer::SyncChange::ACTION_ADD) {
      if (existing_turl) {
        NOTREACHED() << "Unexpected sync change state.";
        error = sync_error_factory_->CreateAndUploadError(
            FROM_HERE,
            "ProcessSyncChanges failed on ChangeType ACTION_ADD");
        LOG(ERROR) << "Trying to add an existing TemplateURL.";
        continue;
      }
      const std::string guid = turl->sync_guid();
      if (existing_keyword_turl) {
        // Resolve any conflicts so we can safely add the new entry.
        ResolveSyncKeywordConflict(turl.get(), existing_keyword_turl,
                                   &new_changes);
      }
      // Force the local ID to kInvalidTemplateURLID so we can add it.
      TemplateURLData data(turl->data());
      data.id = kInvalidTemplateURLID;
      Add(new TemplateURL(profile_, data));

      // Possibly set the newly added |turl| as the default search provider.
      SetDefaultSearchProviderIfNewlySynced(guid);
    } else if (iter->change_type() == syncer::SyncChange::ACTION_UPDATE) {
      if (!existing_turl) {
        NOTREACHED() << "Unexpected sync change state.";
        error = sync_error_factory_->CreateAndUploadError(
            FROM_HERE,
            "ProcessSyncChanges failed on ChangeType ACTION_UPDATE");
        LOG(ERROR) << "Trying to update a non-existent TemplateURL.";
        continue;
      }
      if (existing_keyword_turl && (existing_keyword_turl != existing_turl)) {
        // Resolve any conflicts with other entries so we can safely update the
        // keyword.
        ResolveSyncKeywordConflict(turl.get(), existing_keyword_turl,
                                   &new_changes);
      }
      UIThreadSearchTermsData search_terms_data(existing_turl->profile());
      if (UpdateNoNotify(existing_turl, *turl, search_terms_data))
        NotifyObservers();
    } else {
      // We've unexpectedly received an ACTION_INVALID.
      NOTREACHED() << "Unexpected sync change state.";
      error = sync_error_factory_->CreateAndUploadError(
          FROM_HERE,
          "ProcessSyncChanges received an ACTION_INVALID");
    }
  }

  // If something went wrong, we want to prematurely exit to avoid pushing
  // inconsistent data to Sync. We return the last error we received.
  if (error.IsSet())
    return error;

  error = sync_processor_->ProcessSyncChanges(from_here, new_changes);

  return error;
}

syncer::SyncMergeResult TemplateURLService::MergeDataAndStartSyncing(
    syncer::ModelType type,
    const syncer::SyncDataList& initial_sync_data,
    scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
    scoped_ptr<syncer::SyncErrorFactory> sync_error_factory) {
  DCHECK(loaded_);
  DCHECK_EQ(type, syncer::SEARCH_ENGINES);
  DCHECK(!sync_processor_.get());
  DCHECK(sync_processor.get());
  DCHECK(sync_error_factory.get());
  syncer::SyncMergeResult merge_result(type);
  sync_processor_ = sync_processor.Pass();
  sync_error_factory_ = sync_error_factory.Pass();

  // We just started syncing, so set our wait-for-default flag if we are
  // expecting a default from Sync.
  if (GetPrefs()) {
    std::string default_guid = GetPrefs()->GetString(
        prefs::kSyncedDefaultSearchProviderGUID);
    const TemplateURL* current_default = GetDefaultSearchProvider();

    if (!default_guid.empty() &&
        (!current_default || current_default->sync_guid() != default_guid))
      pending_synced_default_search_ = true;
  }

  // We do a lot of calls to Add/Remove/ResetTemplateURL here, so ensure we
  // don't step on our own toes.
  base::AutoReset<bool> processing_changes(&processing_syncer_changes_, true);

  // We've started syncing, so set our origin member to the base Sync value.
  // As we move through Sync Code, we may set this to increasingly specific
  // origins so we can tell what exactly caused a DSP change.
  base::AutoReset<DefaultSearchChangeOrigin> change_origin(&dsp_change_origin_,
      DSP_CHANGE_SYNC_UNINTENTIONAL);

  syncer::SyncChangeList new_changes;

  // Build maps of our sync GUIDs to syncer::SyncData.
  SyncDataMap local_data_map = CreateGUIDToSyncDataMap(
      GetAllSyncData(syncer::SEARCH_ENGINES));
  SyncDataMap sync_data_map = CreateGUIDToSyncDataMap(initial_sync_data);

  merge_result.set_num_items_before_association(local_data_map.size());
  for (SyncDataMap::const_iterator iter = sync_data_map.begin();
      iter != sync_data_map.end(); ++iter) {
    TemplateURL* local_turl = GetTemplateURLForGUID(iter->first);
    scoped_ptr<TemplateURL> sync_turl(
        CreateTemplateURLFromTemplateURLAndSyncData(profile_, local_turl,
            iter->second, &new_changes));
    if (!sync_turl.get())
      continue;

    if (pre_sync_deletes_.find(sync_turl->sync_guid()) !=
        pre_sync_deletes_.end()) {
      // This entry was deleted before the initial sync began (possibly through
      // preprocessing in TemplateURLService's loading code). Ignore it and send
      // an ACTION_DELETE up to the server.
      new_changes.push_back(
          syncer::SyncChange(FROM_HERE,
                             syncer::SyncChange::ACTION_DELETE,
                             iter->second));
      UMA_HISTOGRAM_ENUMERATION(kDeleteSyncedEngineHistogramName,
          DELETE_ENGINE_PRE_SYNC, DELETE_ENGINE_MAX);
      continue;
    }

    if (local_turl) {
      DCHECK(IsFromSync(local_turl, sync_data_map));
      // This local search engine is already synced. If the timestamp differs
      // from Sync, we need to update locally or to the cloud. Note that if the
      // timestamps are equal, we touch neither.
      if (sync_turl->last_modified() > local_turl->last_modified()) {
        // We've received an update from Sync. We should replace all synced
        // fields in the local TemplateURL. Note that this includes the
        // TemplateURLID and the TemplateURL may have to be reparsed. This
        // also makes the local data's last_modified timestamp equal to Sync's,
        // avoiding an Update on the next MergeData call.
        UIThreadSearchTermsData search_terms_data(local_turl->profile());
        if (UpdateNoNotify(local_turl, *sync_turl, search_terms_data))
          NotifyObservers();
        merge_result.set_num_items_modified(
            merge_result.num_items_modified() + 1);
      } else if (sync_turl->last_modified() < local_turl->last_modified()) {
        // Otherwise, we know we have newer data, so update Sync with our
        // data fields.
        new_changes.push_back(
            syncer::SyncChange(FROM_HERE,
                               syncer::SyncChange::ACTION_UPDATE,
                               local_data_map[local_turl->sync_guid()]));
      }
      local_data_map.erase(iter->first);
    } else {
      // The search engine from the cloud has not been synced locally. Merge it
      // into our local model. This will handle any conflicts with local (and
      // already-synced) TemplateURLs. It will prefer to keep entries from Sync
      // over not-yet-synced TemplateURLs.
      MergeInSyncTemplateURL(sync_turl.get(), sync_data_map, &new_changes,
                             &local_data_map, &merge_result);
    }
  }

  // If there is a pending synced default search provider that was processed
  // above, set it now.
  TemplateURL* pending_default = GetPendingSyncedDefaultSearchProvider();
  if (pending_default) {
    base::AutoReset<DefaultSearchChangeOrigin> change_origin(
        &dsp_change_origin_, DSP_CHANGE_SYNC_ADD);
    SetDefaultSearchProvider(pending_default);
  }

  // The remaining SyncData in local_data_map should be everything that needs to
  // be pushed as ADDs to sync.
  for (SyncDataMap::const_iterator iter = local_data_map.begin();
      iter != local_data_map.end(); ++iter) {
    new_changes.push_back(
        syncer::SyncChange(FROM_HERE,
                           syncer::SyncChange::ACTION_ADD,
                           iter->second));
  }

  // Do some post-processing on the change list to ensure that we are sending
  // valid changes to sync_processor_.
  PruneSyncChanges(&sync_data_map, &new_changes);

  LogDuplicatesHistogram(GetTemplateURLs());
  merge_result.set_num_items_after_association(
      GetAllSyncData(syncer::SEARCH_ENGINES).size());
  merge_result.set_error(
      sync_processor_->ProcessSyncChanges(FROM_HERE, new_changes));
  if (merge_result.error().IsSet())
    return merge_result;

  // The ACTION_DELETEs from this set are processed. Empty it so we don't try to
  // reuse them on the next call to MergeDataAndStartSyncing.
  pre_sync_deletes_.clear();

  models_associated_ = true;
  return merge_result;
}

void TemplateURLService::StopSyncing(syncer::ModelType type) {
  DCHECK_EQ(type, syncer::SEARCH_ENGINES);
  models_associated_ = false;
  sync_processor_.reset();
  sync_error_factory_.reset();
}

void TemplateURLService::ProcessTemplateURLChange(
    const tracked_objects::Location& from_here,
    const TemplateURL* turl,
    syncer::SyncChange::SyncChangeType type) {
  DCHECK_NE(type, syncer::SyncChange::ACTION_INVALID);
  DCHECK(turl);

  if (!models_associated_)
    return;  // Not syncing.

  if (processing_syncer_changes_)
    return;  // These are changes originating from us. Ignore.

  // Avoid syncing Extension keywords.
  // TODO(mpcomplete): If we allow editing extension keywords, then those should
  // be persisted to disk and synced.
  if (turl->IsExtensionKeyword())
    return;

  // Avoid syncing keywords managed by policy.
  if (turl->created_by_policy())
    return;

  syncer::SyncChangeList changes;

  syncer::SyncData sync_data = CreateSyncDataFromTemplateURL(*turl);
  changes.push_back(syncer::SyncChange(from_here,
                                       type,
                                       sync_data));

  sync_processor_->ProcessSyncChanges(FROM_HERE, changes);
}

// static
syncer::SyncData TemplateURLService::CreateSyncDataFromTemplateURL(
    const TemplateURL& turl) {
  sync_pb::EntitySpecifics specifics;
  sync_pb::SearchEngineSpecifics* se_specifics =
      specifics.mutable_search_engine();
  se_specifics->set_short_name(UTF16ToUTF8(turl.short_name()));
  se_specifics->set_keyword(UTF16ToUTF8(turl.keyword()));
  se_specifics->set_favicon_url(turl.favicon_url().spec());
  se_specifics->set_url(turl.url());
  se_specifics->set_safe_for_autoreplace(turl.safe_for_autoreplace());
  se_specifics->set_originating_url(turl.originating_url().spec());
  se_specifics->set_date_created(turl.date_created().ToInternalValue());
  se_specifics->set_input_encodings(JoinString(turl.input_encodings(), ';'));
  se_specifics->set_show_in_default_list(turl.show_in_default_list());
  se_specifics->set_suggestions_url(turl.suggestions_url());
  se_specifics->set_prepopulate_id(turl.prepopulate_id());
  se_specifics->set_instant_url(turl.instant_url());
  se_specifics->set_last_modified(turl.last_modified().ToInternalValue());
  se_specifics->set_sync_guid(turl.sync_guid());
  for (size_t i = 0; i < turl.alternate_urls().size(); ++i)
    se_specifics->add_alternate_urls(turl.alternate_urls()[i]);
  se_specifics->set_search_terms_replacement_key(
      turl.search_terms_replacement_key());

  return syncer::SyncData::CreateLocalData(se_specifics->sync_guid(),
                                           se_specifics->keyword(),
                                           specifics);
}

// static
TemplateURL* TemplateURLService::CreateTemplateURLFromTemplateURLAndSyncData(
    Profile* profile,
    TemplateURL* existing_turl,
    const syncer::SyncData& sync_data,
    syncer::SyncChangeList* change_list) {
  DCHECK(change_list);

  sync_pb::SearchEngineSpecifics specifics =
      sync_data.GetSpecifics().search_engine();

  // Past bugs might have caused either of these fields to be empty.  Just
  // delete this data off the server.
  if (specifics.url().empty() || specifics.sync_guid().empty()) {
    change_list->push_back(
        syncer::SyncChange(FROM_HERE,
                           syncer::SyncChange::ACTION_DELETE,
                           sync_data));
    UMA_HISTOGRAM_ENUMERATION(kDeleteSyncedEngineHistogramName,
        DELETE_ENGINE_EMPTY_FIELD, DELETE_ENGINE_MAX);
    return NULL;
  }

  TemplateURLData data(existing_turl ?
      existing_turl->data() : TemplateURLData());
  data.short_name = UTF8ToUTF16(specifics.short_name());
  data.originating_url = GURL(specifics.originating_url());
  string16 keyword(UTF8ToUTF16(specifics.keyword()));
  // NOTE: Once this code has shipped in a couple of stable releases, we can
  // probably remove the migration portion, comment out the
  // "autogenerate_keyword" field entirely in the .proto file, and fold the
  // empty keyword case into the "delete data" block above.
  bool reset_keyword =
      specifics.autogenerate_keyword() || specifics.keyword().empty();
  if (reset_keyword)
    keyword = ASCIIToUTF16("dummy");  // Will be replaced below.
  DCHECK(!keyword.empty());
  data.SetKeyword(keyword);
  data.SetURL(specifics.url());
  data.suggestions_url = specifics.suggestions_url();
  data.instant_url = specifics.instant_url();
  data.favicon_url = GURL(specifics.favicon_url());
  data.show_in_default_list = specifics.show_in_default_list();
  data.safe_for_autoreplace = specifics.safe_for_autoreplace();
  base::SplitString(specifics.input_encodings(), ';', &data.input_encodings);
  // If the server data has duplicate encodings, we'll want to push an update
  // below to correct it.  Note that we also fix this in
  // GetSearchProvidersUsingKeywordResult(), since otherwise we'd never correct
  // local problems for clients which have disabled search engine sync.
  bool deduped = DeDupeEncodings(&data.input_encodings);
  data.date_created = base::Time::FromInternalValue(specifics.date_created());
  data.last_modified = base::Time::FromInternalValue(specifics.last_modified());
  data.prepopulate_id = specifics.prepopulate_id();
  data.sync_guid = specifics.sync_guid();
  data.alternate_urls.clear();
  for (int i = 0; i < specifics.alternate_urls_size(); ++i)
    data.alternate_urls.push_back(specifics.alternate_urls(i));
  data.search_terms_replacement_key = specifics.search_terms_replacement_key();

  TemplateURL* turl = new TemplateURL(profile, data);
  // If this TemplateURL matches a built-in prepopulated template URL, it's
  // possible that sync is trying to modify fields that should not be touched.
  // Revert these fields to the built-in values.
  UpdateTemplateURLIfPrepopulated(turl, profile);
  DCHECK(!turl->IsExtensionKeyword());
  if (reset_keyword || deduped) {
    if (reset_keyword)
      turl->ResetKeywordIfNecessary(true);
    syncer::SyncData sync_data = CreateSyncDataFromTemplateURL(*turl);
    change_list->push_back(syncer::SyncChange(FROM_HERE,
                                              syncer::SyncChange::ACTION_UPDATE,
                                              sync_data));
  } else if (turl->IsGoogleSearchURLWithReplaceableKeyword()) {
    if (!existing_turl) {
      // We're adding a new TemplateURL that uses the Google base URL, so set
      // its keyword appropriately for the local environment.
      turl->ResetKeywordIfNecessary(false);
    } else if (existing_turl->IsGoogleSearchURLWithReplaceableKeyword()) {
      // Ignore keyword changes triggered by the Google base URL changing on
      // another client.  If the base URL changes in this client as well, we'll
      // pick that up separately at the appropriate time.  Otherwise, changing
      // the keyword here could result in having the wrong keyword for the local
      // environment.
      turl->data_.SetKeyword(existing_turl->keyword());
    }
  }

  return turl;
}

// static
SyncDataMap TemplateURLService::CreateGUIDToSyncDataMap(
    const syncer::SyncDataList& sync_data) {
  SyncDataMap data_map;
  for (syncer::SyncDataList::const_iterator i(sync_data.begin());
       i != sync_data.end();
       ++i)
    data_map[i->GetSpecifics().search_engine().sync_guid()] = *i;
  return data_map;
}

void TemplateURLService::SetKeywordSearchTermsForURL(const TemplateURL* t_url,
                                                     const GURL& url,
                                                     const string16& term) {
  HistoryService* history = profile_  ?
      HistoryServiceFactory::GetForProfile(profile_,
                                           Profile::EXPLICIT_ACCESS) :
      NULL;
  if (!history)
    return;
  history->SetKeywordSearchTermsForURL(url, t_url->id(), term);
}

void TemplateURLService::Init(const Initializer* initializers,
                              int num_initializers) {
  // Register for notifications.
  if (profile_) {
    // TODO(sky): bug 1166191. The keywords should be moved into the history
    // db, which will mean we no longer need this notification and the history
    // backend can handle automatically adding the search terms as the user
    // navigates.
    content::Source<Profile> profile_source(profile_->GetOriginalProfile());
    notification_registrar_.Add(this, chrome::NOTIFICATION_HISTORY_URL_VISITED,
                                profile_source);
    notification_registrar_.Add(this, chrome::NOTIFICATION_GOOGLE_URL_UPDATED,
                                profile_source);
    pref_change_registrar_.Init(GetPrefs());
    pref_change_registrar_.Add(
        prefs::kSyncedDefaultSearchProviderGUID,
        base::Bind(
            &TemplateURLService::OnSyncedDefaultSearchProviderGUIDChanged,
            base::Unretained(this)));
  }
  notification_registrar_.Add(this,
      chrome::NOTIFICATION_DEFAULT_SEARCH_POLICY_CHANGED,
      content::NotificationService::AllSources());

  if (num_initializers > 0) {
    // This path is only hit by test code and is used to simulate a loaded
    // TemplateURLService.
    ChangeToLoadedState();

    // Add specific initializers, if any.
    for (int i(0); i < num_initializers; ++i) {
      DCHECK(initializers[i].keyword);
      DCHECK(initializers[i].url);
      DCHECK(initializers[i].content);

      size_t template_position =
          std::string(initializers[i].url).find(kTemplateParameter);
      DCHECK(template_position != std::string::npos);
      std::string osd_url(initializers[i].url);
      osd_url.replace(template_position, arraysize(kTemplateParameter) - 1,
                      kSearchTermParameter);

      // TemplateURLService ends up owning the TemplateURL, don't try and free
      // it.
      TemplateURLData data;
      data.short_name = UTF8ToUTF16(initializers[i].content);
      data.SetKeyword(UTF8ToUTF16(initializers[i].keyword));
      data.SetURL(osd_url);
      AddNoNotify(new TemplateURL(profile_, data), true);
    }
  }

  // Initialize default search.
  UpdateDefaultSearch();

  // Request a server check for the correct Google URL if Google is the
  // default search engine, not in headless mode and not in Chrome Frame.
  if (profile_ && initial_default_search_provider_.get() &&
      initial_default_search_provider_->url_ref().HasGoogleBaseURLs()) {
    scoped_ptr<base::Environment> env(base::Environment::Create());
    if (!env->HasVar(env_vars::kHeadless) &&
        !CommandLine::ForCurrentProcess()->HasSwitch(switches::kChromeFrame))
      GoogleURLTracker::RequestServerCheck(profile_);
  }
}

void TemplateURLService::RemoveFromMaps(TemplateURL* template_url) {
  const string16& keyword = template_url->keyword();
  DCHECK_NE(0U, keyword_to_template_map_.count(keyword));
  if (keyword_to_template_map_[keyword] == template_url) {
    // We need to check whether the keyword can now be provided by another
    // TemplateURL.  See the comments in AddToMaps() for more information on
    // extension keywords and how they can coexist with non-extension keywords.
    // In the case of more than one extension, we use the most recently
    // installed (which will be the most recently added, which will have the
    // highest ID).
    TemplateURL* best_fallback = NULL;
    for (TemplateURLVector::const_iterator i(template_urls_.begin());
         i != template_urls_.end(); ++i) {
      TemplateURL* turl = *i;
      // This next statement relies on the fact that there can only be one
      // non-extension TemplateURL with a given keyword.
      if ((turl != template_url) && (turl->keyword() == keyword) &&
          (!best_fallback || !best_fallback->IsExtensionKeyword() ||
           (turl->IsExtensionKeyword() && (turl->id() > best_fallback->id()))))
        best_fallback = turl;
    }
    if (best_fallback)
      keyword_to_template_map_[keyword] = best_fallback;
    else
      keyword_to_template_map_.erase(keyword);
  }

  // If the keyword we're removing is from an extension, we're now done, since
  // it won't be synced or stored in the provider map.
  // TODO(mpcomplete): If we allow editing extension keywords, then those should
  // be synced.
  if (template_url->IsExtensionKeyword())
    return;

  if (!template_url->sync_guid().empty())
    guid_to_template_map_.erase(template_url->sync_guid());
  if (loaded_) {
    UIThreadSearchTermsData search_terms_data(template_url->profile());
    provider_map_->Remove(template_url, search_terms_data);
  }
}

void TemplateURLService::RemoveFromKeywordMapByPointer(
    TemplateURL* template_url) {
  DCHECK(template_url);
  for (KeywordToTemplateMap::iterator i = keyword_to_template_map_.begin();
       i != keyword_to_template_map_.end(); ++i) {
    if (i->second == template_url) {
      keyword_to_template_map_.erase(i);
      // A given TemplateURL only occurs once in the map. As soon as we find the
      // entry, stop.
      break;
    }
  }
}

void TemplateURLService::AddToMaps(TemplateURL* template_url) {
  bool template_extension = template_url->IsExtensionKeyword();
  const string16& keyword = template_url->keyword();
  KeywordToTemplateMap::const_iterator i =
      keyword_to_template_map_.find(keyword);
  if (i == keyword_to_template_map_.end()) {
    keyword_to_template_map_[keyword] = template_url;
  } else {
    const TemplateURL* existing_url = i->second;
    // We should only have overlapping keywords when at least one comes from
    // an extension.  In that case, the ranking order is:
    //   Manually-modified keywords > extension keywords > replaceable keywords
    // When there are multiple extensions, the last-added wins.
    bool existing_extension = existing_url->IsExtensionKeyword();
    DCHECK(existing_extension || template_extension);
    if (existing_extension ?
        !CanReplace(template_url) : CanReplace(existing_url))
      keyword_to_template_map_[keyword] = template_url;
  }

  // Extension keywords are not synced, so they don't go into the GUID map,
  // and do not use host-based search URLs, so they don't go into the provider
  // map, so at this point we're done.
  // TODO(mpcomplete): If we allow editing extension keywords, then those should
  // be persisted to disk and synced.
  if (template_extension)
    return;

  if (!template_url->sync_guid().empty())
    guid_to_template_map_[template_url->sync_guid()] = template_url;
  if (loaded_) {
    UIThreadSearchTermsData search_terms_data(profile_);
    provider_map_->Add(template_url, search_terms_data);
  }
}

void TemplateURLService::SetTemplateURLs(const TemplateURLVector& urls) {
  // Add mappings for the new items.

  // First, add the items that already have id's, so that the next_id_
  // gets properly set.
  for (TemplateURLVector::const_iterator i = urls.begin(); i != urls.end();
       ++i) {
    if ((*i)->id() != kInvalidTemplateURLID) {
      next_id_ = std::max(next_id_, (*i)->id());
      AddNoNotify(*i, false);
    }
  }

  // Next add the new items that don't have id's.
  for (TemplateURLVector::const_iterator i = urls.begin(); i != urls.end();
       ++i) {
    if ((*i)->id() == kInvalidTemplateURLID)
      AddNoNotify(*i, true);
  }
}

void TemplateURLService::ChangeToLoadedState() {
  DCHECK(!loaded_);

  UIThreadSearchTermsData search_terms_data(profile_);
  provider_map_->Init(template_urls_, search_terms_data);
  loaded_ = true;
}

void TemplateURLService::NotifyLoaded() {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_TEMPLATE_URL_SERVICE_LOADED,
      content::Source<TemplateURLService>(this),
      content::NotificationService::NoDetails());
}

void TemplateURLService::SaveDefaultSearchProviderToPrefs(
    const TemplateURL* t_url) {
  PrefService* prefs = GetPrefs();
  if (!prefs)
    return;

  bool enabled = false;
  std::string search_url;
  std::string suggest_url;
  std::string instant_url;
  std::string icon_url;
  std::string encodings;
  std::string short_name;
  std::string keyword;
  std::string id_string;
  std::string prepopulate_id;
  ListValue alternate_urls;
  std::string search_terms_replacement_key;
  if (t_url) {
    DCHECK(!t_url->IsExtensionKeyword());
    enabled = true;
    search_url = t_url->url();
    suggest_url = t_url->suggestions_url();
    instant_url = t_url->instant_url();
    GURL icon_gurl = t_url->favicon_url();
    if (!icon_gurl.is_empty())
      icon_url = icon_gurl.spec();
    encodings = JoinString(t_url->input_encodings(), ';');
    short_name = UTF16ToUTF8(t_url->short_name());
    keyword = UTF16ToUTF8(t_url->keyword());
    id_string = base::Int64ToString(t_url->id());
    prepopulate_id = base::Int64ToString(t_url->prepopulate_id());
    for (size_t i = 0; i < t_url->alternate_urls().size(); ++i)
      alternate_urls.AppendString(t_url->alternate_urls()[i]);
    search_terms_replacement_key = t_url->search_terms_replacement_key();
  }
  prefs->SetBoolean(prefs::kDefaultSearchProviderEnabled, enabled);
  prefs->SetString(prefs::kDefaultSearchProviderSearchURL, search_url);
  prefs->SetString(prefs::kDefaultSearchProviderSuggestURL, suggest_url);
  prefs->SetString(prefs::kDefaultSearchProviderInstantURL, instant_url);
  prefs->SetString(prefs::kDefaultSearchProviderIconURL, icon_url);
  prefs->SetString(prefs::kDefaultSearchProviderEncodings, encodings);
  prefs->SetString(prefs::kDefaultSearchProviderName, short_name);
  prefs->SetString(prefs::kDefaultSearchProviderKeyword, keyword);
  prefs->SetString(prefs::kDefaultSearchProviderID, id_string);
  prefs->SetString(prefs::kDefaultSearchProviderPrepopulateID, prepopulate_id);
  prefs->Set(prefs::kDefaultSearchProviderAlternateURLs, alternate_urls);
  prefs->SetString(prefs::kDefaultSearchProviderSearchTermsReplacementKey,
      search_terms_replacement_key);
}

bool TemplateURLService::LoadDefaultSearchProviderFromPrefs(
    scoped_ptr<TemplateURL>* default_provider,
    bool* is_managed) {
  PrefService* prefs = GetPrefs();
  if (!prefs || !prefs->HasPrefPath(prefs::kDefaultSearchProviderSearchURL))
    return false;

  const PrefService::Preference* pref =
      prefs->FindPreference(prefs::kDefaultSearchProviderSearchURL);
  *is_managed = pref && pref->IsManaged();

  if (!prefs->GetBoolean(prefs::kDefaultSearchProviderEnabled)) {
    // The user doesn't want a default search provider.
    default_provider->reset(NULL);
    return true;
  }

  string16 name =
      UTF8ToUTF16(prefs->GetString(prefs::kDefaultSearchProviderName));
  string16 keyword =
      UTF8ToUTF16(prefs->GetString(prefs::kDefaultSearchProviderKeyword));
  // Force keyword to be non-empty.
  // TODO(pkasting): This is only necessary as long as we're potentially loading
  // older prefs where empty keywords are theoretically possible.  Eventually
  // this code can be replaced with a DCHECK(!keyword.empty());.
  bool update_keyword = keyword.empty();
  if (update_keyword)
    keyword = ASCIIToUTF16("dummy");
  std::string search_url =
      prefs->GetString(prefs::kDefaultSearchProviderSearchURL);
  // Force URL to be non-empty.  We've never supported this case, but past bugs
  // might have resulted in it slipping through; eventually this code can be
  // replaced with a DCHECK(!search_url.empty());.
  if (search_url.empty())
    return false;
  std::string suggest_url =
      prefs->GetString(prefs::kDefaultSearchProviderSuggestURL);
  std::string instant_url =
      prefs->GetString(prefs::kDefaultSearchProviderInstantURL);
  std::string icon_url =
      prefs->GetString(prefs::kDefaultSearchProviderIconURL);
  std::string encodings =
      prefs->GetString(prefs::kDefaultSearchProviderEncodings);
  std::string id_string = prefs->GetString(prefs::kDefaultSearchProviderID);
  std::string prepopulate_id =
      prefs->GetString(prefs::kDefaultSearchProviderPrepopulateID);
  const ListValue* alternate_urls =
      prefs->GetList(prefs::kDefaultSearchProviderAlternateURLs);
  std::string search_terms_replacement_key = prefs->GetString(
      prefs::kDefaultSearchProviderSearchTermsReplacementKey);

  TemplateURLData data;
  data.short_name = name;
  data.SetKeyword(keyword);
  data.SetURL(search_url);
  data.suggestions_url = suggest_url;
  data.instant_url = instant_url;
  data.favicon_url = GURL(icon_url);
  data.show_in_default_list = true;
  data.alternate_urls.clear();
  for (size_t i = 0; i < alternate_urls->GetSize(); ++i) {
    std::string alternate_url;
    if (alternate_urls->GetString(i, &alternate_url))
      data.alternate_urls.push_back(alternate_url);
  }
  data.search_terms_replacement_key = search_terms_replacement_key;
  base::SplitString(encodings, ';', &data.input_encodings);
  if (!id_string.empty() && !*is_managed) {
    int64 value;
    base::StringToInt64(id_string, &value);
    data.id = value;
  }
  if (!prepopulate_id.empty() && !*is_managed) {
    int value;
    base::StringToInt(prepopulate_id, &value);
    data.prepopulate_id = value;
  }
  default_provider->reset(new TemplateURL(profile_, data));
  DCHECK(!(*default_provider)->IsExtensionKeyword());
  if (update_keyword)
    (*default_provider)->ResetKeywordIfNecessary(true);
  return true;
}

bool TemplateURLService::CanReplaceKeywordForHost(
    const std::string& host,
    TemplateURL** to_replace) {
  DCHECK(!to_replace || !*to_replace);
  const TemplateURLSet* urls = provider_map_->GetURLsForHost(host);
  if (!urls)
    return true;
  for (TemplateURLSet::const_iterator i(urls->begin()); i != urls->end(); ++i) {
    if (CanReplace(*i)) {
      if (to_replace)
        *to_replace = *i;
      return true;
    }
  }
  return false;
}

bool TemplateURLService::CanReplace(const TemplateURL* t_url) {
  return (t_url != default_search_provider_ && !t_url->show_in_default_list() &&
          t_url->safe_for_autoreplace());
}

TemplateURL* TemplateURLService::FindNonExtensionTemplateURLForKeyword(
    const string16& keyword) {
  TemplateURL* keyword_turl = GetTemplateURLForKeyword(keyword);
  if (!keyword_turl || !keyword_turl->IsExtensionKeyword())
    return keyword_turl;
  // The extension keyword in the model may be hiding a replaceable
  // non-extension keyword.  Look for it.
  for (TemplateURLVector::const_iterator i(template_urls_.begin());
       i != template_urls_.end(); ++i) {
    if (!(*i)->IsExtensionKeyword() && ((*i)->keyword() == keyword))
      return *i;
  }
  return NULL;
}

bool TemplateURLService::UpdateNoNotify(
    TemplateURL* existing_turl,
    const TemplateURL& new_values,
    const SearchTermsData& old_search_terms_data) {
  DCHECK(loaded_);
  DCHECK(existing_turl);
  if (std::find(template_urls_.begin(), template_urls_.end(), existing_turl) ==
      template_urls_.end())
    return false;

  // TODO(mpcomplete): If we allow editing extension keywords, then those should
  // be persisted to disk and synced.  In this case this DCHECK should be
  // removed.
  DCHECK(!existing_turl->IsExtensionKeyword());

  string16 old_keyword(existing_turl->keyword());
  keyword_to_template_map_.erase(old_keyword);
  if (!existing_turl->sync_guid().empty())
    guid_to_template_map_.erase(existing_turl->sync_guid());

  provider_map_->Remove(existing_turl, old_search_terms_data);
  TemplateURLID previous_id = existing_turl->id();
  existing_turl->CopyFrom(new_values);
  existing_turl->data_.id = previous_id;
  UIThreadSearchTermsData new_search_terms_data(profile_);
  provider_map_->Add(existing_turl, new_search_terms_data);

  const string16& keyword = existing_turl->keyword();
  KeywordToTemplateMap::const_iterator i =
      keyword_to_template_map_.find(keyword);
  if (i == keyword_to_template_map_.end()) {
    keyword_to_template_map_[keyword] = existing_turl;
  } else {
    // We can theoretically reach here in two cases:
    //   * There is an existing extension keyword and sync brings in a rename of
    //     a non-extension keyword to match.  In this case we just need to pick
    //     which keyword has priority to update the keyword map.
    //   * Autogeneration of the keyword for a Google default search provider
    //     at load time causes it to conflict with an existing keyword.  In this
    //     case we delete the existing keyword if it's replaceable, or else undo
    //     the change in keyword for |existing_turl|.
    DCHECK(!existing_turl->IsExtensionKeyword());
    TemplateURL* existing_keyword_turl = i->second;
    if (existing_keyword_turl->IsExtensionKeyword()) {
      if (!CanReplace(existing_turl))
        keyword_to_template_map_[keyword] = existing_turl;
    } else {
      if (CanReplace(existing_keyword_turl)) {
        RemoveNoNotify(existing_keyword_turl);
      } else {
        existing_turl->data_.SetKeyword(old_keyword);
        keyword_to_template_map_[old_keyword] = existing_turl;
      }
    }
  }
  if (!existing_turl->sync_guid().empty())
    guid_to_template_map_[existing_turl->sync_guid()] = existing_turl;

  if (service_.get())
    service_->UpdateKeyword(existing_turl->data());

  // Inform sync of the update.
  ProcessTemplateURLChange(FROM_HERE,
                           existing_turl,
                           syncer::SyncChange::ACTION_UPDATE);

  if (default_search_provider_ == existing_turl) {
    bool success = SetDefaultSearchProviderNoNotify(existing_turl);
    DCHECK(success);
  }
  return true;
}

// static
void TemplateURLService::UpdateTemplateURLIfPrepopulated(
    TemplateURL* template_url,
    Profile* profile) {
  int prepopulate_id = template_url->prepopulate_id();
  if (template_url->prepopulate_id() == 0)
    return;

  ScopedVector<TemplateURL> prepopulated_urls;
  size_t default_search_index;
  TemplateURLPrepopulateData::GetPrepopulatedEngines(profile,
      &prepopulated_urls.get(), &default_search_index);

  for (size_t i = 0; i < prepopulated_urls.size(); ++i) {
    if (prepopulated_urls[i]->prepopulate_id() == prepopulate_id) {
      MergeIntoPrepopulatedEngineData(&prepopulated_urls[i]->data_,
                                      template_url);
      template_url->CopyFrom(*prepopulated_urls[i]);
    }
  }
}

PrefService* TemplateURLService::GetPrefs() {
  return profile_ ? profile_->GetPrefs() : NULL;
}

void TemplateURLService::UpdateKeywordSearchTermsForURL(
    const history::URLVisitedDetails& details) {
  const history::URLRow& row = details.row;
  if (!row.url().is_valid() ||
      !row.url().parsed_for_possibly_invalid_spec().query.is_nonempty()) {
    return;
  }

  const TemplateURLSet* urls_for_host =
      provider_map_->GetURLsForHost(row.url().host());
  if (!urls_for_host)
    return;

  QueryTerms query_terms;
  bool built_terms = false;  // Most URLs won't match a TemplateURLs host;
                             // so we lazily build the query_terms.
  const std::string path = row.url().path();

  for (TemplateURLSet::const_iterator i = urls_for_host->begin();
       i != urls_for_host->end(); ++i) {
    const TemplateURLRef& search_ref = (*i)->url_ref();

    // Count the URL against a TemplateURL if the host and path of the
    // visited URL match that of the TemplateURL as well as the search term's
    // key of the TemplateURL occurring in the visited url.
    //
    // NOTE: Even though we're iterating over TemplateURLs indexed by the host
    // of the URL we still need to call GetHost on the search_ref. In
    // particular, GetHost returns an empty string if search_ref doesn't support
    // replacement or isn't valid for use in keyword search terms.

    if (search_ref.GetHost() == row.url().host() &&
        search_ref.GetPath() == path) {
      if (!built_terms && !BuildQueryTerms(row.url(), &query_terms)) {
        // No query terms. No need to continue with the rest of the
        // TemplateURLs.
        return;
      }
      built_terms = true;

      if (content::PageTransitionStripQualifier(details.transition) ==
          content::PAGE_TRANSITION_KEYWORD) {
        // The visit is the result of the user entering a keyword, generate a
        // KEYWORD_GENERATED visit for the KEYWORD so that the keyword typed
        // count is boosted.
        AddTabToSearchVisit(**i);
      }

      QueryTerms::iterator j(query_terms.find(search_ref.GetSearchTermKey()));
      if (j != query_terms.end() && !j->second.empty()) {
        SetKeywordSearchTermsForURL(*i, row.url(),
                                    search_ref.SearchTermToString16(j->second));
      }
    }
  }
}

void TemplateURLService::AddTabToSearchVisit(const TemplateURL& t_url) {
  // Only add visits for entries the user hasn't modified. If the user modified
  // the entry the keyword may no longer correspond to the host name. It may be
  // possible to do something more sophisticated here, but it's so rare as to
  // not be worth it.
  if (!t_url.safe_for_autoreplace())
    return;

  if (!profile_)
    return;

  HistoryService* history =
      HistoryServiceFactory::GetForProfile(profile_, Profile::EXPLICIT_ACCESS);
  if (!history)
    return;

  GURL url(URLFixerUpper::FixupURL(UTF16ToUTF8(t_url.keyword()),
                                   std::string()));
  if (!url.is_valid())
    return;

  // Synthesize a visit for the keyword. This ensures the url for the keyword is
  // autocompleted even if the user doesn't type the url in directly.
  history->AddPage(url, base::Time::Now(), NULL, 0, GURL(),
                   history::RedirectList(),
                   content::PAGE_TRANSITION_KEYWORD_GENERATED,
                   history::SOURCE_BROWSED, false);
}

// static
bool TemplateURLService::BuildQueryTerms(const GURL& url,
                                         QueryTerms* query_terms) {
  url_parse::Component query = url.parsed_for_possibly_invalid_spec().query;
  url_parse::Component key, value;
  size_t valid_term_count = 0;
  while (url_parse::ExtractQueryKeyValue(url.spec().c_str(), &query, &key,
                                         &value)) {
    if (key.is_nonempty() && value.is_nonempty()) {
      std::string key_string = url.spec().substr(key.begin, key.len);
      std::string value_string = url.spec().substr(value.begin, value.len);
      QueryTerms::iterator query_terms_iterator =
          query_terms->find(key_string);
      if (query_terms_iterator != query_terms->end()) {
        if (!query_terms_iterator->second.empty() &&
            query_terms_iterator->second != value_string) {
          // The term occurs in multiple places with different values. Treat
          // this as if the term doesn't occur by setting the value to an empty
          // string.
          (*query_terms)[key_string] = std::string();
          DCHECK(valid_term_count > 0);
          valid_term_count--;
        }
      } else {
        valid_term_count++;
        (*query_terms)[key_string] = value_string;
      }
    }
  }
  return (valid_term_count > 0);
}

void TemplateURLService::GoogleBaseURLChanged(const GURL& old_base_url) {
  bool something_changed = false;
  for (TemplateURLVector::iterator i(template_urls_.begin());
       i != template_urls_.end(); ++i) {
    TemplateURL* t_url = *i;
    if (t_url->url_ref().HasGoogleBaseURLs() ||
        t_url->suggestions_url_ref().HasGoogleBaseURLs()) {
      TemplateURL updated_turl(t_url->profile(), t_url->data());
      updated_turl.ResetKeywordIfNecessary(false);
      KeywordToTemplateMap::const_iterator existing_entry =
          keyword_to_template_map_.find(updated_turl.keyword());
      if ((existing_entry != keyword_to_template_map_.end()) &&
          (existing_entry->second != t_url)) {
        // The new autogenerated keyword conflicts with another TemplateURL.
        // Overwrite it if it's replaceable; otherwise, leave |t_url| using its
        // current keyword.  (This will not prevent |t_url| from auto-updating
        // the keyword in the future if the conflicting TemplateURL disappears.)
        // Note that we must still update |t_url| in this case, or the
        // |provider_map_| will not be updated correctly.
        if (CanReplace(existing_entry->second))
          RemoveNoNotify(existing_entry->second);
        else
          updated_turl.data_.SetKeyword(t_url->keyword());
      }
      something_changed = true;
      // This will send the keyword change to sync.  Note that other clients
      // need to reset the keyword to an appropriate local value when this
      // change arrives; see CreateTemplateURLFromTemplateURLAndSyncData().
      UpdateNoNotify(t_url, updated_turl,
          OldBaseURLSearchTermsData(t_url->profile(), old_base_url.spec()));
    }
  }
  if (something_changed)
    NotifyObservers();
}

void TemplateURLService::UpdateDefaultSearch() {
  if (!loaded_) {
    // Set |initial_default_search_provider_| from the preferences.  We use this
    // value for default search provider until the database has been loaded.
    if (!LoadDefaultSearchProviderFromPrefs(&initial_default_search_provider_,
                                            &is_default_search_managed_)) {
      // Prefs does not specify, so rely on the prepopulated engines.  This
      // should happen only the first time Chrome is started.
      initial_default_search_provider_.reset(
          TemplateURLPrepopulateData::GetPrepopulatedDefaultSearch(profile_));
      is_default_search_managed_ = false;
    }
    return;
  }
  // Load the default search specified in prefs.
  scoped_ptr<TemplateURL> new_default_from_prefs;
  bool new_is_default_managed = false;
  // Load the default from prefs.  It's possible that it won't succeed
  // because we are in the middle of doing SaveDefaultSearchProviderToPrefs()
  // and all the preference items have not been saved.  In that case, we
  // don't have yet a default.  It would be much better if we could save
  // preferences in batches and trigger notifications at the end.
  LoadDefaultSearchProviderFromPrefs(&new_default_from_prefs,
                                     &new_is_default_managed);
  if (!is_default_search_managed_ && !new_is_default_managed) {
    // We're not interested in cases where the default was and remains
    // unmanaged.  In that case, preferences have no impact on the default.
    return;
  }
  if (is_default_search_managed_ && new_is_default_managed) {
    // The default was managed and remains managed.  Update the default only
    // if it has changed; we don't want to respond to changes triggered by
    // SaveDefaultSearchProviderToPrefs.
    if (TemplateURLsHaveSamePrefs(default_search_provider_,
                                  new_default_from_prefs.get()))
      return;
    if (new_default_from_prefs.get() == NULL) {
      // default_search_provider_ can't be NULL otherwise
      // TemplateURLsHaveSamePrefs would have returned true.  Remove this now
      // invalid value.
      TemplateURL* old_default = default_search_provider_;
      bool success = SetDefaultSearchProviderNoNotify(NULL);
      DCHECK(success);
      RemoveNoNotify(old_default);
    } else if (default_search_provider_) {
      TemplateURLData data(new_default_from_prefs->data());
      data.created_by_policy = true;
      TemplateURL new_values(new_default_from_prefs->profile(), data);
      UIThreadSearchTermsData search_terms_data(
          default_search_provider_->profile());
      UpdateNoNotify(default_search_provider_, new_values, search_terms_data);
    } else {
      TemplateURL* new_template = NULL;
      if (new_default_from_prefs.get()) {
        TemplateURLData data(new_default_from_prefs->data());
        data.created_by_policy = true;
        new_template = new TemplateURL(profile_, data);
        if (!AddNoNotify(new_template, true))
          return;
      }
      bool success = SetDefaultSearchProviderNoNotify(new_template);
      DCHECK(success);
    }
  } else if (!is_default_search_managed_ && new_is_default_managed) {
    // The default used to be unmanaged and is now managed.  Add the new
    // managed default to the list of URLs and set it as default.
    is_default_search_managed_ = new_is_default_managed;
    TemplateURL* new_template = NULL;
    if (new_default_from_prefs.get()) {
      TemplateURLData data(new_default_from_prefs->data());
      data.created_by_policy = true;
      new_template = new TemplateURL(profile_, data);
      if (!AddNoNotify(new_template, true))
        return;
    }
    bool success = SetDefaultSearchProviderNoNotify(new_template);
    DCHECK(success);
  } else {
    // The default was managed and is no longer.
    DCHECK(is_default_search_managed_ && !new_is_default_managed);
    is_default_search_managed_ = new_is_default_managed;
    // If we had a default, delete the previous default if created by policy
    // and set a likely default.
    if ((default_search_provider_ != NULL) &&
        default_search_provider_->created_by_policy()) {
      TemplateURL* old_default = default_search_provider_;
      default_search_provider_ = NULL;
      RemoveNoNotify(old_default);
    }

    // The likely default should be from Sync if we were waiting on Sync.
    // Otherwise, it should be FindNewDefaultSearchProvider.
    TemplateURL* synced_default = GetPendingSyncedDefaultSearchProvider();
    if (synced_default) {
      base::AutoReset<DefaultSearchChangeOrigin> change_origin(
          &dsp_change_origin_, DSP_CHANGE_SYNC_NOT_MANAGED);
      pending_synced_default_search_ = false;
    }
    SetDefaultSearchProviderNoNotify(synced_default ? synced_default :
        FindNewDefaultSearchProvider());
  }
  NotifyObservers();
}

bool TemplateURLService::SetDefaultSearchProviderNoNotify(TemplateURL* url) {
  if (url) {
    if (std::find(template_urls_.begin(), template_urls_.end(), url) ==
        template_urls_.end())
      return false;
    // Extension keywords cannot be made default, as they're inherently async.
    DCHECK(!url->IsExtensionKeyword());
  }

  // Only bother reassigning |url| if it has changed. Notice that we don't just
  // early exit if they are equal, because |url| may have had its fields
  // changed, and needs to be persisted below (for example, when this is called
  // from UpdateNoNotify).
  if (default_search_provider_ != url) {
    UMA_HISTOGRAM_ENUMERATION(kDSPChangeHistogramName, dsp_change_origin_,
                              DSP_CHANGE_MAX);
    default_search_provider_ = url;
  }

  if (url) {
    // Don't mark the url as edited, otherwise we won't be able to rev the
    // template urls we ship with.
    url->data_.show_in_default_list = true;
    if (service_.get())
      service_->UpdateKeyword(url->data());

    if (url->url_ref().HasGoogleBaseURLs()) {
      GoogleURLTracker::RequestServerCheck(profile_);
#if defined(ENABLE_RLZ)
      RLZTracker::RecordProductEvent(rlz_lib::CHROME,
                                     RLZTracker::CHROME_OMNIBOX,
                                     rlz_lib::SET_TO_GOOGLE);
#endif
    }
  }

  if (!is_default_search_managed_) {
    SaveDefaultSearchProviderToPrefs(url);

    // If we are syncing, we want to set the synced pref that will notify other
    // instances to change their default to this new search provider.
    // Note: we don't update the pref if we're currently in the middle of
    // handling a sync operation. Sync operations from other clients are not
    // guaranteed to arrive together, and any client that deletes the default
    // needs to set a new default as well. If we update the default here, we're
    // likely to race with the update from the other client, resulting in
    // a possibly random default search provider.
    if (sync_processor_.get() && url && !url->sync_guid().empty() &&
        GetPrefs() && !processing_syncer_changes_) {
      GetPrefs()->SetString(prefs::kSyncedDefaultSearchProviderGUID,
                            url->sync_guid());
    }
  }

  if (service_.get())
    service_->SetDefaultSearchProvider(url);

  // Inform sync the change to the show_in_default_list flag.
  if (url)
    ProcessTemplateURLChange(FROM_HERE,
                             url,
                             syncer::SyncChange::ACTION_UPDATE);
  return true;
}

bool TemplateURLService::AddNoNotify(TemplateURL* template_url,
                                     bool newly_adding) {
  DCHECK(template_url);

  if (newly_adding) {
    DCHECK_EQ(kInvalidTemplateURLID, template_url->id());
    DCHECK(std::find(template_urls_.begin(), template_urls_.end(),
                     template_url) == template_urls_.end());
    template_url->data_.id = ++next_id_;
  }

  template_url->ResetKeywordIfNecessary(false);
  if (!template_url->IsExtensionKeyword()) {
    // Check whether |template_url|'s keyword conflicts with any already in the
    // model.
    TemplateURL* existing_keyword_turl =
        FindNonExtensionTemplateURLForKeyword(template_url->keyword());
    if (existing_keyword_turl != NULL) {
      DCHECK_NE(existing_keyword_turl, template_url);
      if (CanReplace(existing_keyword_turl)) {
        RemoveNoNotify(existing_keyword_turl);
      } else if (CanReplace(template_url)) {
        delete template_url;
        return false;
      } else {
        string16 new_keyword = UniquifyKeyword(*existing_keyword_turl, false);
        ResetTemplateURL(existing_keyword_turl,
                         existing_keyword_turl->short_name(), new_keyword,
                         existing_keyword_turl->url());
      }
    }
  }
  template_urls_.push_back(template_url);
  AddToMaps(template_url);

  if (newly_adding) {
    // Don't persist extension keywords to disk.  They'll get re-added on each
    // launch as the extensions are loaded.
    // TODO(mpcomplete): If we allow editing extension keywords, then those
    // should be persisted to disk and synced.
    if (service_.get() && !template_url->IsExtensionKeyword())
      service_->AddKeyword(template_url->data());

    // Inform sync of the addition. Note that this will assign a GUID to
    // template_url and add it to the guid_to_template_map_.
    ProcessTemplateURLChange(FROM_HERE,
                             template_url,
                             syncer::SyncChange::ACTION_ADD);
  }

  return true;
}

void TemplateURLService::RemoveNoNotify(TemplateURL* template_url) {
  TemplateURLVector::iterator i =
      std::find(template_urls_.begin(), template_urls_.end(), template_url);
  if (i == template_urls_.end())
    return;

  if (template_url == default_search_provider_) {
    // Should never delete the default search provider.
    NOTREACHED();
    return;
  }

  RemoveFromMaps(template_url);

  // Remove it from the vector containing all TemplateURLs.
  template_urls_.erase(i);

  // Extension keywords are not persisted.
  // TODO(mpcomplete): If we allow editing extension keywords, then those should
  // be persisted to disk and synced.
  if (service_.get() && !template_url->IsExtensionKeyword())
    service_->RemoveKeyword(template_url->id());

  // Inform sync of the deletion.
  ProcessTemplateURLChange(FROM_HERE,
                           template_url,
                           syncer::SyncChange::ACTION_DELETE);
  UMA_HISTOGRAM_ENUMERATION(kDeleteSyncedEngineHistogramName,
      DELETE_ENGINE_USER_ACTION, DELETE_ENGINE_MAX);

  if (profile_) {
    content::Source<Profile> source(profile_);
    TemplateURLID id = template_url->id();
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_TEMPLATE_URL_REMOVED,
        source,
        content::Details<TemplateURLID>(&id));
  }

  // We own the TemplateURL and need to delete it.
  delete template_url;
}

void TemplateURLService::NotifyObservers() {
  if (!loaded_)
    return;

  FOR_EACH_OBSERVER(TemplateURLServiceObserver, model_observers_,
                    OnTemplateURLServiceChanged());
}

// |template_urls| are the TemplateURLs loaded from the database.
// |default_search_provider| points to one of them, if it was set in the db.
// |default_from_prefs| is the default search provider from the preferences.
// Check |is_default_search_managed_| to determine if it was set by policy.
//
// This function removes from the vector and the database all the TemplateURLs
// that were set by policy, unless it is the current default search provider
// and matches what is set by a managed preference.
void TemplateURLService::RemoveProvidersCreatedByPolicy(
    TemplateURLVector* template_urls,
    TemplateURL** default_search_provider,
    TemplateURL* default_from_prefs) {
  DCHECK(template_urls);
  DCHECK(default_search_provider);
  for (TemplateURLVector::iterator i = template_urls->begin();
       i != template_urls->end(); ) {
    TemplateURL* template_url = *i;
    if (template_url->created_by_policy()) {
      if (template_url == *default_search_provider &&
          is_default_search_managed_ &&
          TemplateURLsHaveSamePrefs(template_url,
                                    default_from_prefs)) {
        // If the database specified a default search provider that was set
        // by policy, and the default search provider from the preferences
        // is also set by policy and they are the same, keep the entry in the
        // database and the |default_search_provider|.
        ++i;
        continue;
      }

      // The database loaded a managed |default_search_provider|, but it has
      // been updated in the prefs. Remove it from the database, and update the
      // |default_search_provider| pointer here.
      if (*default_search_provider &&
          (*default_search_provider)->id() == template_url->id())
        *default_search_provider = NULL;

      i = template_urls->erase(i);
      // Extension keywords are not persisted.
      // TODO(mpcomplete): If we allow editing extension keywords, then those
      // should be persisted to disk and synced.
      if (service_.get() && !template_url->IsExtensionKeyword())
        service_->RemoveKeyword(template_url->id());
      delete template_url;
    } else {
      ++i;
    }
  }
}

void TemplateURLService::ResetTemplateURLGUID(TemplateURL* url,
                                              const std::string& guid) {
  DCHECK(loaded_);
  DCHECK(!guid.empty());

  TemplateURLData data(url->data());
  data.sync_guid = guid;
  TemplateURL new_url(url->profile(), data);
  UIThreadSearchTermsData search_terms_data(url->profile());
  UpdateNoNotify(url, new_url, search_terms_data);
}

string16 TemplateURLService::UniquifyKeyword(const TemplateURL& turl,
                                             bool force) {
  if (!force) {
    // Already unique.
    if (!GetTemplateURLForKeyword(turl.keyword()))
      return turl.keyword();

    // First, try to return the generated keyword for the TemplateURL.
    GURL gurl(turl.url());
    if (gurl.is_valid()) {
      string16 keyword_candidate = GenerateKeyword(gurl);
      if (!GetTemplateURLForKeyword(keyword_candidate))
        return keyword_candidate;
    }
  }

  // We try to uniquify the keyword by appending a special character to the end.
  // This is a best-effort approach where we try to preserve the original
  // keyword and let the user do what they will after our attempt.
  string16 keyword_candidate(turl.keyword());
  do {
    keyword_candidate.append(ASCIIToUTF16("_"));
  } while (GetTemplateURLForKeyword(keyword_candidate));

  return keyword_candidate;
}

bool TemplateURLService::IsLocalTemplateURLBetter(
    const TemplateURL* local_turl,
    const TemplateURL* sync_turl) {
  DCHECK(GetTemplateURLForGUID(local_turl->sync_guid()));
  return local_turl->last_modified() > sync_turl->last_modified() ||
         local_turl->created_by_policy() ||
         local_turl== GetDefaultSearchProvider();
}

void TemplateURLService::ResolveSyncKeywordConflict(
    TemplateURL* unapplied_sync_turl,
    TemplateURL* applied_sync_turl,
    syncer::SyncChangeList* change_list) {
  DCHECK(loaded_);
  DCHECK(unapplied_sync_turl);
  DCHECK(applied_sync_turl);
  DCHECK(change_list);
  DCHECK_EQ(applied_sync_turl->keyword(), unapplied_sync_turl->keyword());
  DCHECK(!applied_sync_turl->IsExtensionKeyword());

  // Both |unapplied_sync_turl| and |applied_sync_turl| are known to Sync, so
  // don't delete either of them. Instead, determine which is "better" and
  // uniquify the other one, sending an update to the server for the updated
  // entry.
  const bool applied_turl_is_better =
      IsLocalTemplateURLBetter(applied_sync_turl, unapplied_sync_turl);
  TemplateURL* loser = applied_turl_is_better ?
      unapplied_sync_turl : applied_sync_turl;
  string16 new_keyword = UniquifyKeyword(*loser, false);
  DCHECK(!GetTemplateURLForKeyword(new_keyword));
  if (applied_turl_is_better) {
    // Just set the keyword of |unapplied_sync_turl|. The caller is responsible
    // for adding or updating unapplied_sync_turl in the local model.
    unapplied_sync_turl->data_.SetKeyword(new_keyword);
  } else {
    // Update |applied_sync_turl| in the local model with the new keyword.
    TemplateURLData data(applied_sync_turl->data());
    data.SetKeyword(new_keyword);
    TemplateURL new_turl(applied_sync_turl->profile(), data);
    UIThreadSearchTermsData search_terms_data(applied_sync_turl->profile());
    if (UpdateNoNotify(applied_sync_turl, new_turl, search_terms_data))
      NotifyObservers();
  }
  // The losing TemplateURL should have their keyword updated. Send a change to
  // the server to reflect this change.
  syncer::SyncData sync_data = CreateSyncDataFromTemplateURL(*loser);
  change_list->push_back(syncer::SyncChange(FROM_HERE,
      syncer::SyncChange::ACTION_UPDATE,
      sync_data));
}

void TemplateURLService::MergeInSyncTemplateURL(
    TemplateURL* sync_turl,
    const SyncDataMap& sync_data,
    syncer::SyncChangeList* change_list,
    SyncDataMap* local_data,
    syncer::SyncMergeResult* merge_result) {
  DCHECK(sync_turl);
  DCHECK(!GetTemplateURLForGUID(sync_turl->sync_guid()));
  DCHECK(IsFromSync(sync_turl, sync_data));

  TemplateURL* conflicting_turl =
      FindNonExtensionTemplateURLForKeyword(sync_turl->keyword());
  bool should_add_sync_turl = true;

  // If there was no TemplateURL in the local model that conflicts with
  // |sync_turl|, skip the following preparation steps and just add |sync_turl|
  // directly. Otherwise, modify |conflicting_turl| to make room for
  // |sync_turl|.
  if (conflicting_turl) {
    if (IsFromSync(conflicting_turl, sync_data)) {
      // |conflicting_turl| is already known to Sync, so we're not allowed to
      // remove it. In this case, we want to uniquify the worse one and send an
      // update for the changed keyword to sync. We can reuse the logic from
      // ResolveSyncKeywordConflict for this.
      ResolveSyncKeywordConflict(sync_turl, conflicting_turl, change_list);
      merge_result->set_num_items_modified(
          merge_result->num_items_modified() + 1);
    } else {
      // |conflicting_turl| is not yet known to Sync. If it is better, then we
      // want to transfer its values up to sync. Otherwise, we remove it and
      // allow the entry from Sync to overtake it in the model.
      const std::string guid = conflicting_turl->sync_guid();
      if (IsLocalTemplateURLBetter(conflicting_turl, sync_turl)) {
        ResetTemplateURLGUID(conflicting_turl, sync_turl->sync_guid());
        syncer::SyncData sync_data =
            CreateSyncDataFromTemplateURL(*conflicting_turl);
        change_list->push_back(syncer::SyncChange(
            FROM_HERE, syncer::SyncChange::ACTION_UPDATE, sync_data));
        if (conflicting_turl == GetDefaultSearchProvider() &&
            !pending_synced_default_search_) {
          // If we're not waiting for the Synced default to come in, we should
          // override the pref with our new GUID. If we are waiting for the
          // arrival of a synced default, setting the pref here would cause us
          // to lose the GUID we are waiting on.
          PrefService* prefs = GetPrefs();
          if (prefs) {
            prefs->SetString(prefs::kSyncedDefaultSearchProviderGUID,
                             conflicting_turl->sync_guid());
          }
        }
        // Note that in this case we do not add the Sync TemplateURL to the
        // local model, since we've effectively "merged" it in by updating the
        // local conflicting entry with its sync_guid.
        should_add_sync_turl = false;
        merge_result->set_num_items_modified(
            merge_result->num_items_modified() + 1);
      } else {
        // We guarantee that this isn't the local search provider. Otherwise,
        // local would have won.
        DCHECK(conflicting_turl != GetDefaultSearchProvider());
        Remove(conflicting_turl);
        merge_result->set_num_items_deleted(
            merge_result->num_items_deleted() + 1);
      }
      // This TemplateURL was either removed or overwritten in the local model.
      // Remove the entry from the local data so it isn't pushed up to Sync.
      local_data->erase(guid);
    }
  }

  if (should_add_sync_turl) {
    const std::string guid = sync_turl->sync_guid();
    // Force the local ID to kInvalidTemplateURLID so we can add it.
    TemplateURLData data(sync_turl->data());
    data.id = kInvalidTemplateURLID;
    Add(new TemplateURL(profile_, data));
    merge_result->set_num_items_added(
        merge_result->num_items_added() + 1);
  }
}

void TemplateURLService::SetDefaultSearchProviderIfNewlySynced(
    const std::string& guid) {
  // If we're not syncing or if default search is managed by policy, ignore.
  if (!sync_processor_.get() || is_default_search_managed_)
    return;

  PrefService* prefs = GetPrefs();
  if (prefs && pending_synced_default_search_ &&
      prefs->GetString(prefs::kSyncedDefaultSearchProviderGUID) == guid) {
    // Make sure this actually exists. We should not be calling this unless we
    // really just added this TemplateURL.
    TemplateURL* turl_from_sync = GetTemplateURLForGUID(guid);
    if (turl_from_sync && turl_from_sync->SupportsReplacement()) {
      base::AutoReset<DefaultSearchChangeOrigin> change_origin(
          &dsp_change_origin_, DSP_CHANGE_SYNC_ADD);
      SetDefaultSearchProvider(turl_from_sync);
    }
    pending_synced_default_search_ = false;
  }
}

TemplateURL* TemplateURLService::GetPendingSyncedDefaultSearchProvider() {
  PrefService* prefs = GetPrefs();
  if (!prefs || !pending_synced_default_search_)
    return NULL;

  // Could be NULL if no such thing exists.
  return GetTemplateURLForGUID(
      prefs->GetString(prefs::kSyncedDefaultSearchProviderGUID));
}

void TemplateURLService::PatchMissingSyncGUIDs(
    TemplateURLVector* template_urls) {
  DCHECK(template_urls);
  for (TemplateURLVector::iterator i = template_urls->begin();
       i != template_urls->end(); ++i) {
    TemplateURL* template_url = *i;
    DCHECK(template_url);
    // Extension keywords are never synced.
    // TODO(mpcomplete): If we allow editing extension keywords, then those
    // should be persisted to disk and synced.
    if (template_url->sync_guid().empty() &&
        !template_url->IsExtensionKeyword()) {
      template_url->data_.sync_guid = base::GenerateGUID();
      if (service_.get())
        service_->UpdateKeyword(template_url->data());
    }
  }
}
