// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/template_url_service.h"

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/i18n/case_conversion.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/google/google_url_tracker.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/protector/base_setting_change.h"
#include "chrome/browser/protector/protector_service.h"
#include "chrome/browser/protector/protector_service_factory.h"
#include "chrome/browser/protector/protector_utils.h"
#include "chrome/browser/rlz/rlz.h"
#include "chrome/browser/search_engines/search_host_to_urls_map.h"
#include "chrome/browser/search_engines/search_terms_data.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_prepopulate_data.h"
#include "chrome/browser/search_engines/template_url_service_observer.h"
#include "chrome/browser/search_engines/util.h"
#include "chrome/browser/sync/api/sync_change.h"
#include "chrome/browser/sync/api/sync_error_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/env_vars.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/guid.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/notification_service.h"
#include "net/base/net_util.h"
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
      (url1->input_encodings() == url2->input_encodings());
}

TemplateURL* FirstPotentialDefaultEngine(
    const TemplateURLService::TemplateURLVector& template_urls) {
  for (TemplateURLService::TemplateURLVector::const_iterator i(
       template_urls.begin()); i != template_urls.end(); ++i) {
    if (!(*i)->IsExtensionKeyword() && (*i)->SupportsReplacement())
      return *i;
  }
  return NULL;
}

