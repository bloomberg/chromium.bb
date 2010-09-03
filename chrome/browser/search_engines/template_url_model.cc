// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/template_url_model.h"

#include "app/l10n_util.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/stl_util-inl.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/google/google_url_tracker.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/rlz/rlz.h"
#include "chrome/browser/search_engines/search_host_to_urls_map.h"
#include "chrome/browser/search_engines/search_terms_data.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model_observer.h"
#include "chrome/browser/search_engines/template_url_prepopulate_data.h"
#include "chrome/browser/search_engines/util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/env_vars.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "net/base/net_util.h"

using base::Time;
typedef SearchHostToURLsMap::TemplateURLSet TemplateURLSet;

// String in the URL that is replaced by the search term.
static const char kSearchTermParameter[] = "{searchTerms}";

// String in Initializer that is replaced with kSearchTermParameter.
static const char kTemplateParameter[] = "%s";

// Term used when generating a search url. Use something obscure so that on
// the rare case the term replaces the URL it's unlikely another keyword would
// have the same url.
static const wchar_t kReplacementTerm[] = L"blah.blah.blah.blah.blah";

class TemplateURLModel::LessWithPrefix {
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

TemplateURLModel::TemplateURLModel(Profile* profile)
    : profile_(profile),
      loaded_(false),
      load_failed_(false),
      load_handle_(0),
      default_search_provider_(NULL),
      next_id_(1) {
  DCHECK(profile_);
  Init(NULL, 0);
}

TemplateURLModel::TemplateURLModel(const Initializer* initializers,
                                   const int count)
    : profile_(NULL),
      loaded_(false),
      load_failed_(false),
      load_handle_(0),
      service_(NULL),
      default_search_provider_(NULL),
      next_id_(1) {
  Init(initializers, count);
}

TemplateURLModel::~TemplateURLModel() {
  if (load_handle_) {
    DCHECK(service_.get());
    service_->CancelRequest(load_handle_);
  }

  STLDeleteElements(&template_urls_);
}

// static
std::wstring TemplateURLModel::GenerateKeyword(const GURL& url,
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
    return std::wstring();

  // Strip "www." off the front of the keyword; otherwise the keyword won't work
  // properly.  See http://code.google.com/p/chromium/issues/detail?id=6984 .
  return net::StripWWW(UTF8ToWide(url.host()));
}

// static
std::wstring TemplateURLModel::CleanUserInputKeyword(
    const std::wstring& keyword) {
  // Remove the scheme.
  std::wstring result(UTF16ToWide(l10n_util::ToLower(WideToUTF16(keyword))));
  url_parse::Component scheme_component;
  if (url_parse::ExtractScheme(WideToUTF8(keyword).c_str(),
                               static_cast<int>(keyword.length()),
                               &scheme_component)) {
    // If the scheme isn't "http" or "https", bail.  The user isn't trying to
    // type a web address, but rather an FTP, file:, or other scheme URL, or a
    // search query with some sort of initial operator (e.g. "site:").
    if (result.compare(0, scheme_component.end(),
                       ASCIIToWide(chrome::kHttpScheme)) &&
        result.compare(0, scheme_component.end(),
                       ASCIIToWide(chrome::kHttpsScheme)))
      return std::wstring();

    // Include trailing ':'.
    result.erase(0, scheme_component.end() + 1);
    // Many schemes usually have "//" after them, so strip it too.
    const std::wstring after_scheme(L"//");
    if (result.compare(0, after_scheme.length(), after_scheme) == 0)
      result.erase(0, after_scheme.length());
  }

  // Remove leading "www.".
  result = net::StripWWW(result);

  // Remove trailing "/".
  return (result.length() > 0 && result[result.length() - 1] == L'/') ?
      result.substr(0, result.length() - 1) : result;
}

// static
GURL TemplateURLModel::GenerateSearchURL(const TemplateURL* t_url) {
  DCHECK(t_url);
  UIThreadSearchTermsData search_terms_data;
  return GenerateSearchURLUsingTermsData(t_url, search_terms_data);
}

// static
GURL TemplateURLModel::GenerateSearchURLUsingTermsData(
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
      *t_url, kReplacementTerm, TemplateURLRef::NO_SUGGESTIONS_AVAILABLE,
      std::wstring(), search_terms_data));
}

