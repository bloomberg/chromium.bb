// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/util.h"

#include <set>
#include <vector>

#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_prepopulate_data.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

string16 GetDefaultSearchEngineName(Profile* profile) {
  if (!profile) {
    NOTREACHED();
    return string16();
  }
  const TemplateURL* const default_provider =
      TemplateURLServiceFactory::GetForProfile(profile)->
      GetDefaultSearchProvider();
  if (!default_provider) {
    // TODO(cpu): bug 1187517. It is possible to have no default provider.
    // returning an empty string is a stopgap measure for the crash
    // http://code.google.com/p/chromium/issues/detail?id=2573
    return string16();
  }
  return default_provider->short_name();
}

// Removes (and deletes) TemplateURLs from |urls| that have duplicate
// prepopulate ids. Duplicate prepopulate ids are not allowed, but due to a
// bug it was possible get dups. This step is only called when the version
// number changes. Only pass in a non-NULL value for |service| if the removed
// items should be removed from the DB.
static void RemoveDuplicatePrepopulateIDs(
    std::vector<TemplateURL*>* template_urls,
    WebDataService* service) {
  DCHECK(template_urls);
  DCHECK(service == NULL || BrowserThread::CurrentlyOn(BrowserThread::UI));

  std::set<int> ids;
  for (std::vector<TemplateURL*>::iterator i = template_urls->begin();
       i != template_urls->end(); ) {
    int prepopulate_id = (*i)->prepopulate_id();
    if (prepopulate_id) {
      if (ids.find(prepopulate_id) != ids.end()) {
        if (service)
          service->RemoveKeyword((*i)->id());
        delete *i;
        i = template_urls->erase(i);
      } else {
        ids.insert(prepopulate_id);
        ++i;
      }
    } else {
      ++i;
    }
  }
}

// Returns the TemplateURL with id specified from the list of TemplateURLs.
// If not found, returns NULL.
TemplateURL* GetTemplateURLByID(
    const std::vector<TemplateURL*>& template_urls,
    int64 id) {
  for (std::vector<TemplateURL*>::const_iterator i = template_urls.begin();
       i != template_urls.end(); ++i) {
    if ((*i)->id() == id) {
      return *i;
    }
  }
  return NULL;
}