// If |change_list| contains ACTION_UPDATEs followed by more ACTION_UPDATEs or
// ACTION_ADDs for the same GUID, remove all but the last one.
//
// Removing UPDATE before ADD is important.  This can happen if
// ResolveSyncKeywordConflict() changes a local TemplateURL that hasn't actually
// been seen by the server yet.  In this case sending the UPDATE first might
// confuse the sync server.
//
// Removing UPDATE before UPDATE, OTOH, is not really necessary as the server
// will coalesce these before other clients see them; however it's easy to do in
// conjunction with the filtering for UPDATE-before-ADD and saves bandwidth.
void PreventDuplicateGUIDUpdates(SyncChangeList* change_list) {
  for (size_t i = change_list->size(); i > 1; ) {
    --i;  // Prevent underflow that could occur if we did this in the loop body.
    const SyncChange& change_i = (*change_list)[i];
    if ((change_i.change_type() != SyncChange::ACTION_ADD) &&
        (change_i.change_type() != SyncChange::ACTION_UPDATE))
      continue;
    std::string guid(
        change_i.sync_data().GetSpecifics().search_engine().sync_guid());
    for (size_t j = 0; j < i; ) {
      const SyncChange& change_j = (*change_list)[j];
      if ((change_j.change_type() == SyncChange::ACTION_UPDATE) &&
          (change_j.sync_data().GetSpecifics().search_engine().sync_guid() ==
              guid)) {
        change_list->erase(change_list->begin() + j);
        --i;
      } else {
        ++j;
      }
    }
  }
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
      pending_synced_default_search_(false) {
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
      pending_synced_default_search_(false) {
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
  string16 keyword(net::StripWWW(UTF8ToUTF16(url.host())));
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
      ASCIIToUTF16(kReplacementTerm), TemplateURLRef::NO_SUGGESTIONS_AVAILABLE,
      string16(), search_terms_data));
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
  TemplateURL* t_url = provider_map_->GetTemplateURLForHost(host);
  if (t_url)
    return t_url;
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


void TemplateURLService::RegisterExtensionKeyword(const Extension* extension) {
  // TODO(mpcomplete): disable the keyword when the extension is disabled.
  if (extension->omnibox_keyword().empty())
    return;

  Load();
  if (!loaded_) {
    pending_extension_ids_.push_back(extension->id());
    return;
  }

  if (!GetTemplateURLForExtension(extension)) {
    TemplateURLData data;
    data.short_name = UTF8ToUTF16(extension->name());
    data.SetKeyword(UTF8ToUTF16(extension->omnibox_keyword()));
    // This URL is not actually used for navigation. It holds the extension's
    // ID, as well as forcing the TemplateURL to be treated as a search keyword.
    data.SetURL(std::string(chrome::kExtensionScheme) + "://" +
        extension->id() + "/?q={searchTerms}");
    Add(new TemplateURL(profile_, data));
  }
}

void TemplateURLService::UnregisterExtensionKeyword(
    const Extension* extension) {
  if (loaded_) {
    TemplateURL* url = GetTemplateURLForExtension(extension);
    if (url)
      Remove(url);
  } else {
    PendingExtensionIDs::iterator i = std::find(pending_extension_ids_.begin(),
        pending_extension_ids_.end(), extension->id());
    if (i != pending_extension_ids_.end())
      pending_extension_ids_.erase(i);
  }
}

TemplateURL* TemplateURLService::GetTemplateURLForExtension(
    const Extension* extension) {
  for (TemplateURLVector::const_iterator i = template_urls_.begin();
       i != template_urls_.end(); ++i) {
    if ((*i)->IsExtensionKeyword() &&
        ((*i)->url_ref().GetHost() == extension->id()))
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
  if (UpdateNoNotify(url, new_url))
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

  if (!service_.get())
    service_ = profile_->GetWebDataService(Profile::EXPLICIT_ACCESS);

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
      &template_urls, &default_search_provider, &new_resource_keyword_version);

  bool database_specified_a_default = (default_search_provider != NULL);

  // Check if default search provider is now managed.
  scoped_ptr<TemplateURL> default_from_prefs;
  LoadDefaultSearchProviderFromPrefs(&default_from_prefs,
                                     &is_default_search_managed_);

  // Check if the default search provider has been changed in Web Data by
  // another program. No immediate action is performed because the default
  // search may be changed below by Sync which effectively undoes the hijacking.
  bool is_default_search_hijacked = false;
  TemplateURL* hijacked_default_search_provider = NULL;
  scoped_ptr<TemplateURL> backup_default_search_provider;
  // No check is required if the default search is managed.
  // |DidDefaultSearchProviderChange| must always be called because it will
  // take care of the unowned backup default search provider instance.
  if (DidDefaultSearchProviderChange(*result, profile_,
                                     &backup_default_search_provider) &&
      !is_default_search_managed_) {
    hijacked_default_search_provider = default_search_provider;
    is_default_search_hijacked = true;
  }

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

  bool check_if_default_search_valid = !is_default_search_managed_;

#if defined(ENABLE_PROTECTOR_SERVICE)
  // Don't do anything if the default search provider has been changed since the
  // check at the beginning (overridden by Sync).
  if (is_default_search_hijacked &&
      default_search_provider_ == hijacked_default_search_provider) {
    // The histograms should be reported even when Protector is disabled.
    scoped_ptr<protector::BaseSettingChange> change(
        protector::CreateDefaultSearchProviderChange(
            hijacked_default_search_provider,
            backup_default_search_provider.release()));
    if (protector::IsEnabled()) {
      protector::ProtectorService* protector_service =
          protector::ProtectorServiceFactory::GetForProfile(profile());
      DCHECK(protector_service);
      protector_service->ShowChange(change.release());
    } else {
      // Protector is turned off: set the current default search to itself
      // to update the backup and sign it. Otherwise, change will be reported
      // every time when keywords are loaded until a search provider is added.
      service_->SetDefaultSearchProvider(default_search_provider_);
    }
    // The default search provider sanity check makes no sense in this case
    // because ProtectorService is going to change default search eventually.
    check_if_default_search_valid = false;
  }
#endif

  if (check_if_default_search_valid) {
    bool has_default_search_provider = default_search_provider_ != NULL &&
        default_search_provider_->SupportsReplacement();
    UMA_HISTOGRAM_BOOLEAN("Search.HasDefaultSearchProvider",
                          has_default_search_provider);
    // Ensure that default search provider exists. See http://crbug.com/116952.
    if (!has_default_search_provider) {
      bool success =
          SetDefaultSearchProviderNoNotify(FindNewDefaultSearchProvider());
      DCHECK(success);
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
    if (loaded_)
      GoogleBaseURLChanged();
  } else if (type == chrome::NOTIFICATION_PREF_CHANGED) {
    // Listen for changes to the default search from Sync.
    DCHECK_EQ(std::string(prefs::kSyncedDefaultSearchProviderGUID),
              *content::Details<std::string>(details).ptr());
    PrefService* prefs = GetPrefs();
    TemplateURL* new_default_search = GetTemplateURLForGUID(
        prefs->GetString(prefs::kSyncedDefaultSearchProviderGUID));
    if (new_default_search && !is_default_search_managed_) {
      if (new_default_search != GetDefaultSearchProvider()) {
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
  } else {
    NOTREACHED();
  }
}

SyncDataList TemplateURLService::GetAllSyncData(
    syncable::ModelType type) const {
  DCHECK_EQ(syncable::SEARCH_ENGINES, type);

  SyncDataList current_data;
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

SyncError TemplateURLService::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const SyncChangeList& change_list) {
  if (!models_associated_) {
    SyncError error(FROM_HERE, "Models not yet associated.",
                    syncable::SEARCH_ENGINES);
    return error;
  }
  DCHECK(loaded_);

  AutoReset<bool> processing_changes(&processing_syncer_changes_, true);

  SyncChangeList new_changes;
  SyncError error;
  for (SyncChangeList::const_iterator iter = change_list.begin();
      iter != change_list.end(); ++iter) {
    DCHECK_EQ(syncable::SEARCH_ENGINES, iter->sync_data().GetDataType());

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
    if (iter->change_type() == SyncChange::ACTION_DELETE && existing_turl) {
      bool delete_default = (existing_turl == GetDefaultSearchProvider());

      if (delete_default && is_default_search_managed_) {
        NOTREACHED() << "Tried to delete managed default search provider";
      } else {
        if (delete_default)
          default_search_provider_ = NULL;

        Remove(existing_turl);

        if (delete_default)
          SetDefaultSearchProvider(FindNewDefaultSearchProvider());
      }
    } else if (iter->change_type() == SyncChange::ACTION_ADD &&
               !existing_turl) {
      std::string guid = turl->sync_guid();
      if (existing_keyword_turl) {
        ResolveSyncKeywordConflict(turl.get(), existing_keyword_turl,
                                   &new_changes);
      }
      // Force the local ID to kInvalidTemplateURLID so we can add it.
      TemplateURLData data(turl->data());
      data.id = kInvalidTemplateURLID;
      Add(new TemplateURL(profile_, data));

      // Possibly set the newly added |turl| as the default search provider.
      SetDefaultSearchProviderIfNewlySynced(guid);
    } else if (iter->change_type() == SyncChange::ACTION_UPDATE &&
               existing_turl) {
      // Possibly resolve a keyword conflict if they have the same keywords but
      // are not the same entry.
      if (existing_keyword_turl && existing_keyword_turl != existing_turl) {
        ResolveSyncKeywordConflict(turl.get(), existing_keyword_turl,
                                   &new_changes);
      }
      if (UpdateNoNotify(existing_turl, *turl))
        NotifyObservers();
    } else {
      // Something really unexpected happened. Either we received an
      // ACTION_INVALID, or Sync is in a crazy state:
      //  . Trying to DELETE or UPDATE a non-existent search engine.
      //  . Trying to ADD a search engine that already exists.
      NOTREACHED() << "Unexpected sync change state.";
      error = sync_error_factory_->CreateAndUploadError(
            FROM_HERE,
            "ProcessSyncChanges failed on ChangeType " +
                SyncChange::ChangeTypeToString(iter->change_type()));
    }
  }
  PreventDuplicateGUIDUpdates(&new_changes);

  // If something went wrong, we want to prematurely exit to avoid pushing
  // inconsistent data to Sync. We return the last error we received.
  if (error.IsSet())
    return error;

  error = sync_processor_->ProcessSyncChanges(from_here, new_changes);

  return error;
}

SyncError TemplateURLService::MergeDataAndStartSyncing(
    syncable::ModelType type,
    const SyncDataList& initial_sync_data,
    scoped_ptr<SyncChangeProcessor> sync_processor,
    scoped_ptr<SyncErrorFactory> sync_error_factory) {
  DCHECK(loaded_);
  DCHECK_EQ(type, syncable::SEARCH_ENGINES);
  DCHECK(!sync_processor_.get());
  DCHECK(sync_processor.get());
  DCHECK(sync_error_factory.get());
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
  AutoReset<bool> processing_changes(&processing_syncer_changes_, true);

  SyncChangeList new_changes;

  // Build maps of our sync GUIDs to SyncData.
  SyncDataMap local_data_map = CreateGUIDToSyncDataMap(
      GetAllSyncData(syncable::SEARCH_ENGINES));
  SyncDataMap sync_data_map = CreateGUIDToSyncDataMap(initial_sync_data);

  for (SyncDataMap::const_iterator iter = sync_data_map.begin();
      iter != sync_data_map.end(); ++iter) {
    TemplateURL* local_turl = GetTemplateURLForGUID(iter->first);
    scoped_ptr<TemplateURL> sync_turl(
        CreateTemplateURLFromTemplateURLAndSyncData(profile_, local_turl,
            iter->second, &new_changes));
    if (!sync_turl.get())
      continue;

    if (local_turl) {
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
      } else if (sync_turl->last_modified() < local_turl->last_modified()) {
        // Otherwise, we know we have newer data, so update Sync with our
        // data fields.
        new_changes.push_back(SyncChange(SyncChange::ACTION_UPDATE,
            local_data_map[local_turl->sync_guid()]));
      }
      local_data_map.erase(iter->first);
    } else {
      // The search engine from the cloud has not been synced locally, but there
      // might be a local search engine that is a duplicate that needs to be
      // merged.
      TemplateURL* dupe_turl = FindDuplicateOfSyncTemplateURL(*sync_turl);
      if (dupe_turl) {
        // Merge duplicates and remove the processed local TURL from the map.
        std::string old_guid = dupe_turl->sync_guid();
        MergeSyncAndLocalURLDuplicates(sync_turl.release(), dupe_turl,
                                       &new_changes);
        local_data_map.erase(old_guid);
      } else {
        std::string guid = sync_turl->sync_guid();
        // Keyword conflict is possible in this case. Resolve it first before
        // adding the new TemplateURL. Note that we don't remove the local TURL
        // from local_data_map in this case as it may still need to be pushed to
        // the cloud.  We also explicitly don't resolve conflicts against
        // extension keywords; see comments in ProcessSyncChanges().
        TemplateURL* existing_keyword_turl =
            FindNonExtensionTemplateURLForKeyword(sync_turl->keyword());
        if (existing_keyword_turl) {
          ResolveSyncKeywordConflict(sync_turl.get(), existing_keyword_turl,
                                     &new_changes);
        }
        // Force the local ID to kInvalidTemplateURLID so we can add it.
        TemplateURLData data(sync_turl->data());
        data.id = kInvalidTemplateURLID;
        Add(new TemplateURL(profile_, data));

        // Possibly set the newly added |turl| as the default search provider.
        SetDefaultSearchProviderIfNewlySynced(guid);
      }
    }
  }

  // The remaining SyncData in local_data_map should be everything that needs to
  // be pushed as ADDs to sync.
  for (SyncDataMap::const_iterator iter = local_data_map.begin();
      iter != local_data_map.end(); ++iter) {
    new_changes.push_back(SyncChange(SyncChange::ACTION_ADD, iter->second));
  }

  PreventDuplicateGUIDUpdates(&new_changes);

  SyncError error = sync_processor_->ProcessSyncChanges(FROM_HERE, new_changes);
  if (error.IsSet())
    return error;

  models_associated_ = true;
  return SyncError();
}

void TemplateURLService::StopSyncing(syncable::ModelType type) {
  DCHECK_EQ(type, syncable::SEARCH_ENGINES);
  models_associated_ = false;
  sync_processor_.reset();
  sync_error_factory_.reset();
}

void TemplateURLService::ProcessTemplateURLChange(
    const TemplateURL* turl,
    SyncChange::SyncChangeType type) {
  DCHECK_NE(type, SyncChange::ACTION_INVALID);
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

  SyncChangeList changes;

  SyncData sync_data = CreateSyncDataFromTemplateURL(*turl);
  changes.push_back(SyncChange(type, sync_data));

  sync_processor_->ProcessSyncChanges(FROM_HERE, changes);
}

// static
SyncData TemplateURLService::CreateSyncDataFromTemplateURL(
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
  return SyncData::CreateLocalData(se_specifics->sync_guid(),
                                   se_specifics->keyword(),
                                   specifics);
}

// static
TemplateURL* TemplateURLService::CreateTemplateURLFromTemplateURLAndSyncData(
    Profile* profile,
    TemplateURL* existing_turl,
    const SyncData& sync_data,
    SyncChangeList* change_list) {
  DCHECK(change_list);

  sync_pb::SearchEngineSpecifics specifics =
      sync_data.GetSpecifics().search_engine();

  // Past bugs might have caused either of these fields to be empty.  Just
  // delete this data off the server.
  if (specifics.url().empty() || specifics.sync_guid().empty()) {
    change_list->push_back(SyncChange(SyncChange::ACTION_DELETE, sync_data));
    return NULL;
  }

  TemplateURLData data;
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
  data.date_created = base::Time::FromInternalValue(specifics.date_created());
  data.last_modified = base::Time::FromInternalValue(specifics.last_modified());
  data.prepopulate_id = specifics.prepopulate_id();
  data.sync_guid = specifics.sync_guid();
  if (existing_turl) {
    data.id = existing_turl->id();
    data.created_by_policy = existing_turl->created_by_policy();
    data.usage_count = existing_turl->usage_count();
  }

  TemplateURL* turl = new TemplateURL(profile, data);
  DCHECK(!turl->IsExtensionKeyword());
  if (reset_keyword) {
    turl->ResetKeywordIfNecessary(true);
    SyncData sync_data = CreateSyncDataFromTemplateURL(*turl);
    change_list->push_back(SyncChange(SyncChange::ACTION_UPDATE, sync_data));
  }
  return turl;
}

// static
SyncDataMap TemplateURLService::CreateGUIDToSyncDataMap(
    const SyncDataList& sync_data) {
  SyncDataMap data_map;
  for (SyncDataList::const_iterator i(sync_data.begin()); i != sync_data.end();
       ++i)
    data_map[i->GetSpecifics().search_engine().sync_guid()] = *i;
  return data_map;
}

void TemplateURLService::SetKeywordSearchTermsForURL(const TemplateURL* t_url,
                                                     const GURL& url,
                                                     const string16& term) {
  HistoryService* history = profile_  ?
      profile_->GetHistoryService(Profile::EXPLICIT_ACCESS) : NULL;
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
    pref_change_registrar_.Add(prefs::kSyncedDefaultSearchProviderGUID, this);
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
  if (loaded_)
    provider_map_->Remove(template_url);
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

  for (PendingExtensionIDs::const_iterator i(pending_extension_ids_.begin());
       i != pending_extension_ids_.end(); ++i) {
    const Extension* extension =
        profile_->GetExtensionService()->GetExtensionById(*i, true);
    if (extension)
      RegisterExtensionKeyword(extension);
  }
  pending_extension_ids_.clear();
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

  TemplateURLData data;
  data.short_name = name;
  data.SetKeyword(keyword);
  data.SetURL(search_url);
  data.suggestions_url = suggest_url;
  data.instant_url = instant_url;
  data.favicon_url = GURL(icon_url);
  data.show_in_default_list = true;
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

bool TemplateURLService::UpdateNoNotify(TemplateURL* existing_turl,
                                        const TemplateURL& new_values) {
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

  provider_map_->Remove(existing_turl);
  TemplateURLID previous_id = existing_turl->id();
  existing_turl->CopyFrom(new_values);
  existing_turl->data_.id = previous_id;
  UIThreadSearchTermsData search_terms_data(profile_);
  provider_map_->Add(existing_turl, search_terms_data);

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
  ProcessTemplateURLChange(existing_turl, SyncChange::ACTION_UPDATE);

  if (default_search_provider_ == existing_turl) {
      bool success = SetDefaultSearchProviderNoNotify(existing_turl);
      DCHECK(success);
  }
  return true;
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
      profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (!history)
    return;

  GURL url(URLFixerUpper::FixupURL(UTF16ToUTF8(t_url.keyword()),
                                   std::string()));
  if (!url.is_valid())
    return;

  // Synthesize a visit for the keyword. This ensures the url for the keyword is
  // autocompleted even if the user doesn't type the url in directly.
  history->AddPage(url, NULL, 0, GURL(),
                   content::PAGE_TRANSITION_KEYWORD_GENERATED,
                   history::RedirectList(), history::SOURCE_BROWSED, false);
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

void TemplateURLService::GoogleBaseURLChanged() {
  bool something_changed = false;
  for (TemplateURLVector::iterator i(template_urls_.begin());
       i != template_urls_.end(); ++i) {
    TemplateURL* t_url = *i;
    if (t_url->url_ref().HasGoogleBaseURLs() ||
        t_url->suggestions_url_ref().HasGoogleBaseURLs()) {
      something_changed = true;
      string16 original_keyword(t_url->keyword());
      t_url->InvalidateCachedValues();
      const string16& new_keyword(t_url->keyword());
      KeywordToTemplateMap::const_iterator i =
          keyword_to_template_map_.find(new_keyword);
      if ((i != keyword_to_template_map_.end()) && (i->second != t_url)) {
        // The new autogenerated keyword conflicts with another TemplateURL.
        // Overwrite it if it's replaceable; otherwise just reset |t_url|'s
        // keyword.  (This will not prevent |t_url| from auto-updating the
        // keyword in the future if the conflicting TemplateURL disappears.)
        if (!CanReplace(i->second)) {
          t_url->data_.SetKeyword(original_keyword);
          continue;
        }
        RemoveNoNotify(i->second);
      }
      RemoveFromKeywordMapByPointer(t_url);
      keyword_to_template_map_[new_keyword] = t_url;
    }
  }

  if (something_changed && loaded_) {
    UIThreadSearchTermsData search_terms_data(profile_);
    provider_map_->UpdateGoogleBaseURLs(search_terms_data);
    NotifyObservers();
  }
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
      TemplateURL new_values(profile_, data);
      UpdateNoNotify(default_search_provider_, new_values);
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
    if (synced_default)
      pending_synced_default_search_ = false;
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

  default_search_provider_ = url;

  if (url) {
    // Don't mark the url as edited, otherwise we won't be able to rev the
    // template urls we ship with.
    url->data_.show_in_default_list = true;
    if (service_.get())
      service_->UpdateKeyword(url->data());

    if (url->url_ref().HasGoogleBaseURLs()) {
      GoogleURLTracker::RequestServerCheck(profile_);
#if defined(ENABLE_RLZ)
      // Needs to be evaluated. See http://crbug.com/62328.
      base::ThreadRestrictions::ScopedAllowIO allow_io;
      RLZTracker::RecordProductEvent(rlz_lib::CHROME,
                                     rlz_lib::CHROME_OMNIBOX,
                                     rlz_lib::SET_TO_GOOGLE);
#endif
    }
  }

  if (!is_default_search_managed_) {
    SaveDefaultSearchProviderToPrefs(url);

    // If we are syncing, we want to set the synced pref that will notify other
    // instances to change their default to this new search provider.
    if (sync_processor_.get() && url && !url->sync_guid().empty() &&
        GetPrefs()) {
      GetPrefs()->SetString(prefs::kSyncedDefaultSearchProviderGUID,
                            url->sync_guid());
    }
  }

  if (service_.get())
    service_->SetDefaultSearchProvider(url);

  // Inform sync the change to the show_in_default_list flag.
  if (url)
    ProcessTemplateURLChange(url, SyncChange::ACTION_UPDATE);
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
        string16 new_keyword = UniquifyKeyword(*existing_keyword_turl);
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
    ProcessTemplateURLChange(template_url, SyncChange::ACTION_ADD);
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
  ProcessTemplateURLChange(template_url, SyncChange::ACTION_DELETE);

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
  UpdateNoNotify(url, new_url);
}

string16 TemplateURLService::UniquifyKeyword(const TemplateURL& turl) {
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

  // We try to uniquify the keyword by appending a special character to the end.
  // This is a best-effort approach where we try to preserve the original
  // keyword and let the user do what they will after our attempt.
  string16 keyword_candidate(turl.keyword());
  do {
    keyword_candidate.append(ASCIIToUTF16("_"));
  } while (GetTemplateURLForKeyword(keyword_candidate));

  return keyword_candidate;
}

void TemplateURLService::ResolveSyncKeywordConflict(
    TemplateURL* sync_turl,
    TemplateURL* local_turl,
    SyncChangeList* change_list) {
  DCHECK(loaded_);
  DCHECK(sync_turl);
  DCHECK(local_turl);
  DCHECK(sync_turl->sync_guid() != local_turl->sync_guid());
  DCHECK(!local_turl->IsExtensionKeyword());
  DCHECK(change_list);

  if (local_turl->last_modified() > sync_turl->last_modified() ||
      local_turl->created_by_policy()) {
    string16 new_keyword = UniquifyKeyword(*sync_turl);
    DCHECK(!GetTemplateURLForKeyword(new_keyword));
    sync_turl->data_.SetKeyword(new_keyword);
    // If we update the cloud TURL, we need to push an update back to sync
    // informing it that something has changed.
    SyncData sync_data = CreateSyncDataFromTemplateURL(*sync_turl);
    change_list->push_back(SyncChange(SyncChange::ACTION_UPDATE, sync_data));
  } else {
    string16 new_keyword = UniquifyKeyword(*local_turl);
    TemplateURLData data(local_turl->data());
    data.SetKeyword(new_keyword);
    TemplateURL new_turl(local_turl->profile(), data);
    if (UpdateNoNotify(local_turl, new_turl))
      NotifyObservers();
    if (!models_associated_) {
      // We're doing our initial sync, so UpdateNoNotify() won't have generated
      // an ACTION_UPDATE.  If this local URL is one that was just newly brought
      // down from the sync server, we need to go ahead and generate an update
      // for it.  If it was pre-existing, then this is unnecessary (and in fact
      // wrong) because MergeDataAndStartSyncing() will later add an ACTION_ADD
      // for this URL; but in this case, PreventDuplicateGUIDUpdates() will
      // prune out the ACTION_UPDATE we create here.
      SyncData sync_data = CreateSyncDataFromTemplateURL(*local_turl);
      change_list->push_back(SyncChange(SyncChange::ACTION_UPDATE, sync_data));
    }
  }
}

TemplateURL* TemplateURLService::FindDuplicateOfSyncTemplateURL(
    const TemplateURL& sync_turl) {
  TemplateURL* existing_turl = GetTemplateURLForKeyword(sync_turl.keyword());
  return existing_turl && (existing_turl->url() == sync_turl.url()) ?
      existing_turl : NULL;
}

void TemplateURLService::MergeSyncAndLocalURLDuplicates(
    TemplateURL* sync_turl,
    TemplateURL* local_turl,
    SyncChangeList* change_list) {
  DCHECK(loaded_);
  DCHECK(sync_turl);
  DCHECK(local_turl);
  DCHECK(change_list);
  scoped_ptr<TemplateURL> scoped_sync_turl(sync_turl);
  if (sync_turl->last_modified() > local_turl->last_modified()) {
    // Fully replace local_url with Sync's copy. Note that because use Add
    // rather than ResetTemplateURL, |sync_url| is added with a fresh
    // TemplateURLID. We don't need to sync the new ID back to the server since
    // it's only relevant locally.
    bool delete_default = (local_turl == GetDefaultSearchProvider());
    DCHECK(!delete_default || !is_default_search_managed_);
    if (delete_default)
      default_search_provider_ = NULL;

    Remove(local_turl);

    // Force the local ID to kInvalidTemplateURLID so we can add it.
    sync_turl->data_.id = kInvalidTemplateURLID;
    Add(scoped_sync_turl.release());
    if (delete_default)
      SetDefaultSearchProvider(sync_turl);
  } else {
    // Change the local TURL's GUID to the server's GUID and push an update to
    // Sync. This ensures that the rest of local_url's fields are sync'd up to
    // the server, and the next time local_url is synced, it is recognized by
    // having the same GUID.
    ResetTemplateURLGUID(local_turl, sync_turl->sync_guid());
    SyncData sync_data = CreateSyncDataFromTemplateURL(*local_turl);
    change_list->push_back(SyncChange(SyncChange::ACTION_UPDATE, sync_data));
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
    if (turl_from_sync && turl_from_sync->SupportsReplacement())
      SetDefaultSearchProvider(turl_from_sync);
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
      template_url->data_.sync_guid = guid::GenerateGUID();
      if (service_.get())
        service_->UpdateKeyword(template_url->data());
    }
  }
}
