// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/template_url_service.h"

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/i18n/case_conversion.h"
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
#include "chrome/browser/prefs/pref_set_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/protector/base_setting_change.h"
#include "chrome/browser/protector/protector.h"
#include "chrome/browser/rlz/rlz.h"
#include "chrome/browser/search_engines/search_host_to_urls_map.h"
#include "chrome/browser/search_engines/search_terms_data.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service_observer.h"
#include "chrome/browser/search_engines/template_url_prepopulate_data.h"
#include "chrome/browser/search_engines/util.h"
#include "chrome/browser/sync/api/sync_change.h"
#include "chrome/browser/sync/protocol/search_engine_specifics.pb.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/env_vars.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/guid.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/notification_service.h"
#include "net/base/net_util.h"
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
  return NULL != url1 &&
      NULL != url2 &&
      url1->short_name() == url2->short_name() &&
      url1->keyword() == url2->keyword() &&
      TemplateURLRef::SameUrlRefs(url1->url(), url2->url()) &&
      TemplateURLRef::SameUrlRefs(url1->suggestions_url(),
                                  url2->suggestions_url()) &&
      url1->GetFaviconURL() == url2->GetFaviconURL() &&
      url1->safe_for_autoreplace() == url2->safe_for_autoreplace() &&
      url1->show_in_default_list() == url2->show_in_default_list() &&
      url1->input_encodings() == url2->input_encodings();
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
    : profile_(profile),
      loaded_(false),
      load_failed_(false),
      load_handle_(0),
      default_search_provider_(NULL),
      is_default_search_managed_(false),
      next_id_(1),
      time_provider_(&base::Time::Now),
      models_associated_(false),
      processing_syncer_changes_(false),
      sync_processor_(NULL),
      pending_synced_default_search_(false) {
  DCHECK(profile_);
  Init(NULL, 0);
}