// Loads engines from prepopulate data and merges them in with the existing
// engines.  This is invoked when the version of the prepopulate data changes.
void MergeEnginesFromPrepopulateData(
    Profile* profile,
    WebDataService* service,
    std::vector<TemplateURL*>* template_urls,
    TemplateURL** default_search_provider) {
  DCHECK(service == NULL || BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(template_urls);
  DCHECK(default_search_provider);

  // Create a map to hold all provided |template_urls| that originally came from
  // prepopulate data (i.e. have a non-zero prepopulate_id()).
  typedef std::map<int, TemplateURL*> IDMap;
  IDMap id_to_turl;
  for (std::vector<TemplateURL*>::iterator i(template_urls->begin());
       i != template_urls->end(); ++i) {
    int prepopulate_id = (*i)->prepopulate_id();
    if (prepopulate_id > 0)
      id_to_turl[prepopulate_id] = *i;
  }

  // Get the current set of prepopulatd URLs.
  std::vector<TemplateURL*> prepopulated_urls;
  size_t default_search_index;
  TemplateURLPrepopulateData::GetPrepopulatedEngines(profile,
      &prepopulated_urls, &default_search_index);

  // For each current prepopulated URL, check whether |template_urls| contained
  // a matching prepopulated URL.  If so, update the passed-in URL to match the
  // current data.  (If the passed-in URL was user-edited, we persist the user's
  // name and keyword.)  If not, add the prepopulated URL to |template_urls|.
  // Along the way, point |default_search_provider| at the default prepopulated
  // URL, if the user hasn't already set another URL as default.
  for (size_t i = 0; i < prepopulated_urls.size(); ++i) {
    // We take ownership of |prepopulated_urls[i]|.
    scoped_ptr<TemplateURL> prepopulated_url(prepopulated_urls[i]);
    const int prepopulated_id = prepopulated_url->prepopulate_id();
    DCHECK_NE(0, prepopulated_id);

    TemplateURL* url_in_vector = NULL;
    IDMap::iterator existing_url_iter(id_to_turl.find(prepopulated_id));
    if (existing_url_iter != id_to_turl.end()) {
      // Update the data store with the new prepopulated data.  Preserve user
      // edits to the name and keyword.
      TemplateURLData data(prepopulated_url->data());
      scoped_ptr<TemplateURL> existing_url(existing_url_iter->second);
      id_to_turl.erase(existing_url_iter);
      if (!existing_url->safe_for_autoreplace()) {
        data.safe_for_autoreplace = false;
        data.SetKeyword(existing_url->keyword());
        data.short_name = existing_url->short_name();
      }
      data.id = existing_url->id();
      if (service)
        service->UpdateKeyword(data);

      // Replace the entry in |template_urls| with the updated one.
      std::vector<TemplateURL*>::iterator j = std::find(template_urls->begin(),
          template_urls->end(), existing_url.get());
      *j = new TemplateURL(profile, data);
      url_in_vector = *j;
      if (*default_search_provider == existing_url.get())
        *default_search_provider = url_in_vector;
    } else {
      template_urls->push_back(prepopulated_url.release());
      url_in_vector = template_urls->back();
    }
    DCHECK(url_in_vector);
    if (i == default_search_index && !*default_search_provider)
      *default_search_provider = url_in_vector;
  }

  // The block above removed all the URLs from the |id_to_turl| map that were
  // found in the prepopulate data.  Any remaining URLs that haven't been
  // user-edited or made default can be removed from the data store.
  for (IDMap::iterator i(id_to_turl.begin()); i != id_to_turl.end(); ++i) {
    const TemplateURL* template_url = i->second;
    if ((template_url->safe_for_autoreplace()) &&
        (template_url != *default_search_provider)) {
      std::vector<TemplateURL*>::iterator j =
          std::find(template_urls->begin(), template_urls->end(), template_url);
      DCHECK(j != template_urls->end());
      template_urls->erase(j);
       if (service)
         service->RemoveKeyword(template_url->id());
      delete template_url;
    }
  }
}

void GetSearchProvidersUsingKeywordResult(
    const WDTypedResult& result,
    WebDataService* service,
    Profile* profile,
    std::vector<TemplateURL*>* template_urls,
    TemplateURL** default_search_provider,
    int* new_resource_keyword_version) {
  DCHECK(service == NULL || BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(template_urls);
  DCHECK(template_urls->empty());
  DCHECK(default_search_provider);
  DCHECK(*default_search_provider == NULL);
  DCHECK_EQ(result.GetType(), KEYWORDS_RESULT);
  DCHECK(new_resource_keyword_version);

  *new_resource_keyword_version = 0;
  WDKeywordsResult keyword_result = reinterpret_cast<
      const WDResult<WDKeywordsResult>*>(&result)->GetValue();

  for (KeywordTable::Keywords::const_iterator i(
       keyword_result.keywords.begin()); i != keyword_result.keywords.end();
       ++i)
    template_urls->push_back(new TemplateURL(profile, *i));

  const int resource_keyword_version =
      TemplateURLPrepopulateData::GetDataVersion(
          profile ? profile->GetPrefs() : NULL);
  if (keyword_result.builtin_keyword_version != resource_keyword_version) {
    // There should never be duplicate TemplateURLs. We had a bug such that
    // duplicate TemplateURLs existed for one locale. As such we invoke
    // RemoveDuplicatePrepopulateIDs to nuke the duplicates.
    RemoveDuplicatePrepopulateIDs(template_urls, service);
  }

  int64 default_search_provider_id = keyword_result.default_search_provider_id;
  if (default_search_provider_id) {
    *default_search_provider =
        GetTemplateURLByID(*template_urls, default_search_provider_id);
  }

  if (keyword_result.builtin_keyword_version != resource_keyword_version) {
    MergeEnginesFromPrepopulateData(profile, service, template_urls,
                                    default_search_provider);
    *new_resource_keyword_version = resource_keyword_version;
  }
}

bool DidDefaultSearchProviderChange(
    const WDTypedResult& result,
    Profile* profile,
    scoped_ptr<TemplateURL>* backup_default_search_provider) {
  DCHECK(backup_default_search_provider);
  DCHECK(!backup_default_search_provider->get());
  DCHECK_EQ(result.GetType(), KEYWORDS_RESULT);

  WDKeywordsResult keyword_result = reinterpret_cast<
      const WDResult<WDKeywordsResult>*>(&result)->GetValue();

  if (!keyword_result.did_default_search_provider_change)
    return false;

  if (keyword_result.backup_valid) {
    backup_default_search_provider->reset(new TemplateURL(profile,
        keyword_result.default_search_provider_backup));
  }
  return true;
}