bool TemplateURLModel::CanReplaceKeyword(
    const std::wstring& keyword,
    const GURL& url,
    const TemplateURL** template_url_to_replace) {
  DCHECK(!keyword.empty()); // This should only be called for non-empty
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

void TemplateURLModel::FindMatchingKeywords(
    const std::wstring& prefix,
    bool support_replacement_only,
    std::vector<std::wstring>* matches) const {
  // Sanity check args.
  if (prefix.empty())
    return;
  DCHECK(matches != NULL);
  DCHECK(matches->empty());  // The code for exact matches assumes this.

  // Find matching keyword range.  Searches the element map for keywords
  // beginning with |prefix| and stores the endpoints of the resulting set in
  // |match_range|.
  const std::pair<KeywordToTemplateMap::const_iterator,
                  KeywordToTemplateMap::const_iterator> match_range(
      std::equal_range(
          keyword_to_template_map_.begin(), keyword_to_template_map_.end(),
          KeywordToTemplateMap::value_type(prefix, NULL), LessWithPrefix()));

  // Return vector of matching keywords.
  for (KeywordToTemplateMap::const_iterator i(match_range.first);
       i != match_range.second; ++i) {
    DCHECK(i->second->url());
    if (!support_replacement_only || i->second->url()->SupportsReplacement())
      matches->push_back(i->first);
  }
}

const TemplateURL* TemplateURLModel::GetTemplateURLForKeyword(
                                     const std::wstring& keyword) const {
  KeywordToTemplateMap::const_iterator elem(
      keyword_to_template_map_.find(keyword));
  return (elem == keyword_to_template_map_.end()) ? NULL : elem->second;
}

const TemplateURL* TemplateURLModel::GetTemplateURLForHost(
    const std::string& host) const {
  return provider_map_.GetTemplateURLForHost(host);
}

void TemplateURLModel::Add(TemplateURL* template_url) {
  DCHECK(template_url);
  DCHECK(template_url->id() == 0);
  DCHECK(find(template_urls_.begin(), template_urls_.end(), template_url) ==
         template_urls_.end());
  template_url->set_id(++next_id_);
  template_urls_.push_back(template_url);
  AddToMaps(template_url);

  if (service_.get())
    service_->AddKeyword(*template_url);

  if (loaded_) {
    FOR_EACH_OBSERVER(TemplateURLModelObserver, model_observers_,
                      OnTemplateURLModelChanged());
  }
}

void TemplateURLModel::Remove(const TemplateURL* template_url) {
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

  if (loaded_) {
    FOR_EACH_OBSERVER(TemplateURLModelObserver, model_observers_,
                      OnTemplateURLModelChanged());
  }

  if (service_.get())
    service_->RemoveKeyword(*template_url);

  if (profile_) {
    HistoryService* history =
        profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
    if (history)
      history->DeleteAllSearchTermsForKeyword(template_url->id());
  }

  // We own the TemplateURL and need to delete it.
  delete template_url;
}

void TemplateURLModel::RemoveAutoGeneratedBetween(Time created_after,
                                                  Time created_before) {
  for (size_t i = 0; i < template_urls_.size();) {
    if (template_urls_[i]->date_created() >= created_after &&
        (created_before.is_null() ||
         template_urls_[i]->date_created() < created_before) &&
        CanReplace(template_urls_[i])) {
      Remove(template_urls_[i]);
    } else {
      ++i;
    }
  }
}

void TemplateURLModel::RemoveAutoGeneratedSince(Time created_after) {
  RemoveAutoGeneratedBetween(created_after, Time());
}

void TemplateURLModel::RegisterExtensionKeyword(Extension* extension) {
  // TODO(mpcomplete): disable the keyword when the extension is disabled.
  if (extension->omnibox_keyword().empty())
    return;

  Load();
  if (!loaded_) {
    pending_extension_ids_.push_back(extension->id());
    return;
  }

  const TemplateURL* existing_url = GetTemplateURLForExtension(extension);
  std::wstring keyword = UTF8ToWide(extension->omnibox_keyword());

  scoped_ptr<TemplateURL> template_url(new TemplateURL);
  template_url->set_short_name(UTF8ToWide(extension->name()));
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
    Update(existing_url, *template_url);
  } else {
    Add(template_url.release());
  }
}

void TemplateURLModel::UnregisterExtensionKeyword(Extension* extension) {
  const TemplateURL* url = GetTemplateURLForExtension(extension);
  if (url)
    Remove(url);
}