TemplateURLService::TemplateURLService(const Initializer* initializers,
                                       const int count)
    : profile_(NULL),
      loaded_(false),
      load_failed_(false),
      load_handle_(0),
      service_(NULL),
      default_search_provider_(NULL),
      is_default_search_managed_(false),
      next_id_(1),
      time_provider_(&base::Time::Now),
      models_associated_(false),
      processing_syncer_changes_(false),
      sync_processor_(NULL),
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
string16 TemplateURLService::GenerateKeyword(const GURL& url,
                                             bool autodetected) {
  // Don't autogenerate keywords for referrers that are the result of a form
  // submission (TODO: right now we approximate this by checking for the URL
  // having a query, but we should replace this with a call to WebCore to see if
  // the originating page was actually a form submission), anything other than
  // http, or referrers with a path.
  //
  // If we relax the path constraint, we need to be sure to sanitize the path
  // elements and update AutocompletePopup to look for keywords using the path.
  // See http://b/issue?id=863583.
  if (!url.is_valid() ||
      (autodetected && (url.has_query() || !url.SchemeIs(chrome::kHttpScheme) ||
                        ((url.path() != "") && (url.path() != "/")))))
    return string16();

  // Strip "www." off the front of the keyword; otherwise the keyword won't work
  // properly.  See http://code.google.com/p/chromium/issues/detail?id=6984 .
  return net::StripWWW(UTF8ToUTF16(url.host()));
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
GURL TemplateURLService::GenerateSearchURL(const TemplateURL* t_url) {
  DCHECK(t_url);
  UIThreadSearchTermsData search_terms_data;
  return GenerateSearchURLUsingTermsData(t_url, search_terms_data);
}

// static
GURL TemplateURLService::GenerateSearchURLUsingTermsData(
    const TemplateURL* t_url,
    const SearchTermsData& search_terms_data) {
  DCHECK(t_url);
  const TemplateURLRef* search_ref = t_url->url();
  // Extension keywords don't have host-based search URLs.
  if (!search_ref || !search_ref->IsValidUsingTermsData(search_terms_data) ||
      t_url->IsExtensionKeyword())
    return GURL();

  if (!search_ref->SupportsReplacementUsingTermsData(search_terms_data))
    return GURL(search_ref->url());

  return GURL(search_ref->ReplaceSearchTermsUsingTermsData(
      *t_url, ASCIIToUTF16(kReplacementTerm),
      TemplateURLRef::NO_SUGGESTIONS_AVAILABLE,
      string16(), search_terms_data));
}

bool TemplateURLService::CanReplaceKeyword(
    const string16& keyword,
    const GURL& url,
    const TemplateURL** template_url_to_replace) {
  DCHECK(!keyword.empty());  // This should only be called for non-empty
                             // keywords. If we need to support empty kewords
                             // the code needs to change slightly.
  const TemplateURL* existing_url = GetTemplateURLForKeyword(keyword);
  if (existing_url) {
    // We already have a TemplateURL for this keyword. Only allow it to be
    // replaced if the TemplateURL can be replaced.
    if (template_url_to_replace)
      *template_url_to_replace = existing_url;
    return CanReplace(existing_url);
  }

  // We don't have a TemplateURL with keyword. Only allow a new one if there
  // isn't a TemplateURL for the specified host, or there is one but it can
  // be replaced. We do this to ensure that if the user assigns a different
  // keyword to a generated TemplateURL, we won't regenerate another keyword for
  // the same host.
  if (url.is_valid() && !url.host().empty())
    return CanReplaceKeywordForHost(url.host(), template_url_to_replace);
  return true;
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

  // Visual Studio 2010 has problems converting NULL to the null pointer for
  // std::pair.  See http://connect.microsoft.com/VisualStudio/feedback/details/520043/error-converting-from-null-to-a-pointer-type-in-std-pair
  // It will work if we pass nullptr.
#if defined(_MSC_VER) && _MSC_VER >= 1600
  const TemplateURL* null_url = nullptr;
#else
  const TemplateURL* null_url = NULL;
#endif

  // Find matching keyword range.  Searches the element map for keywords
  // beginning with |prefix| and stores the endpoints of the resulting set in
  // |match_range|.
  const std::pair<KeywordToTemplateMap::const_iterator,
                  KeywordToTemplateMap::const_iterator> match_range(
      std::equal_range(
          keyword_to_template_map_.begin(), keyword_to_template_map_.end(),
          KeywordToTemplateMap::value_type(prefix, null_url),
          LessWithPrefix()));

  // Return vector of matching keywords.
  for (KeywordToTemplateMap::const_iterator i(match_range.first);
       i != match_range.second; ++i) {
    DCHECK(i->second->url());
    if (!support_replacement_only || i->second->url()->SupportsReplacement())
      matches->push_back(i->first);
  }
}

const TemplateURL* TemplateURLService::GetTemplateURLForKeyword(
    const string16& keyword) const {
  KeywordToTemplateMap::const_iterator elem(
      keyword_to_template_map_.find(keyword));
  return (elem == keyword_to_template_map_.end()) ? NULL : elem->second;
}

const TemplateURL* TemplateURLService::GetTemplateURLForGUID(
    const std::string& sync_guid) const {
  GUIDToTemplateMap::const_iterator elem(
      guid_to_template_map_.find(sync_guid));
  return (elem == guid_to_template_map_.end()) ? NULL : elem->second;
}

const TemplateURL* TemplateURLService::GetTemplateURLForHost(
    const std::string& host) const {
  return provider_map_.GetTemplateURLForHost(host);
}

void TemplateURLService::Add(TemplateURL* template_url) {
  AddNoNotify(template_url);
  NotifyObservers();
}

void TemplateURLService::Remove(const TemplateURL* template_url) {
  RemoveNoNotify(template_url);
  NotifyObservers();
}

void TemplateURLService::RemoveAutoGeneratedBetween(base::Time created_after,
                                                    base::Time created_before) {
  bool should_notify = false;
  for (size_t i = 0; i < template_urls_.size();) {
    if (template_urls_[i]->date_created() >= created_after &&
        (created_before.is_null() ||
         template_urls_[i]->date_created() < created_before) &&
        CanReplace(template_urls_[i])) {
      RemoveNoNotify(template_urls_[i]);
      should_notify = true;
    } else {
      ++i;
    }
  }
  if (should_notify)
    NotifyObservers();
}

void TemplateURLService::RemoveAutoGeneratedSince(base::Time created_after) {
  RemoveAutoGeneratedBetween(created_after, base::Time());
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

  const TemplateURL* existing_url = GetTemplateURLForExtension(extension);
  string16 keyword = UTF8ToUTF16(extension->omnibox_keyword());

  scoped_ptr<TemplateURL> template_url(new TemplateURL);
  template_url->set_short_name(UTF8ToUTF16(extension->name()));
  template_url->set_keyword(keyword);
  // This URL is not actually used for navigation. It holds the extension's
  // ID, as well as forcing the TemplateURL to be treated as a search keyword.
  template_url->SetURL(
      std::string(chrome::kExtensionScheme) + "://" +
      extension->id() + "/?q={searchTerms}", 0, 0);
  template_url->set_safe_for_autoreplace(false);

  if (existing_url) {
    // TODO(mpcomplete): only replace if the user hasn't changed the keyword.
    // (We don't have UI for that yet).
    UpdateNoNotify(existing_url, *template_url);
  } else {
    AddNoNotify(template_url.release());
  }
  NotifyObservers();
}

void TemplateURLService::UnregisterExtensionKeyword(
    const Extension* extension) {
  const TemplateURL* url = GetTemplateURLForExtension(extension);
  if (url)
    Remove(url);
}

const TemplateURL* TemplateURLService::GetTemplateURLForExtension(
    const Extension* extension) const {
  for (TemplateURLVector::const_iterator i = template_urls_.begin();
       i != template_urls_.end(); ++i) {
    if ((*i)->IsExtensionKeyword() && (*i)->url()->GetHost() == extension->id())
      return *i;
  }

  return NULL;
}

std::vector<const TemplateURL*> TemplateURLService::GetTemplateURLs() const {
  return template_urls_;
}

void TemplateURLService::IncrementUsageCount(const TemplateURL* url) {
  DCHECK(url && find(template_urls_.begin(), template_urls_.end(), url) !=
         template_urls_.end());
  const_cast<TemplateURL*>(url)->set_usage_count(url->usage_count() + 1);
  if (service_.get())
    service_.get()->UpdateKeyword(*url);
}

void TemplateURLService::ResetTemplateURL(const TemplateURL* url,
                                          const string16& title,
                                          const string16& keyword,
                                          const std::string& search_url) {
  TemplateURL new_url(*url);
  new_url.set_short_name(title);
  new_url.set_keyword(keyword);
  if ((new_url.url() && search_url.empty()) ||
      (!new_url.url() && !search_url.empty()) ||
      (new_url.url() && new_url.url()->url() != search_url)) {
    // The urls have changed, reset the favicon url.
    new_url.SetFaviconURL(GURL());
    new_url.SetURL(search_url, 0, 0);
  }
  new_url.set_safe_for_autoreplace(false);
  new_url.set_last_modified(time_provider_());
  UpdateNoNotify(url, new_url);
  NotifyObservers();
}

bool TemplateURLService::CanMakeDefault(const TemplateURL* url) {
  return url != GetDefaultSearchProvider() &&
      url->url() &&
      url->url()->SupportsReplacement() &&
      !is_default_search_managed();
}

void TemplateURLService::SetDefaultSearchProvider(const TemplateURL* url) {
  if (is_default_search_managed_) {
    NOTREACHED();
    return;
  }
  // Always persist the setting in the database, that way if the backup
  // signature has changed out from under us it gets reset correctly.
  SetDefaultSearchProviderNoNotify(url);
  NotifyObservers();
}

const TemplateURL* TemplateURLService::GetDefaultSearchProvider() {
  if (loaded_ && !load_failed_)
    return default_search_provider_;

  // We're not loaded, rely on the default search provider stored in prefs.
  return initial_default_search_provider_.get();
}

const TemplateURL* TemplateURLService::FindNewDefaultSearchProvider() {
  // See if the prepoluated default still exists.
  scoped_ptr<TemplateURL> prepopulated_default(
      TemplateURLPrepopulateData::GetPrepopulatedDefaultSearch(GetPrefs()));
  for (TemplateURLVector::iterator i = template_urls_.begin();
       i != template_urls_.end(); ++i) {
    if ((*i)->prepopulate_id() == prepopulated_default->prepopulate_id())
      return *i;
  }
  // If not, use the first of the templates.
  return template_urls_.empty() ? NULL : template_urls_[0];
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

  std::vector<TemplateURL*> template_urls;
  const TemplateURL* default_search_provider = NULL;
  int new_resource_keyword_version = 0;
  GetSearchProvidersUsingKeywordResult(*result,
                                       service_.get(),
                                       GetPrefs(),
                                       &template_urls,
                                       &default_search_provider,
                                       &new_resource_keyword_version);

  bool database_specified_a_default = NULL != default_search_provider;

  // Check if default search provider is now managed.
  scoped_ptr<TemplateURL> default_from_prefs;
  LoadDefaultSearchProviderFromPrefs(&default_from_prefs,
                                     &is_default_search_managed_);

  // Check if the default search provider has been changed and notify
  // Protector instance about it. Don't check if the default search is
  // managed.
  const TemplateURL* backup_default_search_provider = NULL;
  if (!is_default_search_managed_ &&
      DidDefaultSearchProviderChange(
          *result,
          template_urls,
          &backup_default_search_provider) &&
      !CommandLine::ForCurrentProcess()->HasSwitch(switches::kNoProtector)) {
    // TODO: need to handle no default_search_provider better. Likely need to
    // make sure the default search engine is there, and if not assume it was
    // deleted and add it back.

    // Protector will delete itself when it's needed no longer.
    protector::Protector* protector = new protector::Protector(profile());
    protector->ShowChange(protector::CreateDefaultSearchProviderChange(
        default_search_provider,
        backup_default_search_provider));
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
      //
      // AddNoNotify will take ownership of default_from_prefs so it is safe to
      // release. If it's null, there's no ownership to worry about :-)
      TemplateURL* managed_default = default_from_prefs.release();
      if (managed_default) {
        managed_default->set_created_by_policy(true);
        managed_default->set_id(0);
        AddNoNotify(managed_default);
      }
      default_search_provider = managed_default;
    }
    // Note that this saves the default search provider to prefs.
    SetDefaultSearchProviderNoNotify(default_search_provider);
  } else {
    // If we had a managed default, replace it with the synced default if
    // applicable, or the first provider of the list.
    const TemplateURL* synced_default = GetPendingSyncedDefaultSearchProvider();
    if (synced_default) {
      default_search_provider = synced_default;
      pending_synced_default_search_ = false;
    } else if (database_specified_a_default &&
               NULL == default_search_provider &&
               !template_urls.empty()) {
      default_search_provider = template_urls[0];
    }

    // If the default search provider existed previously, then just
    // set the member variable. Otherwise, we'll set it using the method
    // to ensure that it is saved properly after its id is set.
    if (default_search_provider && default_search_provider->id() != 0) {
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

  if (new_resource_keyword_version && service_.get())
    service_->SetBuiltinKeywordVersion(new_resource_keyword_version);

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
    if (!loaded())
      visits_to_add_.push_back(*visit_details.ptr());
    else
      UpdateKeywordSearchTermsForURL(*visit_details.ptr());
  } else if (type == chrome::NOTIFICATION_GOOGLE_URL_UPDATED) {
    if (loaded_)
      GoogleBaseURLChanged();
  } else if (type == chrome::NOTIFICATION_PREF_CHANGED) {
    const std::string* pref_name = content::Details<std::string>(details).ptr();
    if (!pref_name || default_search_prefs_->IsObserved(*pref_name)) {
      // Listen for changes to the default search from Sync. If it is
      // specifically the synced default search provider GUID that changed, we
      // have to set it (or wait for it).
      PrefService* prefs = GetPrefs();
      if (pref_name && *pref_name == prefs::kSyncedDefaultSearchProviderGUID &&
          prefs) {
        const TemplateURL* new_default_search = GetTemplateURLForGUID(
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
      }

      // A preference related to default search engine has changed.
      // Update the model if needed.
      UpdateDefaultSearch();
    }
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

  AutoReset<bool> processing_changes(&processing_syncer_changes_, true);

  SyncChangeList new_changes;
  SyncError error;
  for (SyncChangeList::const_iterator iter = change_list.begin();
      iter != change_list.end(); ++iter) {
    DCHECK_EQ(syncable::SEARCH_ENGINES, iter->sync_data().GetDataType());

    scoped_ptr<TemplateURL> turl(
        CreateTemplateURLFromSyncData(iter->sync_data()));
    if (!turl.get()) {
      NOTREACHED() << "Failed to read search engine.";
      continue;
    }

    const TemplateURL* existing_turl = GetTemplateURLForGUID(turl->sync_guid());
    const TemplateURL* existing_keyword_turl =
        GetTemplateURLForKeyword(turl->keyword());

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
      if (existing_keyword_turl)
        ResolveSyncKeywordConflict(turl.get(), &new_changes);
      // Force the local ID to 0 so we can add it.
      turl->set_id(0);
      Add(turl.release());

      // Possibly set the newly added |turl| as the default search provider.
      SetDefaultSearchProviderIfNewlySynced(guid);
    } else if (iter->change_type() == SyncChange::ACTION_UPDATE &&
               existing_turl) {
      // Possibly resolve a keyword conflict if they have the same keywords but
      // are not the same entry.
      if (existing_keyword_turl && existing_keyword_turl != existing_turl)
        ResolveSyncKeywordConflict(turl.get(), &new_changes);
      ResetTemplateURL(existing_turl, turl->short_name(), turl->keyword(),
          turl->url() ? turl->url()->url() : std::string());
    } else {
      // Something really unexpected happened. Either we received an
      // ACTION_INVALID, or Sync is in a crazy state:
      //  . Trying to DELETE or UPDATE a non-existent search engine.
      //  . Trying to ADD a search engine that already exists.
      NOTREACHED() << "Unexpected sync change state.";
      error = SyncError(FROM_HERE, "ProcessSyncChanges failed on ChangeType " +
          SyncChange::ChangeTypeToString(iter->change_type()),
          syncable::SEARCH_ENGINES);
    }
  }

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
    SyncChangeProcessor* sync_processor) {
  DCHECK(loaded());
  DCHECK_EQ(type, syncable::SEARCH_ENGINES);
  DCHECK(!sync_processor_);
  sync_processor_ = sync_processor;

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
    scoped_ptr<TemplateURL> sync_turl(
        CreateTemplateURLFromSyncData(iter->second));
    DCHECK(sync_turl.get());
    const TemplateURL* local_turl = GetTemplateURLForGUID(iter->first);

    if (sync_turl->sync_guid().empty()) {
      // Due to a bug, older search engine entries with no sync GUID
      // may have been uploaded to the server.  This is bad data, so
      // just delete it.
      new_changes.push_back(
          SyncChange(SyncChange::ACTION_DELETE, iter->second));
    } else if (local_turl) {
      // This local search engine is already synced. If the timestamp differs
      // from Sync, we need to update locally or to the cloud. Note that if the
      // timestamps are equal, we touch neither.
      if (sync_turl->last_modified() > local_turl->last_modified()) {
        // We've received an update from Sync. We should replace all existing
        // values with the ones from sync except the local TemplateURLID. Note
        // that this means that the TemplateURL may have to be reparsed. This
        // also makes the local data's last_modified timestamp equal to Sync's,
        // avoiding an Update on the next MergeData call.
        sync_turl->set_id(local_turl->id());
        UpdateNoNotify(local_turl, *sync_turl);
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
      const TemplateURL* dupe_turl = FindDuplicateOfSyncTemplateURL(*sync_turl);
      if (dupe_turl) {
        // Merge duplicates and remove the processed local TURL from the map.
        TemplateURL* modifiable_dupe_turl =
            const_cast<TemplateURL*>(dupe_turl);
        std::string old_guid = dupe_turl->sync_guid();
        MergeSyncAndLocalURLDuplicates(sync_turl.release(),
                                       modifiable_dupe_turl,
                                       &new_changes);
        local_data_map.erase(old_guid);
      } else {
        std::string guid = sync_turl->sync_guid();
        // Keyword conflict is possible in this case. Resolve it first before
        // adding the new TemplateURL. Note that we don't remove the local TURL
        // from local_data_map in this case as it may still need to be pushed to
        // the cloud.
        ResolveSyncKeywordConflict(sync_turl.get(), &new_changes);
        // Force the local ID to 0 so we can add it.
        sync_turl->set_id(0);
        Add(sync_turl.release());

        // Possibly set the newly added |turl| as the default search provider.
        SetDefaultSearchProviderIfNewlySynced(guid);
      }
    }
  }  // for

  // The remaining SyncData in local_data_map should be everything that needs to
  // be pushed as ADDs to sync.
  for (SyncDataMap::const_iterator iter = local_data_map.begin();
      iter != local_data_map.end(); ++iter) {
    new_changes.push_back(SyncChange(SyncChange::ACTION_ADD, iter->second));
  }

  SyncError error = sync_processor_->ProcessSyncChanges(FROM_HERE, new_changes);
  if (error.IsSet())
    return error;

  models_associated_ = true;
  return SyncError();
}

void TemplateURLService::StopSyncing(syncable::ModelType type) {
  DCHECK_EQ(type, syncable::SEARCH_ENGINES);
  models_associated_ = false;
  sync_processor_ = NULL;
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
  sync_pb::SearchEngineSpecifics* se_specifics = specifics.MutableExtension(
      sync_pb::search_engine);
  se_specifics->set_short_name(UTF16ToUTF8(turl.short_name()));
  se_specifics->set_keyword(UTF16ToUTF8(turl.keyword()));
  se_specifics->set_favicon_url(turl.GetFaviconURL().spec());
  se_specifics->set_url(turl.url() ? turl.url()->url() : std::string());
  se_specifics->set_safe_for_autoreplace(turl.safe_for_autoreplace());
  se_specifics->set_originating_url(turl.originating_url().spec());
  se_specifics->set_date_created(turl.date_created().ToInternalValue());
  se_specifics->set_input_encodings(JoinString(turl.input_encodings(), ';'));
  se_specifics->set_show_in_default_list(turl.show_in_default_list());
  se_specifics->set_suggestions_url(turl.suggestions_url() ?
      turl.suggestions_url()->url() : std::string());
  se_specifics->set_prepopulate_id(turl.prepopulate_id());
  se_specifics->set_autogenerate_keyword(turl.autogenerate_keyword());
  se_specifics->set_instant_url(turl.instant_url() ?
      turl.instant_url()->url() : std::string());
  se_specifics->set_last_modified(turl.last_modified().ToInternalValue());
  se_specifics->set_sync_guid(turl.sync_guid());
  return SyncData::CreateLocalData(se_specifics->sync_guid(),
                                   se_specifics->keyword(),
                                   specifics);
}

// static
TemplateURL* TemplateURLService::CreateTemplateURLFromSyncData(
    const SyncData& sync_data) {
  sync_pb::SearchEngineSpecifics specifics =
      sync_data.GetSpecifics().GetExtension(sync_pb::search_engine);
  TemplateURL* turl = new TemplateURL();
  turl->set_short_name(UTF8ToUTF16(specifics.short_name()));
  turl->set_keyword(UTF8ToUTF16(specifics.keyword()));
  turl->SetFaviconURL(GURL(specifics.favicon_url()));
  turl->SetURL(specifics.url(), 0, 0);
  turl->set_safe_for_autoreplace(specifics.safe_for_autoreplace());
  turl->set_originating_url(GURL(specifics.originating_url()));
  turl->set_date_created(
      base::Time::FromInternalValue(specifics.date_created()));
  std::vector<std::string> input_encodings;
  base::SplitString(specifics.input_encodings(), ';', &input_encodings);
  turl->set_input_encodings(input_encodings);
  turl->set_show_in_default_list(specifics.show_in_default_list());
  turl->SetSuggestionsURL(specifics.suggestions_url(), 0, 0);
  turl->SetPrepopulateId(specifics.prepopulate_id());
  turl->set_autogenerate_keyword(specifics.autogenerate_keyword());
  turl->SetInstantURL(specifics.instant_url(), 0, 0);
  turl->set_last_modified(
      base::Time::FromInternalValue(specifics.last_modified()));
  turl->set_sync_guid(specifics.sync_guid());
  return turl;
}

// static
SyncDataMap TemplateURLService::CreateGUIDToSyncDataMap(
    const SyncDataList& sync_data) {
  SyncDataMap data_map;
  SyncDataList::const_iterator iter;
  for (iter = sync_data.begin(); iter != sync_data.end(); ++iter) {
    data_map[iter->GetSpecifics().GetExtension(
        sync_pb::search_engine).sync_guid()] = *iter;
  }
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
    registrar_.Add(this, chrome::NOTIFICATION_HISTORY_URL_VISITED,
                   content::Source<Profile>(profile_->GetOriginalProfile()));
    PrefService* prefs = GetPrefs();
    default_search_prefs_.reset(
        PrefSetObserver::CreateDefaultSearchPrefSetObserver(prefs, this));
  }
  registrar_.Add(this, chrome::NOTIFICATION_GOOGLE_URL_UPDATED,
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
      TemplateURL* template_url = new TemplateURL();
      template_url->set_keyword(UTF8ToUTF16(initializers[i].keyword));
      template_url->set_short_name(UTF8ToUTF16(initializers[i].content));
      template_url->SetURL(osd_url, 0, 0);
      AddNoNotify(template_url);
    }
  }

  // Initialize default search.
  UpdateDefaultSearch();

  // Request a server check for the correct Google URL if Google is the
  // default search engine, not in headless mode and not in Chrome Frame.
  if (initial_default_search_provider_.get()) {
    const TemplateURLRef* default_provider_ref =
        initial_default_search_provider_->url();
    if (default_provider_ref && default_provider_ref->HasGoogleBaseURLs()) {
      scoped_ptr<base::Environment> env(base::Environment::Create());
      if (!env->HasVar(env_vars::kHeadless) &&
          !CommandLine::ForCurrentProcess()->HasSwitch(switches::kChromeFrame))
        GoogleURLTracker::RequestServerCheck();
    }
  }
}

