// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/template_url_service.h"

#include <algorithm>
#include <utility>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/guid.h"
#include "base/i18n/case_conversion.h"
#include "base/memory/scoped_vector.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/rappor/rappor_service.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_host_to_urls_map.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service_client.h"
#include "components/search_engines/template_url_service_observer.h"
#include "components/search_engines/util.h"
#include "components/url_fixer/url_fixer.h"
#include "net/base/net_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_error_factory.h"
#include "sync/protocol/search_engine_specifics.pb.h"
#include "sync/protocol/sync.pb.h"
#include "url/gurl.h"

typedef SearchHostToURLsMap::TemplateURLSet TemplateURLSet;
typedef TemplateURLService::SyncDataMap SyncDataMap;

namespace {

bool IdenticalSyncGUIDs(const TemplateURLData* data, const TemplateURL* turl) {
  if (!data || !turl)
    return !data && !turl;

  return data->sync_guid == turl->sync_guid();
}

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
    std::string keyword = base::UTF16ToASCII((*it)->keyword());
    base::TrimString(keyword, "_", &keyword);
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


// TemplateURLService::LessWithPrefix -----------------------------------------

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


// TemplateURLService ---------------------------------------------------------

TemplateURLService::TemplateURLService(
    PrefService* prefs,
    scoped_ptr<SearchTermsData> search_terms_data,
    KeywordWebDataService* web_data_service,
    scoped_ptr<TemplateURLServiceClient> client,
    GoogleURLTracker* google_url_tracker,
    rappor::RapporService* rappor_service,
    const base::Closure& dsp_change_callback)
    : prefs_(prefs),
      search_terms_data_(search_terms_data.Pass()),
      web_data_service_(web_data_service),
      client_(client.Pass()),
      google_url_tracker_(google_url_tracker),
      rappor_service_(rappor_service),
      dsp_change_callback_(dsp_change_callback),
      provider_map_(new SearchHostToURLsMap),
      loaded_(false),
      load_failed_(false),
      load_handle_(0),
      default_search_provider_(NULL),
      next_id_(kInvalidTemplateURLID + 1),
      time_provider_(&base::Time::Now),
      models_associated_(false),
      processing_syncer_changes_(false),
      dsp_change_origin_(DSP_CHANGE_OTHER),
      default_search_manager_(
          prefs_,
          base::Bind(&TemplateURLService::OnDefaultSearchChange,
                     base::Unretained(this))) {
  DCHECK(search_terms_data_);
  Init(NULL, 0);
}

TemplateURLService::TemplateURLService(const Initializer* initializers,
                                       const int count)
    : prefs_(NULL),
      search_terms_data_(new SearchTermsData),
      web_data_service_(NULL),
      google_url_tracker_(NULL),
      rappor_service_(NULL),
      provider_map_(new SearchHostToURLsMap),
      loaded_(false),
      load_failed_(false),
      load_handle_(0),
      default_search_provider_(NULL),
      next_id_(kInvalidTemplateURLID + 1),
      time_provider_(&base::Time::Now),
      models_associated_(false),
      processing_syncer_changes_(false),
      dsp_change_origin_(DSP_CHANGE_OTHER),
      default_search_manager_(
          prefs_,
          base::Bind(&TemplateURLService::OnDefaultSearchChange,
                     base::Unretained(this))) {
  Init(initializers, count);
}

TemplateURLService::~TemplateURLService() {
  // |web_data_service_| should be deleted during Shutdown().
  DCHECK(!web_data_service_);
  STLDeleteElements(&template_urls_);
}

// static
bool TemplateURLService::LoadDefaultSearchProviderFromPrefs(
    PrefService* prefs,
    scoped_ptr<TemplateURLData>* default_provider_data,
    bool* is_managed) {
  if (!prefs || !prefs->HasPrefPath(prefs::kDefaultSearchProviderSearchURL) ||
      !prefs->HasPrefPath(prefs::kDefaultSearchProviderKeyword))
    return false;

  const PrefService::Preference* pref =
      prefs->FindPreference(prefs::kDefaultSearchProviderSearchURL);
  *is_managed = pref && pref->IsManaged();

  if (!prefs->GetBoolean(prefs::kDefaultSearchProviderEnabled)) {
    // The user doesn't want a default search provider.
    default_provider_data->reset(NULL);
    return true;
  }

  base::string16 name =
      base::UTF8ToUTF16(prefs->GetString(prefs::kDefaultSearchProviderName));
  base::string16 keyword =
      base::UTF8ToUTF16(prefs->GetString(prefs::kDefaultSearchProviderKeyword));
  if (keyword.empty())
    return false;
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
  std::string image_url =
      prefs->GetString(prefs::kDefaultSearchProviderImageURL);
  std::string new_tab_url =
      prefs->GetString(prefs::kDefaultSearchProviderNewTabURL);
  std::string search_url_post_params =
      prefs->GetString(prefs::kDefaultSearchProviderSearchURLPostParams);
  std::string suggest_url_post_params =
      prefs->GetString(prefs::kDefaultSearchProviderSuggestURLPostParams);
  std::string instant_url_post_params =
      prefs->GetString(prefs::kDefaultSearchProviderInstantURLPostParams);
  std::string image_url_post_params =
      prefs->GetString(prefs::kDefaultSearchProviderImageURLPostParams);
  std::string icon_url =
      prefs->GetString(prefs::kDefaultSearchProviderIconURL);
  std::string encodings =
      prefs->GetString(prefs::kDefaultSearchProviderEncodings);
  std::string id_string = prefs->GetString(prefs::kDefaultSearchProviderID);
  std::string prepopulate_id =
      prefs->GetString(prefs::kDefaultSearchProviderPrepopulateID);
  const base::ListValue* alternate_urls =
      prefs->GetList(prefs::kDefaultSearchProviderAlternateURLs);
  std::string search_terms_replacement_key = prefs->GetString(
      prefs::kDefaultSearchProviderSearchTermsReplacementKey);

  default_provider_data->reset(new TemplateURLData);
  (*default_provider_data)->short_name = name;
  (*default_provider_data)->SetKeyword(keyword);
  (*default_provider_data)->SetURL(search_url);
  (*default_provider_data)->suggestions_url = suggest_url;
  (*default_provider_data)->instant_url = instant_url;
  (*default_provider_data)->image_url = image_url;
  (*default_provider_data)->new_tab_url = new_tab_url;
  (*default_provider_data)->search_url_post_params = search_url_post_params;
  (*default_provider_data)->suggestions_url_post_params =
      suggest_url_post_params;
  (*default_provider_data)->instant_url_post_params = instant_url_post_params;
  (*default_provider_data)->image_url_post_params = image_url_post_params;
  (*default_provider_data)->favicon_url = GURL(icon_url);
  (*default_provider_data)->show_in_default_list = true;
  (*default_provider_data)->alternate_urls.clear();
  for (size_t i = 0; i < alternate_urls->GetSize(); ++i) {
    std::string alternate_url;
    if (alternate_urls->GetString(i, &alternate_url))
      (*default_provider_data)->alternate_urls.push_back(alternate_url);
  }
  (*default_provider_data)->search_terms_replacement_key =
      search_terms_replacement_key;
  base::SplitString(encodings, ';', &(*default_provider_data)->input_encodings);
  if (!id_string.empty() && !*is_managed) {
    int64 value;
    base::StringToInt64(id_string, &value);
    (*default_provider_data)->id = value;
  }
  if (!prepopulate_id.empty() && !*is_managed) {
    int value;
    base::StringToInt(prepopulate_id, &value);
    (*default_provider_data)->prepopulate_id = value;
  }
  return true;
}

// static
base::string16 TemplateURLService::CleanUserInputKeyword(
    const base::string16& keyword) {
  // Remove the scheme.
  base::string16 result(base::i18n::ToLower(keyword));
  base::TrimWhitespace(result, base::TRIM_ALL, &result);
  url::Component scheme_component;
  if (url::ExtractScheme(base::UTF16ToUTF8(keyword).c_str(),
                         static_cast<int>(keyword.length()),
                         &scheme_component)) {
    // If the scheme isn't "http" or "https", bail.  The user isn't trying to
    // type a web address, but rather an FTP, file:, or other scheme URL, or a
    // search query with some sort of initial operator (e.g. "site:").
    if (result.compare(0, scheme_component.end(),
                       base::ASCIIToUTF16(url::kHttpScheme)) &&
        result.compare(0, scheme_component.end(),
                       base::ASCIIToUTF16(url::kHttpsScheme)))
      return base::string16();

    // Include trailing ':'.
    result.erase(0, scheme_component.end() + 1);
    // Many schemes usually have "//" after them, so strip it too.
    const base::string16 after_scheme(base::ASCIIToUTF16("//"));
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
void TemplateURLService::SaveDefaultSearchProviderToPrefs(
    const TemplateURL* t_url,
    PrefService* prefs) {
  if (!prefs)
    return;

  bool enabled = false;
  std::string search_url;
  std::string suggest_url;
  std::string instant_url;
  std::string image_url;
  std::string new_tab_url;
  std::string search_url_post_params;
  std::string suggest_url_post_params;
  std::string instant_url_post_params;
  std::string image_url_post_params;
  std::string icon_url;
  std::string encodings;
  std::string short_name;
  std::string keyword;
  std::string id_string;
  std::string prepopulate_id;
  base::ListValue alternate_urls;
  std::string search_terms_replacement_key;
  if (t_url) {
    DCHECK_EQ(TemplateURL::NORMAL, t_url->GetType());
    enabled = true;
    search_url = t_url->url();
    suggest_url = t_url->suggestions_url();
    instant_url = t_url->instant_url();
    image_url = t_url->image_url();
    new_tab_url = t_url->new_tab_url();
    search_url_post_params = t_url->search_url_post_params();
    suggest_url_post_params = t_url->suggestions_url_post_params();
    instant_url_post_params = t_url->instant_url_post_params();
    image_url_post_params = t_url->image_url_post_params();
    GURL icon_gurl = t_url->favicon_url();
    if (!icon_gurl.is_empty())
      icon_url = icon_gurl.spec();
    encodings = JoinString(t_url->input_encodings(), ';');
    short_name = base::UTF16ToUTF8(t_url->short_name());
    keyword = base::UTF16ToUTF8(t_url->keyword());
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
  prefs->SetString(prefs::kDefaultSearchProviderImageURL, image_url);
  prefs->SetString(prefs::kDefaultSearchProviderNewTabURL, new_tab_url);
  prefs->SetString(prefs::kDefaultSearchProviderSearchURLPostParams,
                   search_url_post_params);
  prefs->SetString(prefs::kDefaultSearchProviderSuggestURLPostParams,
                   suggest_url_post_params);
  prefs->SetString(prefs::kDefaultSearchProviderInstantURLPostParams,
                   instant_url_post_params);
  prefs->SetString(prefs::kDefaultSearchProviderImageURLPostParams,
                   image_url_post_params);
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

bool TemplateURLService::CanReplaceKeyword(
    const base::string16& keyword,
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
    const base::string16& prefix,
    bool support_replacement_only,
    TemplateURLVector* matches) {
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
    if (!support_replacement_only ||
        i->second->url_ref().SupportsReplacement(search_terms_data()))
      matches->push_back(i->second);
  }
}

TemplateURL* TemplateURLService::GetTemplateURLForKeyword(
    const base::string16& keyword) {
  KeywordToTemplateMap::const_iterator elem(
      keyword_to_template_map_.find(keyword));
  if (elem != keyword_to_template_map_.end())
    return elem->second;
  return (!loaded_ &&
      initial_default_search_provider_.get() &&
      (initial_default_search_provider_->keyword() == keyword)) ?
      initial_default_search_provider_.get() : NULL;
}

TemplateURL* TemplateURLService::GetTemplateURLForGUID(
    const std::string& sync_guid) {
  GUIDToTemplateMap::const_iterator elem(guid_to_template_map_.find(sync_guid));
  if (elem != guid_to_template_map_.end())
    return elem->second;
  return (!loaded_ &&
      initial_default_search_provider_.get() &&
      (initial_default_search_provider_->sync_guid() == sync_guid)) ?
      initial_default_search_provider_.get() : NULL;
}

TemplateURL* TemplateURLService::GetTemplateURLForHost(
    const std::string& host) {
  if (loaded_)
    return provider_map_->GetTemplateURLForHost(host);
  TemplateURL* initial_dsp = initial_default_search_provider_.get();
  if (!initial_dsp)
    return NULL;
  return (initial_dsp->GenerateSearchURL(search_terms_data()).host() == host) ?
      initial_dsp : NULL;
}

bool TemplateURLService::Add(TemplateURL* template_url) {
  KeywordWebDataService::BatchModeScoper scoper(web_data_service_.get());
  if (!AddNoNotify(template_url, true))
    return false;
  NotifyObservers();
  return true;
}

void TemplateURLService::AddWithOverrides(TemplateURL* template_url,
                                          const base::string16& short_name,
                                          const base::string16& keyword,
                                          const std::string& url) {
  DCHECK(!keyword.empty());
  DCHECK(!url.empty());
  template_url->data_.short_name = short_name;
  template_url->data_.SetKeyword(keyword);
  template_url->SetURL(url);
  Add(template_url);
}

void TemplateURLService::AddExtensionControlledTURL(
    TemplateURL* template_url,
    scoped_ptr<TemplateURL::AssociatedExtensionInfo> info) {
  DCHECK(loaded_);
  DCHECK(template_url);
  DCHECK_EQ(kInvalidTemplateURLID, template_url->id());
  DCHECK(info);
  DCHECK_NE(TemplateURL::NORMAL, info->type);
  DCHECK_EQ(info->wants_to_be_default_engine,
            template_url->show_in_default_list());
  DCHECK(!FindTemplateURLForExtension(info->extension_id, info->type));
  template_url->extension_info_.swap(info);

  KeywordWebDataService::BatchModeScoper scoper(web_data_service_.get());
  if (AddNoNotify(template_url, true)) {
    if (template_url->extension_info_->wants_to_be_default_engine)
      UpdateExtensionDefaultSearchEngine();
    NotifyObservers();
  }
}

void TemplateURLService::Remove(TemplateURL* template_url) {
  RemoveNoNotify(template_url);
  NotifyObservers();
}

void TemplateURLService::RemoveExtensionControlledTURL(
    const std::string& extension_id,
    TemplateURL::Type type) {
  DCHECK(loaded_);
  TemplateURL* url = FindTemplateURLForExtension(extension_id, type);
  if (!url)
    return;
  // NULL this out so that we can call RemoveNoNotify.
  // UpdateExtensionDefaultSearchEngine will cause it to be reset.
  if (default_search_provider_ == url)
    default_search_provider_ = NULL;
  KeywordWebDataService::BatchModeScoper scoper(web_data_service_.get());
  RemoveNoNotify(url);
  UpdateExtensionDefaultSearchEngine();
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
  KeywordWebDataService::BatchModeScoper scoper(web_data_service_.get());
  for (size_t i = 0; i < template_urls_.size();) {
    if (template_urls_[i]->date_created() >= created_after &&
        (created_before.is_null() ||
         template_urls_[i]->date_created() < created_before) &&
        CanReplace(template_urls_[i]) &&
        (o.is_empty() ||
         template_urls_[i]->GenerateSearchURL(
             search_terms_data()).GetOrigin() == o)) {
      RemoveNoNotify(template_urls_[i]);
      should_notify = true;
    } else {
      ++i;
    }
  }
  if (should_notify)
    NotifyObservers();
}

void TemplateURLService::RegisterOmniboxKeyword(
     const std::string& extension_id,
     const std::string& extension_name,
     const std::string& keyword,
     const std::string& template_url_string) {
  DCHECK(loaded_);

  if (FindTemplateURLForExtension(extension_id,
                                  TemplateURL::OMNIBOX_API_EXTENSION))
    return;

  TemplateURLData data;
  data.short_name = base::UTF8ToUTF16(extension_name);
  data.SetKeyword(base::UTF8ToUTF16(keyword));
  data.SetURL(template_url_string);
  TemplateURL* url = new TemplateURL(data);
  scoped_ptr<TemplateURL::AssociatedExtensionInfo> info(
      new TemplateURL::AssociatedExtensionInfo(
          TemplateURL::OMNIBOX_API_EXTENSION, extension_id));
  AddExtensionControlledTURL(url, info.Pass());
}

TemplateURLService::TemplateURLVector TemplateURLService::GetTemplateURLs() {
  return template_urls_;
}

void TemplateURLService::IncrementUsageCount(TemplateURL* url) {
  DCHECK(url);
  // Extension-controlled search engines are not persisted.
  if (url->GetType() == TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION)
    return;
  if (std::find(template_urls_.begin(), template_urls_.end(), url) ==
      template_urls_.end())
    return;
  ++url->data_.usage_count;

  if (web_data_service_)
    web_data_service_->UpdateKeyword(url->data());
}

void TemplateURLService::ResetTemplateURL(TemplateURL* url,
                                          const base::string16& title,
                                          const base::string16& keyword,
                                          const std::string& search_url) {
  if (ResetTemplateURLNoNotify(url, title, keyword, search_url))
    NotifyObservers();
}

bool TemplateURLService::CanMakeDefault(const TemplateURL* url) {
  return
      ((default_search_provider_source_ == DefaultSearchManager::FROM_USER) ||
       (default_search_provider_source_ ==
        DefaultSearchManager::FROM_FALLBACK)) &&
      (url != GetDefaultSearchProvider()) &&
      url->url_ref().SupportsReplacement(search_terms_data()) &&
      (url->GetType() == TemplateURL::NORMAL);
}

void TemplateURLService::SetUserSelectedDefaultSearchProvider(
    TemplateURL* url) {
  // Omnibox keywords cannot be made default. Extension-controlled search
  // engines can be made default only by the extension itself because they
  // aren't persisted.
  DCHECK(!url || (url->GetType() == TemplateURL::NORMAL));
  if (load_failed_) {
    // Skip the DefaultSearchManager, which will persist to user preferences.
    if ((default_search_provider_source_ == DefaultSearchManager::FROM_USER) ||
        (default_search_provider_source_ ==
         DefaultSearchManager::FROM_FALLBACK)) {
      ApplyDefaultSearchChange(url ? &url->data() : NULL,
                               DefaultSearchManager::FROM_USER);
    }
  } else {
    // We rely on the DefaultSearchManager to call OnDefaultSearchChange if, in
    // fact, the effective DSE changes.
    if (url)
      default_search_manager_.SetUserSelectedDefaultSearchEngine(url->data());
    else
      default_search_manager_.ClearUserSelectedDefaultSearchEngine();
  }
}

TemplateURL* TemplateURLService::GetDefaultSearchProvider() {
  return loaded_ ?
    default_search_provider_ : initial_default_search_provider_.get();
}

bool TemplateURLService::IsSearchResultsPageFromDefaultSearchProvider(
    const GURL& url) {
  TemplateURL* default_provider = GetDefaultSearchProvider();
  return default_provider &&
      default_provider->IsSearchURL(url, search_terms_data());
}

bool TemplateURLService::IsExtensionControlledDefaultSearch() {
  return default_search_provider_source_ ==
      DefaultSearchManager::FROM_EXTENSION;
}

void TemplateURLService::RepairPrepopulatedSearchEngines() {
  // Can't clean DB if it hasn't been loaded.
  DCHECK(loaded());

  if ((default_search_provider_source_ == DefaultSearchManager::FROM_USER) ||
      (default_search_provider_source_ ==
          DefaultSearchManager::FROM_FALLBACK)) {
    // Clear |default_search_provider_| in case we want to remove the engine it
    // points to. This will get reset at the end of the function anyway.
    default_search_provider_ = NULL;
  }

  size_t default_search_provider_index = 0;
  ScopedVector<TemplateURLData> prepopulated_urls =
      TemplateURLPrepopulateData::GetPrepopulatedEngines(
          prefs_, &default_search_provider_index);
  DCHECK(!prepopulated_urls.empty());
  ActionsFromPrepopulateData actions(CreateActionsFromCurrentPrepopulateData(
      &prepopulated_urls, template_urls_, default_search_provider_));

  KeywordWebDataService::BatchModeScoper scoper(web_data_service_.get());

  // Remove items.
  for (std::vector<TemplateURL*>::iterator i = actions.removed_engines.begin();
       i < actions.removed_engines.end(); ++i)
    RemoveNoNotify(*i);

  // Edit items.
  for (EditedEngines::iterator i(actions.edited_engines.begin());
       i < actions.edited_engines.end(); ++i) {
    TemplateURL new_values(i->second);
    UpdateNoNotify(i->first, new_values);
  }

  // Add items.
  for (std::vector<TemplateURLData>::const_iterator i =
           actions.added_engines.begin();
       i < actions.added_engines.end();
       ++i) {
    AddNoNotify(new TemplateURL(*i), true);
  }

  base::AutoReset<DefaultSearchChangeOrigin> change_origin(
      &dsp_change_origin_, DSP_CHANGE_PROFILE_RESET);

  default_search_manager_.ClearUserSelectedDefaultSearchEngine();

  if (!default_search_provider_) {
    // If the default search provider came from a user pref we would have been
    // notified of the new (fallback-provided) value in
    // ClearUserSelectedDefaultSearchEngine() above. Since we are here, the
    // value was presumably originally a fallback value (which may have been
    // repaired).
    DefaultSearchManager::Source source;
    const TemplateURLData* new_dse =
        default_search_manager_.GetDefaultSearchEngine(&source);
    // ApplyDefaultSearchChange will notify observers once it is done.
    ApplyDefaultSearchChange(new_dse, source);
  } else {
    NotifyObservers();
  }
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

  if (web_data_service_)
    load_handle_ = web_data_service_->GetKeywords(this);
  else
    ChangeToLoadedState();
}

scoped_ptr<TemplateURLService::Subscription>
    TemplateURLService::RegisterOnLoadedCallback(
        const base::Closure& callback) {
  return loaded_ ?
      scoped_ptr<TemplateURLService::Subscription>() :
      on_loaded_callbacks_.Add(callback);
}

void TemplateURLService::OnWebDataServiceRequestDone(
    KeywordWebDataService::Handle h,
    const WDTypedResult* result) {
  // Reset the load_handle so that we don't try and cancel the load in
  // the destructor.
  load_handle_ = 0;

  if (!result) {
    // Results are null if the database went away or (most likely) wasn't
    // loaded.
    load_failed_ = true;
    web_data_service_ = NULL;
    ChangeToLoadedState();
    return;
  }

  TemplateURLVector template_urls;
  int new_resource_keyword_version = 0;
  GetSearchProvidersUsingKeywordResult(
      *result,
      web_data_service_.get(),
      prefs_,
      &template_urls,
      (default_search_provider_source_ == DefaultSearchManager::FROM_USER) ?
          initial_default_search_provider_.get() : NULL,
      search_terms_data(),
      &new_resource_keyword_version,
      &pre_sync_deletes_);

  KeywordWebDataService::BatchModeScoper scoper(web_data_service_.get());

  PatchMissingSyncGUIDs(&template_urls);
  SetTemplateURLs(&template_urls);

  // This initializes provider_map_ which should be done before
  // calling UpdateKeywordSearchTermsForURL.
  // This also calls NotifyObservers.
  ChangeToLoadedState();

  // Index any visits that occurred before we finished loading.
  for (size_t i = 0; i < visits_to_add_.size(); ++i)
    UpdateKeywordSearchTermsForURL(visits_to_add_[i]);
  visits_to_add_.clear();

  if (new_resource_keyword_version)
    web_data_service_->SetBuiltinKeywordVersion(new_resource_keyword_version);

  if (default_search_provider_) {
    UMA_HISTOGRAM_ENUMERATION(
        "Search.DefaultSearchProviderType",
        TemplateURLPrepopulateData::GetEngineType(
            *default_search_provider_, search_terms_data()),
        SEARCH_ENGINE_MAX);

    if (rappor_service_) {
      rappor_service_->RecordSample(
          "Search.DefaultSearchProvider",
          rappor::ETLD_PLUS_ONE_RAPPOR_TYPE,
          net::registry_controlled_domains::GetDomainAndRegistry(
              default_search_provider_->url_ref().GetHost(search_terms_data()),
              net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES));
    }
  }
}

base::string16 TemplateURLService::GetKeywordShortName(
    const base::string16& keyword,
    bool* is_omnibox_api_extension_keyword) {
  const TemplateURL* template_url = GetTemplateURLForKeyword(keyword);

  // TODO(sky): Once LocationBarView adds a listener to the TemplateURLService
  // to track changes to the model, this should become a DCHECK.
  if (template_url) {
    *is_omnibox_api_extension_keyword =
        template_url->GetType() == TemplateURL::OMNIBOX_API_EXTENSION;
    return template_url->AdjustedShortNameForLocaleDirection();
  }
  *is_omnibox_api_extension_keyword = false;
  return base::string16();
}

void TemplateURLService::OnHistoryURLVisited(const URLVisitedDetails& details) {
  if (!loaded_)
    visits_to_add_.push_back(details);
  else
    UpdateKeywordSearchTermsForURL(details);
}

void TemplateURLService::Shutdown() {
  // This check has to be done at Shutdown() instead of in the dtor to ensure
  // that no clients of KeywordWebDataService are holding ptrs to it after the
  // first phase of the KeyedService Shutdown() process.
  if (load_handle_) {
    DCHECK(web_data_service_.get());
    web_data_service_->CancelRequest(load_handle_);
  }
  web_data_service_ = NULL;
}

syncer::SyncDataList TemplateURLService::GetAllSyncData(
    syncer::ModelType type) const {
  DCHECK_EQ(syncer::SEARCH_ENGINES, type);

  syncer::SyncDataList current_data;
  for (TemplateURLVector::const_iterator iter = template_urls_.begin();
      iter != template_urls_.end(); ++iter) {
    // We don't sync keywords managed by policy.
    if ((*iter)->created_by_policy())
      continue;
    // We don't sync extension-controlled search engines.
    if ((*iter)->GetType() == TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION)
      continue;
    current_data.push_back(CreateSyncDataFromTemplateURL(**iter));
  }

  return current_data;
}

syncer::SyncError TemplateURLService::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  if (!models_associated_) {
    syncer::SyncError error(FROM_HERE,
                            syncer::SyncError::DATATYPE_ERROR,
                            "Models not yet associated.",
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

  KeywordWebDataService::BatchModeScoper scoper(web_data_service_.get());

  syncer::SyncChangeList new_changes;
  syncer::SyncError error;
  for (syncer::SyncChangeList::const_iterator iter = change_list.begin();
      iter != change_list.end(); ++iter) {
    DCHECK_EQ(syncer::SEARCH_ENGINES, iter->sync_data().GetDataType());

    std::string guid =
        iter->sync_data().GetSpecifics().search_engine().sync_guid();
    TemplateURL* existing_turl = GetTemplateURLForGUID(guid);
    scoped_ptr<TemplateURL> turl(CreateTemplateURLFromTemplateURLAndSyncData(
        prefs_, search_terms_data(), existing_turl, iter->sync_data(),
        &new_changes));
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
        error = sync_error_factory_->CreateAndUploadError(
            FROM_HERE,
            "ProcessSyncChanges failed on ChangeType ACTION_DELETE");
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
        base::string16 updated_keyword = UniquifyKeyword(*existing_turl, true);
        TemplateURLData data(existing_turl->data());
        data.SetKeyword(updated_keyword);
        TemplateURL new_turl(data);
        if (UpdateNoNotify(existing_turl, new_turl))
          NotifyObservers();

        syncer::SyncData sync_data = CreateSyncDataFromTemplateURL(new_turl);
        new_changes.push_back(syncer::SyncChange(FROM_HERE,
                                                 syncer::SyncChange::ACTION_ADD,
                                                 sync_data));
        // Ignore the delete attempt. This means we never end up resetting the
        // default search provider due to an ACTION_DELETE from sync.
        continue;
      }

      Remove(existing_turl);
    } else if (iter->change_type() == syncer::SyncChange::ACTION_ADD) {
      if (existing_turl) {
        error = sync_error_factory_->CreateAndUploadError(
            FROM_HERE,
            "ProcessSyncChanges failed on ChangeType ACTION_ADD");
        continue;
      }
      const std::string guid = turl->sync_guid();
      if (existing_keyword_turl) {
        // Resolve any conflicts so we can safely add the new entry.
        ResolveSyncKeywordConflict(turl.get(), existing_keyword_turl,
                                   &new_changes);
      }
      base::AutoReset<DefaultSearchChangeOrigin> change_origin(
          &dsp_change_origin_, DSP_CHANGE_SYNC_ADD);
      // Force the local ID to kInvalidTemplateURLID so we can add it.
      TemplateURLData data(turl->data());
      data.id = kInvalidTemplateURLID;
      TemplateURL* added = new TemplateURL(data);
      if (Add(added))
        MaybeUpdateDSEAfterSync(added);
    } else if (iter->change_type() == syncer::SyncChange::ACTION_UPDATE) {
      if (!existing_turl) {
        error = sync_error_factory_->CreateAndUploadError(
            FROM_HERE,
            "ProcessSyncChanges failed on ChangeType ACTION_UPDATE");
        continue;
      }
      if (existing_keyword_turl && (existing_keyword_turl != existing_turl)) {
        // Resolve any conflicts with other entries so we can safely update the
        // keyword.
        ResolveSyncKeywordConflict(turl.get(), existing_keyword_turl,
                                   &new_changes);
      }
      if (UpdateNoNotify(existing_turl, *turl)) {
        NotifyObservers();
        MaybeUpdateDSEAfterSync(existing_turl);
      }
    } else {
      // We've unexpectedly received an ACTION_INVALID.
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

  // Disable sync if we failed to load.
  if (load_failed_) {
    merge_result.set_error(syncer::SyncError(
        FROM_HERE, syncer::SyncError::DATATYPE_ERROR,
        "Local database load failed.", syncer::SEARCH_ENGINES));
    return merge_result;
  }

  sync_processor_ = sync_processor.Pass();
  sync_error_factory_ = sync_error_factory.Pass();

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

  KeywordWebDataService::BatchModeScoper scoper(web_data_service_.get());

  merge_result.set_num_items_before_association(local_data_map.size());
  for (SyncDataMap::const_iterator iter = sync_data_map.begin();
      iter != sync_data_map.end(); ++iter) {
    TemplateURL* local_turl = GetTemplateURLForGUID(iter->first);
    scoped_ptr<TemplateURL> sync_turl(
        CreateTemplateURLFromTemplateURLAndSyncData(
            prefs_, search_terms_data(), local_turl, iter->second,
            &new_changes));
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
        if (UpdateNoNotify(local_turl, *sync_turl))
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

  // Avoid syncing keywords managed by policy.
  if (turl->created_by_policy())
    return;

  // Avoid syncing extension-controlled search engines.
  if (turl->GetType() == TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION)
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
  se_specifics->set_short_name(base::UTF16ToUTF8(turl.short_name()));
  se_specifics->set_keyword(base::UTF16ToUTF8(turl.keyword()));
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
  if (!turl.image_url().empty())
    se_specifics->set_image_url(turl.image_url());
  se_specifics->set_new_tab_url(turl.new_tab_url());
  if (!turl.search_url_post_params().empty())
    se_specifics->set_search_url_post_params(turl.search_url_post_params());
  if (!turl.suggestions_url_post_params().empty()) {
    se_specifics->set_suggestions_url_post_params(
        turl.suggestions_url_post_params());
  }
  if (!turl.instant_url_post_params().empty())
    se_specifics->set_instant_url_post_params(turl.instant_url_post_params());
  if (!turl.image_url_post_params().empty())
    se_specifics->set_image_url_post_params(turl.image_url_post_params());
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
    PrefService* prefs,
    const SearchTermsData& search_terms_data,
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
  data.short_name = base::UTF8ToUTF16(specifics.short_name());
  data.originating_url = GURL(specifics.originating_url());
  base::string16 keyword(base::UTF8ToUTF16(specifics.keyword()));
  // NOTE: Once this code has shipped in a couple of stable releases, we can
  // probably remove the migration portion, comment out the
  // "autogenerate_keyword" field entirely in the .proto file, and fold the
  // empty keyword case into the "delete data" block above.
  bool reset_keyword =
      specifics.autogenerate_keyword() || specifics.keyword().empty();
  if (reset_keyword)
    keyword = base::ASCIIToUTF16("dummy");  // Will be replaced below.
  DCHECK(!keyword.empty());
  data.SetKeyword(keyword);
  data.SetURL(specifics.url());
  data.suggestions_url = specifics.suggestions_url();
  data.instant_url = specifics.instant_url();
  data.image_url = specifics.image_url();
  data.new_tab_url = specifics.new_tab_url();
  data.search_url_post_params = specifics.search_url_post_params();
  data.suggestions_url_post_params = specifics.suggestions_url_post_params();
  data.instant_url_post_params = specifics.instant_url_post_params();
  data.image_url_post_params = specifics.image_url_post_params();
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

  TemplateURL* turl = new TemplateURL(data);
  // If this TemplateURL matches a built-in prepopulated template URL, it's
  // possible that sync is trying to modify fields that should not be touched.
  // Revert these fields to the built-in values.
  UpdateTemplateURLIfPrepopulated(turl, prefs);
  DCHECK_NE(TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION, turl->GetType());
  if (reset_keyword || deduped) {
    if (reset_keyword)
      turl->ResetKeywordIfNecessary(search_terms_data, true);
    syncer::SyncData sync_data = CreateSyncDataFromTemplateURL(*turl);
    change_list->push_back(syncer::SyncChange(FROM_HERE,
                                              syncer::SyncChange::ACTION_UPDATE,
                                              sync_data));
  } else if (turl->IsGoogleSearchURLWithReplaceableKeyword(search_terms_data)) {
    if (!existing_turl) {
      // We're adding a new TemplateURL that uses the Google base URL, so set
      // its keyword appropriately for the local environment.
      turl->ResetKeywordIfNecessary(search_terms_data, false);
    } else if (existing_turl->IsGoogleSearchURLWithReplaceableKeyword(
        search_terms_data)) {
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

void TemplateURLService::SetKeywordSearchTermsForURL(
    const TemplateURL* t_url,
    const GURL& url,
    const base::string16& term) {
  if (client_)
    client_->SetKeywordSearchTermsForURL(url, t_url->id(), term);
}

void TemplateURLService::Init(const Initializer* initializers,
                              int num_initializers) {
  if (client_)
    client_->SetOwner(this);

  // GoogleURLTracker is not created in tests.
  if (google_url_tracker_) {
    google_url_updated_subscription_ =
        google_url_tracker_->RegisterCallback(base::Bind(
            &TemplateURLService::GoogleBaseURLChanged, base::Unretained(this)));
  }

  if (prefs_) {
    pref_change_registrar_.Init(prefs_);
    pref_change_registrar_.Add(
        prefs::kSyncedDefaultSearchProviderGUID,
        base::Bind(
            &TemplateURLService::OnSyncedDefaultSearchProviderGUIDChanged,
            base::Unretained(this)));
  }

  DefaultSearchManager::Source source = DefaultSearchManager::FROM_USER;
  TemplateURLData* dse =
      default_search_manager_.GetDefaultSearchEngine(&source);
  ApplyDefaultSearchChange(dse, source);

  if (num_initializers > 0) {
    // This path is only hit by test code and is used to simulate a loaded
    // TemplateURLService.
    ChangeToLoadedState();

    // Add specific initializers, if any.
    KeywordWebDataService::BatchModeScoper scoper(web_data_service_.get());
    for (int i(0); i < num_initializers; ++i) {
      DCHECK(initializers[i].keyword);
      DCHECK(initializers[i].url);
      DCHECK(initializers[i].content);

      // TemplateURLService ends up owning the TemplateURL, don't try and free
      // it.
      TemplateURLData data;
      data.short_name = base::UTF8ToUTF16(initializers[i].content);
      data.SetKeyword(base::UTF8ToUTF16(initializers[i].keyword));
      data.SetURL(initializers[i].url);
      TemplateURL* template_url = new TemplateURL(data);
      AddNoNotify(template_url, true);

      // Set the first provided identifier to be the default.
      if (i == 0)
        default_search_manager_.SetUserSelectedDefaultSearchEngine(data);
    }
  }

  // Request a server check for the correct Google URL if Google is the
  // default search engine.
  RequestGoogleURLTrackerServerCheckIfNecessary();
}

void TemplateURLService::RemoveFromMaps(TemplateURL* template_url) {
  const base::string16& keyword = template_url->keyword();
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
      // non-Omnibox API TemplateURL with a given keyword.
      if ((turl != template_url) && (turl->keyword() == keyword) &&
          (!best_fallback ||
           (best_fallback->GetType() != TemplateURL::OMNIBOX_API_EXTENSION) ||
           ((turl->GetType() == TemplateURL::OMNIBOX_API_EXTENSION) &&
            (turl->id() > best_fallback->id()))))
        best_fallback = turl;
    }
    if (best_fallback)
      keyword_to_template_map_[keyword] = best_fallback;
    else
      keyword_to_template_map_.erase(keyword);
  }

  if (!template_url->sync_guid().empty())
    guid_to_template_map_.erase(template_url->sync_guid());
  // |provider_map_| is only initialized after loading has completed.
  if (loaded_) {
    provider_map_->Remove(template_url);
  }
}

void TemplateURLService::AddToMaps(TemplateURL* template_url) {
  bool template_url_is_omnibox_api =
      template_url->GetType() == TemplateURL::OMNIBOX_API_EXTENSION;
  const base::string16& keyword = template_url->keyword();
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
    bool existing_url_is_omnibox_api =
        existing_url->GetType() == TemplateURL::OMNIBOX_API_EXTENSION;
    DCHECK(existing_url_is_omnibox_api || template_url_is_omnibox_api);
    if (existing_url_is_omnibox_api ?
        !CanReplace(template_url) : CanReplace(existing_url))
      keyword_to_template_map_[keyword] = template_url;
  }

  if (!template_url->sync_guid().empty())
    guid_to_template_map_[template_url->sync_guid()] = template_url;
  // |provider_map_| is only initialized after loading has completed.
  if (loaded_)
    provider_map_->Add(template_url, search_terms_data());
}

// Helper for partition() call in next function.
bool HasValidID(TemplateURL* t_url) {
  return t_url->id() != kInvalidTemplateURLID;
}

void TemplateURLService::SetTemplateURLs(TemplateURLVector* urls) {
  // Partition the URLs first, instead of implementing the loops below by simply
  // scanning the input twice.  While it's not supposed to happen normally, it's
  // possible for corrupt databases to return multiple entries with the same
  // keyword.  In this case, the first loop may delete the first entry when
  // adding the second.  If this happens, the second loop must not attempt to
  // access the deleted entry.  Partitioning ensures this constraint.
  TemplateURLVector::iterator first_invalid(
      std::partition(urls->begin(), urls->end(), HasValidID));

  // First, add the items that already have id's, so that the next_id_ gets
  // properly set.
  for (TemplateURLVector::const_iterator i = urls->begin(); i != first_invalid;
       ++i) {
    next_id_ = std::max(next_id_, (*i)->id());
    AddNoNotify(*i, false);
  }

  // Next add the new items that don't have id's.
  for (TemplateURLVector::const_iterator i = first_invalid; i != urls->end();
       ++i)
    AddNoNotify(*i, true);

  // Clear the input vector to reduce the chance callers will try to use a
  // (possibly deleted) entry.
  urls->clear();
}

void TemplateURLService::ChangeToLoadedState() {
  DCHECK(!loaded_);

  provider_map_->Init(template_urls_, search_terms_data());
  loaded_ = true;

  // This will cause a call to NotifyObservers().
  ApplyDefaultSearchChangeNoMetrics(
      initial_default_search_provider_ ?
          &initial_default_search_provider_->data() : NULL,
      default_search_provider_source_);
  initial_default_search_provider_.reset();
  on_loaded_callbacks_.Notify();
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
    const base::string16& keyword) {
  TemplateURL* keyword_turl = GetTemplateURLForKeyword(keyword);
  if (!keyword_turl || (keyword_turl->GetType() == TemplateURL::NORMAL))
    return keyword_turl;
  // The extension keyword in the model may be hiding a replaceable
  // non-extension keyword.  Look for it.
  for (TemplateURLVector::const_iterator i(template_urls_.begin());
       i != template_urls_.end(); ++i) {
    if (((*i)->GetType() == TemplateURL::NORMAL) &&
        ((*i)->keyword() == keyword))
      return *i;
  }
  return NULL;
}

bool TemplateURLService::UpdateNoNotify(TemplateURL* existing_turl,
                                        const TemplateURL& new_values) {
  DCHECK(existing_turl);
  if (std::find(template_urls_.begin(), template_urls_.end(), existing_turl) ==
      template_urls_.end())
    return false;

  base::string16 old_keyword(existing_turl->keyword());
  keyword_to_template_map_.erase(old_keyword);
  if (!existing_turl->sync_guid().empty())
    guid_to_template_map_.erase(existing_turl->sync_guid());

  // |provider_map_| is only initialized after loading has completed.
  if (loaded_)
    provider_map_->Remove(existing_turl);

  TemplateURLID previous_id = existing_turl->id();
  existing_turl->CopyFrom(new_values);
  existing_turl->data_.id = previous_id;

  if (loaded_) {
    provider_map_->Add(existing_turl, search_terms_data());
  }

  const base::string16& keyword = existing_turl->keyword();
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
    TemplateURL* existing_keyword_turl = i->second;
    if (existing_keyword_turl->GetType() != TemplateURL::NORMAL) {
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

  if (web_data_service_)
    web_data_service_->UpdateKeyword(existing_turl->data());

  // Inform sync of the update.
  ProcessTemplateURLChange(
      FROM_HERE, existing_turl, syncer::SyncChange::ACTION_UPDATE);

  if (default_search_provider_ == existing_turl &&
      default_search_provider_source_ == DefaultSearchManager::FROM_USER) {
    default_search_manager_.SetUserSelectedDefaultSearchEngine(
        default_search_provider_->data());
  }
  return true;
}

// static
void TemplateURLService::UpdateTemplateURLIfPrepopulated(
    TemplateURL* template_url,
    PrefService* prefs) {
  int prepopulate_id = template_url->prepopulate_id();
  if (template_url->prepopulate_id() == 0)
    return;

  size_t default_search_index;
  ScopedVector<TemplateURLData> prepopulated_urls =
      TemplateURLPrepopulateData::GetPrepopulatedEngines(
          prefs, &default_search_index);

  for (size_t i = 0; i < prepopulated_urls.size(); ++i) {
    if (prepopulated_urls[i]->prepopulate_id == prepopulate_id) {
      MergeIntoPrepopulatedEngineData(template_url, prepopulated_urls[i]);
      template_url->CopyFrom(TemplateURL(*prepopulated_urls[i]));
    }
  }
}

void TemplateURLService::MaybeUpdateDSEAfterSync(TemplateURL* synced_turl) {
  if (prefs_ &&
      (synced_turl->sync_guid() ==
          prefs_->GetString(prefs::kSyncedDefaultSearchProviderGUID))) {
    default_search_manager_.SetUserSelectedDefaultSearchEngine(
        synced_turl->data());
  }
}

void TemplateURLService::UpdateKeywordSearchTermsForURL(
    const URLVisitedDetails& details) {
  if (!details.url.is_valid())
    return;

  const TemplateURLSet* urls_for_host =
      provider_map_->GetURLsForHost(details.url.host());
  if (!urls_for_host)
    return;

  for (TemplateURLSet::const_iterator i = urls_for_host->begin();
       i != urls_for_host->end(); ++i) {
    base::string16 search_terms;
    if ((*i)->ExtractSearchTermsFromURL(details.url, search_terms_data(),
                                        &search_terms) &&
        !search_terms.empty()) {
      if (details.is_keyword_transition) {
        // The visit is the result of the user entering a keyword, generate a
        // KEYWORD_GENERATED visit for the KEYWORD so that the keyword typed
        // count is boosted.
        AddTabToSearchVisit(**i);
      }
      SetKeywordSearchTermsForURL(*i, details.url, search_terms);
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

  if (!client_)
    return;

  GURL url(
      url_fixer::FixupURL(base::UTF16ToUTF8(t_url.keyword()), std::string()));
  if (!url.is_valid())
    return;

  // Synthesize a visit for the keyword. This ensures the url for the keyword is
  // autocompleted even if the user doesn't type the url in directly.
  client_->AddKeywordGeneratedVisit(url);
}

void TemplateURLService::RequestGoogleURLTrackerServerCheckIfNecessary() {
  if (default_search_provider_ &&
      default_search_provider_->HasGoogleBaseURLs(search_terms_data()) &&
      google_url_tracker_)
    google_url_tracker_->RequestServerCheck(false);
}

void TemplateURLService::GoogleBaseURLChanged() {
  if (!loaded_)
    return;

  KeywordWebDataService::BatchModeScoper scoper(web_data_service_.get());
  bool something_changed = false;
  for (TemplateURLVector::iterator i(template_urls_.begin());
       i != template_urls_.end(); ++i) {
    TemplateURL* t_url = *i;
    if (t_url->HasGoogleBaseURLs(search_terms_data())) {
      TemplateURL updated_turl(t_url->data());
      updated_turl.ResetKeywordIfNecessary(search_terms_data(), false);
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
      UpdateNoNotify(t_url, updated_turl);
    }
  }
  if (something_changed)
    NotifyObservers();
}

void TemplateURLService::OnDefaultSearchChange(
    const TemplateURLData* data,
    DefaultSearchManager::Source source) {
  if (prefs_ && (source == DefaultSearchManager::FROM_USER) &&
      ((source != default_search_provider_source_) ||
       !IdenticalSyncGUIDs(data, GetDefaultSearchProvider()))) {
    prefs_->SetString(prefs::kSyncedDefaultSearchProviderGUID, data->sync_guid);
  }
  ApplyDefaultSearchChange(data, source);
}

void TemplateURLService::ApplyDefaultSearchChange(
    const TemplateURLData* data,
    DefaultSearchManager::Source source) {
  if (!ApplyDefaultSearchChangeNoMetrics(data, source))
    return;

  UMA_HISTOGRAM_ENUMERATION(
      "Search.DefaultSearchChangeOrigin", dsp_change_origin_, DSP_CHANGE_MAX);

  if (GetDefaultSearchProvider() &&
      GetDefaultSearchProvider()->HasGoogleBaseURLs(search_terms_data()) &&
      !dsp_change_callback_.is_null())
    dsp_change_callback_.Run();
}

bool TemplateURLService::ApplyDefaultSearchChangeNoMetrics(
    const TemplateURLData* data,
    DefaultSearchManager::Source source) {
  if (!loaded_) {
    // Set |initial_default_search_provider_| from the preferences. This is
    // mainly so we can hold ownership until we get to the point where the list
    // of keywords from Web Data is the owner of everything including the
    // default.
    bool changed = TemplateURL::MatchesData(
        initial_default_search_provider_.get(), data, search_terms_data());
    initial_default_search_provider_.reset(
        data ? new TemplateURL(*data) : NULL);
    default_search_provider_source_ = source;
    return changed;
  }

  // Prevent recursion if we update the value stored in default_search_manager_.
  // Note that we exclude the case of data == NULL because that could cause a
  // false positive for recursion when the initial_default_search_provider_ is
  // NULL due to policy. We'll never actually get recursion with data == NULL.
  if (source == default_search_provider_source_ && data != NULL &&
      TemplateURL::MatchesData(default_search_provider_, data,
                               search_terms_data()))
    return false;

  // This may be deleted later. Use exclusively for pointer comparison to detect
  // a change.
  TemplateURL* previous_default_search_engine = default_search_provider_;

  KeywordWebDataService::BatchModeScoper scoper(web_data_service_.get());
  if (default_search_provider_source_ == DefaultSearchManager::FROM_POLICY ||
      source == DefaultSearchManager::FROM_POLICY) {
    // We do this both to remove any no-longer-applicable policy-defined DSE as
    // well as to add the new one, if appropriate.
    UpdateProvidersCreatedByPolicy(
        &template_urls_,
        source == DefaultSearchManager::FROM_POLICY ? data : NULL);
  }

  if (!data) {
    default_search_provider_ = NULL;
  } else if (source == DefaultSearchManager::FROM_EXTENSION) {
    default_search_provider_ = FindMatchingExtensionTemplateURL(
        *data, TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION);
  } else if (source == DefaultSearchManager::FROM_FALLBACK) {
    default_search_provider_ =
        FindPrepopulatedTemplateURL(data->prepopulate_id);
    if (default_search_provider_) {
      TemplateURLData update_data(*data);
      update_data.sync_guid = default_search_provider_->sync_guid();
      if (!default_search_provider_->safe_for_autoreplace()) {
        update_data.safe_for_autoreplace = false;
        update_data.SetKeyword(default_search_provider_->keyword());
        update_data.short_name = default_search_provider_->short_name();
      }
      UpdateNoNotify(default_search_provider_, TemplateURL(update_data));
    } else {
      // Normally the prepopulated fallback should be present in
      // |template_urls_|, but in a few cases it might not be:
      // (1) Tests that initialize the TemplateURLService in peculiar ways.
      // (2) If the user deleted the pre-populated default and we subsequently
      // lost their user-selected value.
      TemplateURL* new_dse = new TemplateURL(*data);
      if (AddNoNotify(new_dse, true))
        default_search_provider_ = new_dse;
    }
  } else if (source == DefaultSearchManager::FROM_USER) {
    default_search_provider_ = GetTemplateURLForGUID(data->sync_guid);
    if (!default_search_provider_ && data->prepopulate_id) {
      default_search_provider_ =
          FindPrepopulatedTemplateURL(data->prepopulate_id);
    }
    TemplateURLData new_data(*data);
    new_data.show_in_default_list = true;
    if (default_search_provider_) {
      UpdateNoNotify(default_search_provider_, TemplateURL(new_data));
    } else {
      new_data.id = kInvalidTemplateURLID;
      TemplateURL* new_dse = new TemplateURL(new_data);
      if (AddNoNotify(new_dse, true))
        default_search_provider_ = new_dse;
    }
    if (default_search_provider_ && prefs_) {
      prefs_->SetString(prefs::kSyncedDefaultSearchProviderGUID,
                        default_search_provider_->sync_guid());
    }

  }

  default_search_provider_source_ = source;

  bool changed = default_search_provider_ != previous_default_search_engine;
  if (changed)
    RequestGoogleURLTrackerServerCheckIfNecessary();

  NotifyObservers();

  return changed;
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

  template_url->ResetKeywordIfNecessary(search_terms_data(), false);
  // Check whether |template_url|'s keyword conflicts with any already in the
  // model.
  TemplateURL* existing_keyword_turl =
      GetTemplateURLForKeyword(template_url->keyword());

  // Check whether |template_url|'s keyword conflicts with any already in the
  // model.  Note that we can reach here during the loading phase while
  // processing the template URLs from the web data service.  In this case,
  // GetTemplateURLForKeyword() will look not only at what's already in the
  // model, but at the |initial_default_search_provider_|.  Since this engine
  // will presumably also be present in the web data, we need to double-check
  // that any "pre-existing" entries we find are actually coming from
  // |template_urls_|, lest we detect a "conflict" between the
  // |initial_default_search_provider_| and the web data version of itself.
  if (existing_keyword_turl &&
      (std::find(template_urls_.begin(), template_urls_.end(),
                 existing_keyword_turl) != template_urls_.end())) {
    DCHECK_NE(existing_keyword_turl, template_url);
    // Only replace one of the TemplateURLs if they are either both extensions,
    // or both not extensions.
    bool are_same_type = existing_keyword_turl->GetType() ==
        template_url->GetType();
    if (CanReplace(existing_keyword_turl) && are_same_type) {
      RemoveNoNotify(existing_keyword_turl);
    } else if (CanReplace(template_url) && are_same_type) {
      delete template_url;
      return false;
    } else {
      base::string16 new_keyword =
          UniquifyKeyword(*existing_keyword_turl, false);
      ResetTemplateURLNoNotify(existing_keyword_turl,
                               existing_keyword_turl->short_name(), new_keyword,
                               existing_keyword_turl->url());
    }
  }
  template_urls_.push_back(template_url);
  AddToMaps(template_url);

  if (newly_adding &&
      (template_url->GetType() !=
          TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION)) {
    if (web_data_service_)
      web_data_service_->AddKeyword(template_url->data());

    // Inform sync of the addition. Note that this will assign a GUID to
    // template_url and add it to the guid_to_template_map_.
    ProcessTemplateURLChange(FROM_HERE,
                             template_url,
                             syncer::SyncChange::ACTION_ADD);
  }

  return true;
}

void TemplateURLService::RemoveNoNotify(TemplateURL* template_url) {
  DCHECK(template_url != default_search_provider_);

  TemplateURLVector::iterator i =
      std::find(template_urls_.begin(), template_urls_.end(), template_url);
  if (i == template_urls_.end())
    return;

  RemoveFromMaps(template_url);

  // Remove it from the vector containing all TemplateURLs.
  template_urls_.erase(i);

  if (template_url->GetType() != TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION) {
    if (web_data_service_)
      web_data_service_->RemoveKeyword(template_url->id());

    // Inform sync of the deletion.
    ProcessTemplateURLChange(FROM_HERE,
                             template_url,
                             syncer::SyncChange::ACTION_DELETE);

    UMA_HISTOGRAM_ENUMERATION(kDeleteSyncedEngineHistogramName,
                              DELETE_ENGINE_USER_ACTION, DELETE_ENGINE_MAX);
  }

  if (loaded_ && client_)
    client_->DeleteAllSearchTermsForKeyword(template_url->id());

  // We own the TemplateURL and need to delete it.
  delete template_url;
}

bool TemplateURLService::ResetTemplateURLNoNotify(
    TemplateURL* url,
    const base::string16& title,
    const base::string16& keyword,
    const std::string& search_url) {
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
  return UpdateNoNotify(url, TemplateURL(data));
}

void TemplateURLService::NotifyObservers() {
  if (!loaded_)
    return;

  FOR_EACH_OBSERVER(TemplateURLServiceObserver, model_observers_,
                    OnTemplateURLServiceChanged());
}

// |template_urls| are the TemplateURLs loaded from the database.
// |default_from_prefs| is the default search provider from the preferences, or
// NULL if the DSE is not policy-defined.
//
// This function removes from the vector and the database all the TemplateURLs
// that were set by policy, unless it is the current default search provider, in
// which case it is updated with the data from prefs.
void TemplateURLService::UpdateProvidersCreatedByPolicy(
    TemplateURLVector* template_urls,
    const TemplateURLData* default_from_prefs) {
  DCHECK(template_urls);

  for (TemplateURLVector::iterator i = template_urls->begin();
       i != template_urls->end(); ) {
    TemplateURL* template_url = *i;
    if (template_url->created_by_policy()) {
      if (default_from_prefs &&
          TemplateURL::MatchesData(template_url, default_from_prefs,
                                   search_terms_data())) {
        // If the database specified a default search provider that was set
        // by policy, and the default search provider from the preferences
        // is also set by policy and they are the same, keep the entry in the
        // database and the |default_search_provider|.
        default_search_provider_ = template_url;
        // Prevent us from saving any other entries, or creating a new one.
        default_from_prefs = NULL;
        ++i;
        continue;
      }

      RemoveFromMaps(template_url);
      i = template_urls->erase(i);
      if (web_data_service_)
        web_data_service_->RemoveKeyword(template_url->id());
      delete template_url;
    } else {
      ++i;
    }
  }

  if (default_from_prefs) {
    default_search_provider_ = NULL;
    default_search_provider_source_ = DefaultSearchManager::FROM_POLICY;
    TemplateURLData new_data(*default_from_prefs);
    if (new_data.sync_guid.empty())
      new_data.sync_guid = base::GenerateGUID();
    new_data.created_by_policy = true;
    TemplateURL* new_dse = new TemplateURL(new_data);
    if (AddNoNotify(new_dse, true))
      default_search_provider_ = new_dse;
  }
}

void TemplateURLService::ResetTemplateURLGUID(TemplateURL* url,
                                              const std::string& guid) {
  DCHECK(loaded_);
  DCHECK(!guid.empty());

  TemplateURLData data(url->data());
  data.sync_guid = guid;
  UpdateNoNotify(url, TemplateURL(data));
}

base::string16 TemplateURLService::UniquifyKeyword(const TemplateURL& turl,
                                                   bool force) {
  if (!force) {
    // Already unique.
    if (!GetTemplateURLForKeyword(turl.keyword()))
      return turl.keyword();

    // First, try to return the generated keyword for the TemplateURL (except
    // for extensions, as their keywords are not associated with their URLs).
    GURL gurl(turl.url());
    if (gurl.is_valid() &&
        (turl.GetType() != TemplateURL::OMNIBOX_API_EXTENSION)) {
      base::string16 keyword_candidate = TemplateURL::GenerateKeyword(gurl);
      if (!GetTemplateURLForKeyword(keyword_candidate))
        return keyword_candidate;
    }
  }

  // We try to uniquify the keyword by appending a special character to the end.
  // This is a best-effort approach where we try to preserve the original
  // keyword and let the user do what they will after our attempt.
  base::string16 keyword_candidate(turl.keyword());
  do {
    keyword_candidate.append(base::ASCIIToUTF16("_"));
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
  DCHECK_NE(TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION,
            applied_sync_turl->GetType());

  // Both |unapplied_sync_turl| and |applied_sync_turl| are known to Sync, so
  // don't delete either of them. Instead, determine which is "better" and
  // uniquify the other one, sending an update to the server for the updated
  // entry.
  const bool applied_turl_is_better =
      IsLocalTemplateURLBetter(applied_sync_turl, unapplied_sync_turl);
  TemplateURL* loser = applied_turl_is_better ?
      unapplied_sync_turl : applied_sync_turl;
  base::string16 new_keyword = UniquifyKeyword(*loser, false);
  DCHECK(!GetTemplateURLForKeyword(new_keyword));
  if (applied_turl_is_better) {
    // Just set the keyword of |unapplied_sync_turl|. The caller is responsible
    // for adding or updating unapplied_sync_turl in the local model.
    unapplied_sync_turl->data_.SetKeyword(new_keyword);
  } else {
    // Update |applied_sync_turl| in the local model with the new keyword.
    TemplateURLData data(applied_sync_turl->data());
    data.SetKeyword(new_keyword);
    if (UpdateNoNotify(applied_sync_turl, TemplateURL(data)))
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
    // Force the local ID to kInvalidTemplateURLID so we can add it.
    TemplateURLData data(sync_turl->data());
    data.id = kInvalidTemplateURLID;
    TemplateURL* added = new TemplateURL(data);
    base::AutoReset<DefaultSearchChangeOrigin> change_origin(
        &dsp_change_origin_, DSP_CHANGE_SYNC_ADD);
    if (Add(added))
      MaybeUpdateDSEAfterSync(added);
    merge_result->set_num_items_added(
        merge_result->num_items_added() + 1);
  }
}

void TemplateURLService::PatchMissingSyncGUIDs(
    TemplateURLVector* template_urls) {
  DCHECK(template_urls);
  for (TemplateURLVector::iterator i = template_urls->begin();
       i != template_urls->end(); ++i) {
    TemplateURL* template_url = *i;
    DCHECK(template_url);
    if (template_url->sync_guid().empty() &&
        (template_url->GetType() !=
            TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION)) {
      template_url->data_.sync_guid = base::GenerateGUID();
      if (web_data_service_)
        web_data_service_->UpdateKeyword(template_url->data());
    }
  }
}

void TemplateURLService::OnSyncedDefaultSearchProviderGUIDChanged() {
  base::AutoReset<DefaultSearchChangeOrigin> change_origin(
      &dsp_change_origin_, DSP_CHANGE_SYNC_PREF);

  std::string new_guid =
      prefs_->GetString(prefs::kSyncedDefaultSearchProviderGUID);
  if (new_guid.empty()) {
    default_search_manager_.ClearUserSelectedDefaultSearchEngine();
    return;
  }

  TemplateURL* turl = GetTemplateURLForGUID(new_guid);
  if (turl)
    default_search_manager_.SetUserSelectedDefaultSearchEngine(turl->data());
}

TemplateURL* TemplateURLService::FindPrepopulatedTemplateURL(
    int prepopulated_id) {
  for (TemplateURLVector::const_iterator i = template_urls_.begin();
       i != template_urls_.end(); ++i) {
    if ((*i)->prepopulate_id() == prepopulated_id)
      return *i;
  }
  return NULL;
}

TemplateURL* TemplateURLService::FindTemplateURLForExtension(
    const std::string& extension_id,
    TemplateURL::Type type) {
  DCHECK_NE(TemplateURL::NORMAL, type);
  for (TemplateURLVector::const_iterator i = template_urls_.begin();
       i != template_urls_.end(); ++i) {
    if ((*i)->GetType() == type &&
        (*i)->GetExtensionId() == extension_id)
      return *i;
  }
  return NULL;
}

TemplateURL* TemplateURLService::FindMatchingExtensionTemplateURL(
    const TemplateURLData& data,
    TemplateURL::Type type) {
  DCHECK_NE(TemplateURL::NORMAL, type);
  for (TemplateURLVector::const_iterator i = template_urls_.begin();
       i != template_urls_.end(); ++i) {
    if ((*i)->GetType() == type &&
        TemplateURL::MatchesData(*i, &data, search_terms_data()))
      return *i;
  }
  return NULL;
}

void TemplateURLService::UpdateExtensionDefaultSearchEngine() {
  TemplateURL* most_recently_intalled_default = NULL;
  for (TemplateURLVector::const_iterator i = template_urls_.begin();
       i != template_urls_.end(); ++i) {
    if (((*i)->GetType() == TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION) &&
        (*i)->extension_info_->wants_to_be_default_engine &&
        (*i)->SupportsReplacement(search_terms_data()) &&
        (!most_recently_intalled_default ||
         (most_recently_intalled_default->extension_info_->install_time <
             (*i)->extension_info_->install_time)))
      most_recently_intalled_default = *i;
  }

  if (most_recently_intalled_default) {
    base::AutoReset<DefaultSearchChangeOrigin> change_origin(
        &dsp_change_origin_, DSP_CHANGE_OVERRIDE_SETTINGS_EXTENSION);
    default_search_manager_.SetExtensionControlledDefaultSearchEngine(
        most_recently_intalled_default->data());
  } else {
    default_search_manager_.ClearExtensionControlledDefaultSearchEngine();
  }
}