const TemplateURL* TemplateURLModel::GetTemplateURLForExtension(
    Extension* extension) const {
  for (TemplateURLVector::const_iterator i = template_urls_.begin();
       i != template_urls_.end(); ++i) {
    if ((*i)->IsExtensionKeyword() && (*i)->url()->GetHost() == extension->id())
      return *i;
  }

  return NULL;
}

std::vector<const TemplateURL*> TemplateURLModel::GetTemplateURLs() const {
  return template_urls_;
}

void TemplateURLModel::IncrementUsageCount(const TemplateURL* url) {
  DCHECK(url && find(template_urls_.begin(), template_urls_.end(), url) !=
         template_urls_.end());
  const_cast<TemplateURL*>(url)->set_usage_count(url->usage_count() + 1);
  if (service_.get())
    service_.get()->UpdateKeyword(*url);
}

void TemplateURLModel::ResetTemplateURL(const TemplateURL* url,
                                        const std::wstring& title,
                                        const std::wstring& keyword,
                                        const std::string& search_url) {
  TemplateURL new_url(*url);
  new_url.set_short_name(title);
  new_url.set_keyword(keyword);
  if ((new_url.url() && search_url.empty()) ||
      (!new_url.url() && !search_url.empty()) ||
      (new_url.url() && new_url.url()->url() != search_url)) {
    // The urls have changed, reset the favicon url.
    new_url.SetFavIconURL(GURL());
    new_url.SetURL(search_url, 0, 0);
  }
  new_url.set_safe_for_autoreplace(false);
  Update(url, new_url);
}

void TemplateURLModel::SetDefaultSearchProvider(const TemplateURL* url) {
  if (default_search_provider_ == url)
    return;

  DCHECK(!url || find(template_urls_.begin(), template_urls_.end(), url) !=
         template_urls_.end());
  default_search_provider_ = url;

  if (url) {
    TemplateURL* modifiable_url = const_cast<TemplateURL*>(url);
    // Don't mark the url as edited, otherwise we won't be able to rev the
    // templateurls we ship with.
    modifiable_url->set_show_in_default_list(true);
    if (service_.get())
      service_.get()->UpdateKeyword(*url);

    const TemplateURLRef* url_ref = url->url();
    if (url_ref && url_ref->HasGoogleBaseURLs()) {
      GoogleURLTracker::RequestServerCheck();
#if defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)
      RLZTracker::RecordProductEvent(rlz_lib::CHROME,
                                     rlz_lib::CHROME_OMNIBOX,
                                     rlz_lib::SET_TO_GOOGLE);
#endif
    }
  }

  SaveDefaultSearchProviderToPrefs(url);

  if (service_.get())
    service_->SetDefaultSearchProvider(url);

  if (loaded_) {
    FOR_EACH_OBSERVER(TemplateURLModelObserver, model_observers_,
                      OnTemplateURLModelChanged());
  }
}

const TemplateURL* TemplateURLModel::GetDefaultSearchProvider() {
  if (loaded_ && !load_failed_)
    return default_search_provider_;

  if (!prefs_default_search_provider_.get()) {
    TemplateURL* default_from_prefs;
    if (LoadDefaultSearchProviderFromPrefs(&default_from_prefs)) {
      prefs_default_search_provider_.reset(default_from_prefs);
    } else {
      std::vector<TemplateURL*> loaded_urls;
      size_t default_search_index;
      TemplateURLPrepopulateData::GetPrepopulatedEngines(GetPrefs(),
                                                         &loaded_urls,
                                                         &default_search_index);
      if (default_search_index < loaded_urls.size()) {
        prefs_default_search_provider_.reset(loaded_urls[default_search_index]);
        loaded_urls.erase(loaded_urls.begin() + default_search_index);
      }
      STLDeleteElements(&loaded_urls);
    }
  }

  return prefs_default_search_provider_.get();
}

void TemplateURLModel::AddObserver(TemplateURLModelObserver* observer) {
  model_observers_.AddObserver(observer);
}

void TemplateURLModel::RemoveObserver(TemplateURLModelObserver* observer) {
  model_observers_.RemoveObserver(observer);
}