void TemplateURLService::RemoveFromMaps(const TemplateURL* template_url) {
  if (!template_url->keyword().empty())
    keyword_to_template_map_.erase(template_url->keyword());
  if (!template_url->sync_guid().empty())
    guid_to_template_map_.erase(template_url->sync_guid());
  if (loaded_)
    provider_map_.Remove(template_url);
}

void TemplateURLService::RemoveFromKeywordMapByPointer(
    const TemplateURL* template_url) {
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

void TemplateURLService::AddToMaps(const TemplateURL* template_url) {
  if (!template_url->keyword().empty())
    keyword_to_template_map_[template_url->keyword()] = template_url;
  if (!template_url->sync_guid().empty())
    guid_to_template_map_[template_url->sync_guid()] = template_url;
  if (loaded_) {
    UIThreadSearchTermsData search_terms_data;
    provider_map_.Add(template_url, search_terms_data);
  }
}

void TemplateURLService::SetTemplateURLs(
    const std::vector<TemplateURL*>& urls) {
  // Add mappings for the new items.

  // First, add the items that already have id's, so that the next_id_
  // gets properly set.
  for (std::vector<TemplateURL*>::const_iterator i = urls.begin();
       i != urls.end();
       ++i) {
    if ((*i)->id() == 0)
      continue;
    next_id_ = std::max(next_id_, (*i)->id());
    AddToMaps(*i);
    template_urls_.push_back(*i);
  }

  // Next add the new items that don't have id's.
  for (std::vector<TemplateURL*>::const_iterator i = urls.begin();
       i != urls.end();
       ++i) {
    if ((*i)->id() != 0)
      continue;
    AddNoNotify(*i);
  }
}

void TemplateURLService::ChangeToLoadedState() {
  DCHECK(!loaded_);

  UIThreadSearchTermsData search_terms_data;
  provider_map_.Init(template_urls_, search_terms_data);
  loaded_ = true;
}

void TemplateURLService::NotifyLoaded() {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_TEMPLATE_URL_SERVICE_LOADED,
      content::Source<TemplateURLService>(this),
      content::NotificationService::NoDetails());

  for (size_t i = 0; i < pending_extension_ids_.size(); ++i) {
    const Extension* extension = profile_->GetExtensionService()->
        GetExtensionById(pending_extension_ids_[i], true);
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
    enabled = true;
    if (t_url->url())
      search_url = t_url->url()->url();
    if (t_url->suggestions_url())
      suggest_url = t_url->suggestions_url()->url();
    if (t_url->instant_url())
      instant_url = t_url->instant_url()->url();
    GURL icon_gurl = t_url->GetFaviconURL();
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

  prefs->ScheduleSavePersistentPrefs();
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

  bool enabled =
      prefs->GetBoolean(prefs::kDefaultSearchProviderEnabled);
  std::string suggest_url =
      prefs->GetString(prefs::kDefaultSearchProviderSuggestURL);
  std::string search_url =
      prefs->GetString(prefs::kDefaultSearchProviderSearchURL);
  std::string instant_url =
      prefs->GetString(prefs::kDefaultSearchProviderInstantURL);

  if (!enabled || (suggest_url.empty() && search_url.empty())) {
    // The user doesn't want a default search provider.
    default_provider->reset(NULL);
    return true;
  }

  string16 name =
      UTF8ToUTF16(prefs->GetString(prefs::kDefaultSearchProviderName));
  string16 keyword =
      UTF8ToUTF16(prefs->GetString(prefs::kDefaultSearchProviderKeyword));
  std::string icon_url =
      prefs->GetString(prefs::kDefaultSearchProviderIconURL);
  std::string encodings =
      prefs->GetString(prefs::kDefaultSearchProviderEncodings);
  std::string id_string = prefs->GetString(prefs::kDefaultSearchProviderID);
  std::string prepopulate_id =
      prefs->GetString(prefs::kDefaultSearchProviderPrepopulateID);

  default_provider->reset(new TemplateURL());
  (*default_provider)->set_short_name(name);
  (*default_provider)->SetURL(search_url, 0, 0);
  (*default_provider)->SetSuggestionsURL(suggest_url, 0, 0);
  (*default_provider)->SetInstantURL(instant_url, 0, 0);
  (*default_provider)->set_keyword(keyword);
  (*default_provider)->SetFaviconURL(GURL(icon_url));
  std::vector<std::string> encodings_vector;
  base::SplitString(encodings, ';', &encodings_vector);
  (*default_provider)->set_input_encodings(encodings_vector);
  if (!id_string.empty() && !*is_managed) {
    int64 value;
    base::StringToInt64(id_string, &value);
    (*default_provider)->set_id(value);
  }
  if (!prepopulate_id.empty() && !*is_managed) {
    int value;
    base::StringToInt(prepopulate_id, &value);
    (*default_provider)->SetPrepopulateId(value);
  }
  (*default_provider)->set_show_in_default_list(true);
  return true;
}

bool TemplateURLService::CanReplaceKeywordForHost(
    const std::string& host,
    const TemplateURL** to_replace) {
  const TemplateURLSet* urls = provider_map_.GetURLsForHost(host);
  if (urls) {
    for (TemplateURLSet::const_iterator i = urls->begin();
         i != urls->end(); ++i) {
      const TemplateURL* url = *i;
      if (CanReplace(url)) {
        if (to_replace)
          *to_replace = url;
        return true;
      }
    }
  }

  if (to_replace)
    *to_replace = NULL;
  return !urls;
}

bool TemplateURLService::CanReplace(const TemplateURL* t_url) {
  return (t_url != default_search_provider_ && !t_url->show_in_default_list() &&
          t_url->safe_for_autoreplace());
}

void TemplateURLService::UpdateNoNotify(const TemplateURL* existing_turl,
                                        const TemplateURL& new_values) {
  DCHECK(loaded_);
  DCHECK(existing_turl);
  DCHECK(find(template_urls_.begin(), template_urls_.end(), existing_turl) !=
         template_urls_.end());

  if (!existing_turl->keyword().empty())
    keyword_to_template_map_.erase(existing_turl->keyword());
  if (!existing_turl->sync_guid().empty())
    guid_to_template_map_.erase(existing_turl->sync_guid());

  // This call handles copying over the values (while retaining the id).
  UIThreadSearchTermsData search_terms_data;
  provider_map_.Update(existing_turl, new_values, search_terms_data);

  if (!existing_turl->keyword().empty())
    keyword_to_template_map_[existing_turl->keyword()] = existing_turl;
  if (!existing_turl->sync_guid().empty())
    guid_to_template_map_[existing_turl->sync_guid()] = existing_turl;

  if (service_.get())
    service_->UpdateKeyword(*existing_turl);

  // Inform sync of the update.
  ProcessTemplateURLChange(existing_turl, SyncChange::ACTION_UPDATE);

  if (default_search_provider_ == existing_turl)
    SetDefaultSearchProviderNoNotify(existing_turl);
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
      provider_map_.GetURLsForHost(row.url().host());
  if (!urls_for_host)
    return;

  QueryTerms query_terms;
  bool built_terms = false;  // Most URLs won't match a TemplateURLs host;
                             // so we lazily build the query_terms.
  const std::string path = row.url().path();

  for (TemplateURLSet::const_iterator i = urls_for_host->begin();
       i != urls_for_host->end(); ++i) {
    const TemplateURLRef* search_ref = (*i)->url();

    // Count the URL against a TemplateURL if the host and path of the
    // visited URL match that of the TemplateURL as well as the search term's
    // key of the TemplateURL occurring in the visited url.
    //
    // NOTE: Even though we're iterating over TemplateURLs indexed by the host
    // of the URL we still need to call GetHost on the search_ref. In
    // particular, GetHost returns an empty string if search_ref doesn't support
    // replacement or isn't valid for use in keyword search terms.

    if (search_ref && search_ref->GetHost() == row.url().host() &&
        search_ref->GetPath() == path) {
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

      QueryTerms::iterator terms_iterator =
          query_terms.find(search_ref->GetSearchTermKey());
      if (terms_iterator != query_terms.end() &&
          !terms_iterator->second.empty()) {
        SetKeywordSearchTermsForURL(
            *i, row.url(), search_ref->SearchTermToString16(*(*i),
            terms_iterator->second));
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
  for (size_t i = 0; i < template_urls_.size(); ++i) {
    const TemplateURL* t_url = template_urls_[i];
    if ((t_url->url() && t_url->url()->HasGoogleBaseURLs()) ||
        (t_url->suggestions_url() &&
         t_url->suggestions_url()->HasGoogleBaseURLs())) {
      RemoveFromKeywordMapByPointer(t_url);
      t_url->InvalidateCachedValues();
      if (!t_url->keyword().empty())
        keyword_to_template_map_[t_url->keyword()] = t_url;
      something_changed = true;
    }
  }

  if (something_changed && loaded_) {
    UIThreadSearchTermsData search_terms_data;
    provider_map_.UpdateGoogleBaseURLs(search_terms_data);
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
          TemplateURLPrepopulateData::GetPrepopulatedDefaultSearch(GetPrefs()));
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
      const TemplateURL* old_default = default_search_provider_;
      SetDefaultSearchProviderNoNotify(NULL);
      RemoveNoNotify(old_default);
    } else if (default_search_provider_) {
      new_default_from_prefs->set_created_by_policy(true);
      UpdateNoNotify(default_search_provider_, *new_default_from_prefs.get());
    } else {
      // AddNoNotify will take ownership of new_template, so it's safe to
      // release.
      TemplateURL* new_template = new_default_from_prefs.release();
      if (new_template) {
        new_template->set_created_by_policy(true);
        AddNoNotify(new_template);
      }
      SetDefaultSearchProviderNoNotify(new_template);
    }
  } else if (!is_default_search_managed_ && new_is_default_managed) {
    // The default used to be unmanaged and is now managed.  Add the new
    // managed default to the list of URLs and set it as default.
    is_default_search_managed_ = new_is_default_managed;
    // AddNoNotify will take ownership of new_template, so it's safe to
    // release.
    TemplateURL* new_template = new_default_from_prefs.release();
    if (new_template) {
      new_template->set_created_by_policy(true);
      AddNoNotify(new_template);
    }
    SetDefaultSearchProviderNoNotify(new_template);
  } else {
    // The default was managed and is no longer.
    DCHECK(is_default_search_managed_ && !new_is_default_managed);
    is_default_search_managed_ = new_is_default_managed;
    // If we had a default, delete the previous default if created by policy
    // and set a likely default.
    if (NULL != default_search_provider_ &&
        default_search_provider_->created_by_policy()) {
      const TemplateURL* old_default = default_search_provider_;
      default_search_provider_ = NULL;
      RemoveNoNotify(old_default);
    }

    // The likely default should be from Sync if we were waiting on Sync.
    // Otherwise, it should be FindNewDefaultSearchProvider.
    const TemplateURL* synced_default = GetPendingSyncedDefaultSearchProvider();
    if (synced_default)
      pending_synced_default_search_ = false;
    SetDefaultSearchProviderNoNotify(synced_default ? synced_default :
        FindNewDefaultSearchProvider());
  }
  NotifyObservers();
}

void TemplateURLService::SetDefaultSearchProviderNoNotify(
    const TemplateURL* url) {
  DCHECK(!url || find(template_urls_.begin(), template_urls_.end(), url) !=
         template_urls_.end());
  default_search_provider_ = url;

  if (url) {
    TemplateURL* modifiable_url = const_cast<TemplateURL*>(url);
    // Don't mark the url as edited, otherwise we won't be able to rev the
    // template urls we ship with.
    modifiable_url->set_show_in_default_list(true);
    if (service_.get())
      service_.get()->UpdateKeyword(*url);

    const TemplateURLRef* url_ref = url->url();
    if (url_ref && url_ref->HasGoogleBaseURLs()) {
      GoogleURLTracker::RequestServerCheck();
#if defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)
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
    if (sync_processor_ && !url->sync_guid().empty() && GetPrefs()) {
      GetPrefs()->SetString(prefs::kSyncedDefaultSearchProviderGUID,
                            url->sync_guid());
    }
  }

  if (service_.get())
    service_->SetDefaultSearchProvider(url);

  // Inform sync the change to the show_in_default_list flag.
  if (url)
    ProcessTemplateURLChange(url, SyncChange::ACTION_UPDATE);
}

void TemplateURLService::AddNoNotify(TemplateURL* template_url) {
  DCHECK(template_url);
  DCHECK(template_url->id() == 0);
  DCHECK(find(template_urls_.begin(), template_urls_.end(), template_url) ==
         template_urls_.end());
  template_url->set_id(++next_id_);
  template_urls_.push_back(template_url);
  AddToMaps(template_url);

  if (service_.get())
    service_->AddKeyword(*template_url);

  // Inform sync of the addition. Note that this will assign a GUID to
  // template_url and add it to the guid_to_template_map_.
  ProcessTemplateURLChange(template_url, SyncChange::ACTION_ADD);
}

void TemplateURLService::RemoveNoNotify(const TemplateURL* template_url) {
  TemplateURLVector::iterator i = find(template_urls_.begin(),
                                       template_urls_.end(),
                                       template_url);
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

  if (service_.get())
    service_->RemoveKeyword(*template_url);

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
    std::vector<TemplateURL*>* template_urls,
    const TemplateURL** default_search_provider,
    const TemplateURL* default_from_prefs) {
  DCHECK(template_urls);
  DCHECK(default_search_provider);
  for (std::vector<TemplateURL*>::iterator i = template_urls->begin();
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
      if (service_.get())
        service_->RemoveKeyword(*template_url);
      delete template_url;
    } else {
      ++i;
    }
  }
}

void TemplateURLService::ResetTemplateURLGUID(const TemplateURL* url,
                                              const std::string& guid) {
  DCHECK(!guid.empty());

  TemplateURL new_url(*url);
  new_url.set_sync_guid(guid);
  UpdateNoNotify(url, new_url);
}

string16 TemplateURLService::UniquifyKeyword(const TemplateURL& turl) const {
  // Already unique.
  if (!GetTemplateURLForKeyword(turl.keyword()))
    return turl.keyword();

  // First, try to return the generated keyword for the TemplateURL.
  string16 keyword_candidate = GenerateKeyword(
      turl.url() ? GURL(turl.url()->url()) : GURL(), true);
  if (!GetTemplateURLForKeyword(keyword_candidate) &&
      !keyword_candidate.empty()) {
    return keyword_candidate;
  }

  // We try to uniquify the keyword by appending a special character to the end.
  // This is a best-effort approach where we try to preserve the original
  // keyword and let the user do what they will after our attempt.
  keyword_candidate = turl.keyword();
  do {
    keyword_candidate.append(UTF8ToUTF16("_"));
  } while (GetTemplateURLForKeyword(keyword_candidate));

  return keyword_candidate;
}

bool TemplateURLService::ResolveSyncKeywordConflict(
    TemplateURL* sync_turl,
    SyncChangeList* change_list) {
  DCHECK(sync_turl);
  DCHECK(change_list);

  const TemplateURL* existing_turl =
      GetTemplateURLForKeyword(sync_turl->keyword());
  // If there is no conflict, or it's just conflicting with itself, return.
  if (!existing_turl || existing_turl->sync_guid() == sync_turl->sync_guid())
    return false;

  if (existing_turl->last_modified() > sync_turl->last_modified() ||
      existing_turl->created_by_policy()) {
    string16 new_keyword = UniquifyKeyword(*sync_turl);
    DCHECK(!GetTemplateURLForKeyword(new_keyword));
    sync_turl->set_keyword(new_keyword);
    // If we update the cloud TURL, we need to push an update back to sync
    // informing it that something has changed.
    SyncData sync_data = CreateSyncDataFromTemplateURL(*sync_turl);
    change_list->push_back(SyncChange(SyncChange::ACTION_UPDATE, sync_data));
  } else {
    string16 new_keyword = UniquifyKeyword(*existing_turl);
    ResetTemplateURL(existing_turl, existing_turl->short_name(), new_keyword,
        existing_turl->url() ? existing_turl->url()->url() : std::string());
  }
  return true;
}

const TemplateURL* TemplateURLService::FindDuplicateOfSyncTemplateURL(
    const TemplateURL& sync_turl) {
  const TemplateURL* existing_turl =
      GetTemplateURLForKeyword(sync_turl.keyword());
  if (!existing_turl)
    return NULL;

  if (existing_turl->url() && sync_turl.url() &&
      existing_turl->url()->url() == sync_turl.url()->url()) {
    return existing_turl;
  }
  return NULL;
}

void TemplateURLService::MergeSyncAndLocalURLDuplicates(
    TemplateURL* sync_turl,
    TemplateURL* local_turl,
    SyncChangeList* change_list) {
  DCHECK(sync_turl);
  DCHECK(local_turl);
  DCHECK(change_list);

  scoped_ptr<TemplateURL> scoped_sync_turl(sync_turl);

  if (scoped_sync_turl->last_modified() > local_turl->last_modified()) {
    // Fully replace local_url with Sync's copy. Note that because use Add
    // rather than ResetTemplateURL, |sync_url| is added with a fresh
    // TemplateURLID. We don't need to sync the new ID back to the server since
    // it's only relevant locally.
    Remove(local_turl);
    // Force the local ID to 0 so we can add it.
    scoped_sync_turl->set_id(0);
    Add(scoped_sync_turl.release());
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
  if (!sync_processor_ || is_default_search_managed_)
    return;

  PrefService* prefs = GetPrefs();
  if (prefs && pending_synced_default_search_ &&
      prefs->GetString(prefs::kSyncedDefaultSearchProviderGUID) == guid) {
    // Make sure this actually exists. We should not be calling this unless we
    // really just added this TemplateURL.
    const TemplateURL* turl_from_sync = GetTemplateURLForGUID(guid);
    DCHECK(turl_from_sync);
    SetDefaultSearchProvider(turl_from_sync);
    pending_synced_default_search_ = false;
  }
}

const TemplateURL* TemplateURLService::GetPendingSyncedDefaultSearchProvider() {
  PrefService* prefs = GetPrefs();
  if (!prefs || !pending_synced_default_search_)
    return NULL;

  // Could be NULL if no such thing exists.
  return GetTemplateURLForGUID(
      prefs->GetString(prefs::kSyncedDefaultSearchProviderGUID));
}

void TemplateURLService::PatchMissingSyncGUIDs(
    std::vector<TemplateURL*>* template_urls) {
  DCHECK(template_urls);
  for (std::vector<TemplateURL*>::iterator i = template_urls->begin();
       i != template_urls->end(); ++i) {
    TemplateURL* template_url = *i;
    DCHECK(template_url);
    if (template_url->sync_guid().empty()) {
      template_url->set_sync_guid(guid::GenerateGUID());
      if (service_.get())
        service_->UpdateKeyword(*template_url);
    }
  }
}