void TemplateURLModel::Load() {
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

void TemplateURLModel::OnWebDataServiceRequestDone(
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

  // prefs_default_search_provider_ is only needed before we've finished
  // loading. Now that we've loaded we can nuke it.
  prefs_default_search_provider_.reset();

  std::vector<TemplateURL*> template_urls;
  const TemplateURL* default_search_provider = NULL;
  int new_resource_keyword_version = 0;
  GetSearchProvidersUsingKeywordResult(*result,
                                       service_.get(),
                                       GetPrefs(),
                                       &template_urls,
                                       &default_search_provider,
                                       &new_resource_keyword_version);

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

  // This initializes provider_map_ which should be done before
  // calling UpdateKeywordSearchTermsForURL.
  ChangeToLoadedState();

  // Index any visits that occurred before we finished loading.
  for (size_t i = 0; i < visits_to_add_.size(); ++i)
    UpdateKeywordSearchTermsForURL(visits_to_add_[i]);
  visits_to_add_.clear();

  if (new_resource_keyword_version && service_.get())
    service_->SetBuiltinKeywordVersion(new_resource_keyword_version);

  FOR_EACH_OBSERVER(TemplateURLModelObserver, model_observers_,
                    OnTemplateURLModelChanged());

  NotifyLoaded();
}

std::wstring TemplateURLModel::GetKeywordShortName(const std::wstring& keyword,
                                                   bool* is_extension_keyword) {
  const TemplateURL* template_url = GetTemplateURLForKeyword(keyword);

  // TODO(sky): Once LocationBarView adds a listener to the TemplateURLModel
  // to track changes to the model, this should become a DCHECK.
  if (template_url) {
    *is_extension_keyword = template_url->IsExtensionKeyword();
    return template_url->AdjustedShortNameForLocaleDirection();
  }
  *is_extension_keyword = false;
  return std::wstring();
}

void TemplateURLModel::Observe(NotificationType type,
                               const NotificationSource& source,
                               const NotificationDetails& details) {
  if (type == NotificationType::HISTORY_URL_VISITED) {
    Details<history::URLVisitedDetails> visit_details(details);

    if (!loaded())
      visits_to_add_.push_back(*visit_details.ptr());
    else
      UpdateKeywordSearchTermsForURL(*visit_details.ptr());
  } else if (type == NotificationType::GOOGLE_URL_UPDATED) {
    if (loaded_)
      GoogleBaseURLChanged();
  } else {
    NOTREACHED();
  }
}

void TemplateURLModel::SetKeywordSearchTermsForURL(const TemplateURL* t_url,
                                                   const GURL& url,
                                                   const std::wstring& term) {
  HistoryService* history = profile_  ?
      profile_->GetHistoryService(Profile::EXPLICIT_ACCESS) : NULL;
  if (!history)
    return;
  history->SetKeywordSearchTermsForURL(url, t_url->id(),
                                       WideToUTF16Hack(term));
}

void TemplateURLModel::Init(const Initializer* initializers,
                            int num_initializers) {
  // Register for notifications.
  if (profile_) {
    // TODO(sky): bug 1166191. The keywords should be moved into the history
    // db, which will mean we no longer need this notification and the history
    // backend can handle automatically adding the search terms as the user
    // navigates.
    registrar_.Add(this, NotificationType::HISTORY_URL_VISITED,
                   Source<Profile>(profile_->GetOriginalProfile()));
  }
  registrar_.Add(this, NotificationType::GOOGLE_URL_UPDATED,
                 NotificationService::AllSources());

  if (num_initializers > 0) {
    // This path is only hit by test code and is used to simulate a loaded
    // TemplateURLModel.
    ChangeToLoadedState();

    // Add specific initializers, if any.
    for (int i(0); i < num_initializers; ++i) {
      DCHECK(initializers[i].keyword);
      DCHECK(initializers[i].url);
      DCHECK(initializers[i].content);

      size_t template_position =
          std::string(initializers[i].url).find(kTemplateParameter);
      DCHECK(template_position != std::wstring::npos);
      std::string osd_url(initializers[i].url);
      osd_url.replace(template_position, arraysize(kTemplateParameter) - 1,
                      kSearchTermParameter);

      // TemplateURLModel ends up owning the TemplateURL, don't try and free it.
      TemplateURL* template_url = new TemplateURL();
      template_url->set_keyword(initializers[i].keyword);
      template_url->set_short_name(initializers[i].content);
      template_url->SetURL(osd_url, 0, 0);
      Add(template_url);
    }
  }

  // Request a server check for the correct Google URL if Google is the default
  // search engine, not in headless mode and not in Chrome Frame.
  const TemplateURL* default_provider = GetDefaultSearchProvider();
  scoped_ptr<base::Environment> env(base::Environment::Create());
  if (default_provider && !env->HasVar(env_vars::kHeadless) &&
      !CommandLine::ForCurrentProcess()->HasSwitch(switches::kChromeFrame)) {
    const TemplateURLRef* default_provider_ref = default_provider->url();
    if (default_provider_ref && default_provider_ref->HasGoogleBaseURLs())
      GoogleURLTracker::RequestServerCheck();
  }
}

void TemplateURLModel::RemoveFromMaps(const TemplateURL* template_url) {
  if (!template_url->keyword().empty())
    keyword_to_template_map_.erase(template_url->keyword());
  if (loaded_)
    provider_map_.Remove(template_url);
}

void TemplateURLModel::RemoveFromKeywordMapByPointer(
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

void TemplateURLModel::AddToMaps(const TemplateURL* template_url) {
  if (!template_url->keyword().empty())
    keyword_to_template_map_[template_url->keyword()] = template_url;
  if (loaded_) {
    UIThreadSearchTermsData search_terms_data;
    provider_map_.Add(template_url, search_terms_data);
  }
}

void TemplateURLModel::SetTemplateURLs(const std::vector<TemplateURL*>& urls) {
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
    Add(*i);
  }
}

void TemplateURLModel::ChangeToLoadedState() {
  DCHECK(!loaded_);

  UIThreadSearchTermsData search_terms_data;
  provider_map_.Init(template_urls_, search_terms_data);
  loaded_ = true;
}

void TemplateURLModel::NotifyLoaded() {
  NotificationService::current()->Notify(
      NotificationType::TEMPLATE_URL_MODEL_LOADED,
      Source<TemplateURLModel>(this),
      NotificationService::NoDetails());

  for (size_t i = 0; i < pending_extension_ids_.size(); ++i) {
    Extension* extension = profile_->GetExtensionsService()->
        GetExtensionById(pending_extension_ids_[i], true);
    if (extension)
      RegisterExtensionKeyword(extension);
  }
  pending_extension_ids_.clear();
}

void TemplateURLModel::SaveDefaultSearchProviderToPrefs(
    const TemplateURL* t_url) {
  PrefService* prefs = GetPrefs();
  if (!prefs)
    return;

  RegisterPrefs(prefs);

  const std::string search_url =
      (t_url && t_url->url()) ? t_url->url()->url() : std::string();
  prefs->SetString(prefs::kDefaultSearchProviderSearchURL, search_url);

  const std::string suggest_url =
      (t_url && t_url->suggestions_url()) ? t_url->suggestions_url()->url() :
                                            std::string();
  prefs->SetString(prefs::kDefaultSearchProviderSuggestURL, suggest_url);

  const std::string name =
      t_url ? WideToUTF8(t_url->short_name()) : std::string();
  prefs->SetString(prefs::kDefaultSearchProviderName, name);

  const std::string id_string =
      t_url ? base::Int64ToString(t_url->id()) : std::string();
  prefs->SetString(prefs::kDefaultSearchProviderID, id_string);

  const std::string prepopulate_id =
      t_url ? base::Int64ToString(t_url->prepopulate_id()) : std::string();
  prefs->SetString(prefs::kDefaultSearchProviderPrepopulateID, prepopulate_id);

  prefs->ScheduleSavePersistentPrefs();
}

bool TemplateURLModel::LoadDefaultSearchProviderFromPrefs(
    TemplateURL** default_provider) {
  PrefService* prefs = GetPrefs();
  if (!prefs || !prefs->HasPrefPath(prefs::kDefaultSearchProviderSearchURL) ||
      !prefs->HasPrefPath(prefs::kDefaultSearchProviderSuggestURL) ||
      !prefs->HasPrefPath(prefs::kDefaultSearchProviderName) ||
      !prefs->HasPrefPath(prefs::kDefaultSearchProviderID)) {
    return false;
  }
  RegisterPrefs(prefs);

  std::string suggest_url =
      prefs->GetString(prefs::kDefaultSearchProviderSuggestURL);
  std::string search_url =
      prefs->GetString(prefs::kDefaultSearchProviderSearchURL);

  if (suggest_url.empty() && search_url.empty()) {
    // The user doesn't want a default search provider.
    *default_provider = NULL;
    return true;
  }

  std::wstring name =
      UTF8ToWide(prefs->GetString(prefs::kDefaultSearchProviderName));

  std::string id_string = prefs->GetString(prefs::kDefaultSearchProviderID);

  std::string prepopulate_id =
      prefs->GetString(prefs::kDefaultSearchProviderPrepopulateID);

  *default_provider = new TemplateURL();
  (*default_provider)->set_short_name(name);
  (*default_provider)->SetURL(search_url, 0, 0);
  (*default_provider)->SetSuggestionsURL(suggest_url, 0, 0);
  if (!id_string.empty()) {
    int64 value;
    base::StringToInt64(id_string, &value);
    (*default_provider)->set_id(value);
  }
  if (!prepopulate_id.empty()) {
    int value;
    base::StringToInt(prepopulate_id, &value);
    (*default_provider)->set_prepopulate_id(value);
  }
  return true;
}

void TemplateURLModel::RegisterPrefs(PrefService* prefs) {
  if (prefs->FindPreference(prefs::kDefaultSearchProviderName))
    return;
  prefs->RegisterStringPref(
      prefs::kDefaultSearchProviderName, std::string());
  prefs->RegisterStringPref(
      prefs::kDefaultSearchProviderID, std::string());
  prefs->RegisterStringPref(
      prefs::kDefaultSearchProviderPrepopulateID, std::string());
  prefs->RegisterStringPref(
      prefs::kDefaultSearchProviderSuggestURL, std::string());
  prefs->RegisterStringPref(
      prefs::kDefaultSearchProviderSearchURL, std::string());
}

bool TemplateURLModel::CanReplaceKeywordForHost(
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

bool TemplateURLModel::CanReplace(const TemplateURL* t_url) {
  return (t_url != default_search_provider_ && !t_url->show_in_default_list() &&
          t_url->safe_for_autoreplace());
}

void TemplateURLModel::Update(const TemplateURL* existing_turl,
                              const TemplateURL& new_values) {
  DCHECK(loaded_);
  DCHECK(existing_turl);
  DCHECK(find(template_urls_.begin(), template_urls_.end(), existing_turl) !=
         template_urls_.end());

  if (!existing_turl->keyword().empty())
    keyword_to_template_map_.erase(existing_turl->keyword());

  // This call handles copying over the values (while retaining the id).
  UIThreadSearchTermsData search_terms_data;
  provider_map_.Update(existing_turl, new_values, search_terms_data);

  if (!existing_turl->keyword().empty())
    keyword_to_template_map_[existing_turl->keyword()] = existing_turl;

  if (service_.get())
    service_->UpdateKeyword(*existing_turl);

  if (default_search_provider_ == existing_turl) {
    // Force an update to happen to account for any changes
    // that occurred during the update.
    default_search_provider_ = NULL;
    SetDefaultSearchProvider(existing_turl);
  }

  FOR_EACH_OBSERVER(TemplateURLModelObserver, model_observers_,
                    OnTemplateURLModelChanged());
}

PrefService* TemplateURLModel::GetPrefs() {
  return profile_ ? profile_->GetPrefs() : NULL;
}

void TemplateURLModel::UpdateKeywordSearchTermsForURL(
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

      if (PageTransition::StripQualifier(details.transition) ==
          PageTransition::KEYWORD) {
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
            *i, row.url(), search_ref->SearchTermToWide(*(*i),
            terms_iterator->second));
      }
    }
  }
}

void TemplateURLModel::AddTabToSearchVisit(const TemplateURL& t_url) {
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

  GURL url(URLFixerUpper::FixupURL(WideToUTF8(t_url.keyword()), std::string()));
  if (!url.is_valid())
    return;

  // Synthesize a visit for the keyword. This ensures the url for the keyword is
  // autocompleted even if the user doesn't type the url in directly.
  history->AddPage(url, NULL, 0, GURL(),
                   PageTransition::KEYWORD_GENERATED,
                   history::RedirectList(), history::SOURCE_BROWSED, false);
}

// static
bool TemplateURLModel::BuildQueryTerms(const GURL& url,
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

void TemplateURLModel::GoogleBaseURLChanged() {
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
    FOR_EACH_OBSERVER(TemplateURLModelObserver, model_observers_,
                      OnTemplateURLModelChanged());
  }
}
