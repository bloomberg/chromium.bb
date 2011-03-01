// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/util.h"

#include <set>
#include <vector>

#include "base/logging.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/search_engines/template_url_prepopulate_data.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "content/browser/browser_thread.h"

string16 GetDefaultSearchEngineName(Profile* profile) {
  if (!profile) {
    NOTREACHED();
    return string16();
  }
  const TemplateURL* const default_provider =
      profile->GetTemplateURLModel()->GetDefaultSearchProvider();
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
          service->RemoveKeyword(**i);
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

// Loads engines from prepopulate data and merges them in with the existing
// engines.  This is invoked when the version of the prepopulate data changes.
void MergeEnginesFromPrepopulateData(
    PrefService* prefs,
    WebDataService* service,
    std::vector<TemplateURL*>* template_urls,
    const TemplateURL** default_search_provider) {
  DCHECK(service == NULL || BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(template_urls);
  DCHECK(default_search_provider);

  // Build a map from prepopulate id to TemplateURL of existing urls.
  typedef std::map<int, TemplateURL*> IDMap;
  IDMap id_to_turl;
  for (std::vector<TemplateURL*>::iterator i(template_urls->begin());
       i != template_urls->end(); ++i) {
    int prepopulate_id = (*i)->prepopulate_id();
    if (prepopulate_id > 0)
      id_to_turl[prepopulate_id] = *i;
  }

  std::vector<TemplateURL*> prepopulated_urls;
  size_t default_search_index;
  TemplateURLPrepopulateData::GetPrepopulatedEngines(prefs,
                                                     &prepopulated_urls,
                                                     &default_search_index);

  std::set<int> updated_ids;
  for (size_t i = 0; i < prepopulated_urls.size(); ++i) {
    // We take ownership of |prepopulated_urls[i]|.
    scoped_ptr<TemplateURL> prepopulated_url(prepopulated_urls[i]);
    const int prepopulated_id = prepopulated_url->prepopulate_id();
    if (!prepopulated_id || updated_ids.count(prepopulated_id)) {
      // Prepopulate engines need a unique id.
      NOTREACHED();
      continue;
    }

    TemplateURL* existing_url = NULL;
    IDMap::iterator existing_url_iter(id_to_turl.find(prepopulated_id));
    if (existing_url_iter != id_to_turl.end()) {
      existing_url = existing_url_iter->second;
      if (!existing_url->safe_for_autoreplace()) {
        // User edited the entry, preserve the keyword and description.
        prepopulated_url->set_safe_for_autoreplace(false);
        prepopulated_url->set_keyword(existing_url->keyword());
        prepopulated_url->set_autogenerate_keyword(
            existing_url->autogenerate_keyword());
        prepopulated_url->set_short_name(existing_url->short_name());
      }
      prepopulated_url->set_id(existing_url->id());

      *existing_url = *prepopulated_url;
      if (service) {
        service->UpdateKeyword(*existing_url);
      }
      id_to_turl.erase(existing_url_iter);
    } else {
      existing_url = prepopulated_url.get();
      template_urls->push_back(prepopulated_url.release());
    }
    DCHECK(existing_url);
    if (i == default_search_index && !*default_search_provider)
      *default_search_provider = existing_url;

    updated_ids.insert(prepopulated_id);
  }

  // Remove any prepopulated engines which are no longer in the master list, as
  // long as the user hasn't modified them or made them the default engine.
  for (IDMap::iterator i(id_to_turl.begin()); i != id_to_turl.end(); ++i) {
    const TemplateURL* template_url = i->second;
    if ((template_url->safe_for_autoreplace()) &&
        (template_url != *default_search_provider)) {
      std::vector<TemplateURL*>::iterator i = find(template_urls->begin(),
                                                   template_urls->end(),
                                                   template_url);
      DCHECK(i != template_urls->end());
      template_urls->erase(i);
       if (service)
         service->RemoveKeyword(*template_url);
      delete template_url;
    }
  }
}

void GetSearchProvidersUsingKeywordResult(
    const WDTypedResult& result,
    WebDataService* service,
    PrefService* prefs,
    std::vector<TemplateURL*>* template_urls,
    const TemplateURL** default_search_provider,
    int* new_resource_keyword_version) {
  DCHECK(service == NULL || BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(template_urls);
  DCHECK(template_urls->empty());
  DCHECK(default_search_provider);
  DCHECK(*default_search_provider == NULL);
  DCHECK(result.GetType() == KEYWORDS_RESULT);
  DCHECK(new_resource_keyword_version);

  *new_resource_keyword_version = 0;
  WDKeywordsResult keyword_result = reinterpret_cast<
      const WDResult<WDKeywordsResult>*>(&result)->GetValue();

  template_urls->swap(keyword_result.keywords);

  const int resource_keyword_version =
      TemplateURLPrepopulateData::GetDataVersion(prefs);
  if (keyword_result.builtin_keyword_version != resource_keyword_version) {
    // There should never be duplicate TemplateURLs. We had a bug such that
    // duplicate TemplateURLs existed for one locale. As such we invoke
    // RemoveDuplicatePrepopulateIDs to nuke the duplicates.
    RemoveDuplicatePrepopulateIDs(template_urls, service);
  }

  if (keyword_result.default_search_provider_id) {
    // See if we can find the default search provider.
    for (std::vector<TemplateURL*>::iterator i = template_urls->begin();
         i != template_urls->end(); ++i) {
      if ((*i)->id() == keyword_result.default_search_provider_id) {
        *default_search_provider = *i;
        break;
      }
    }
  }

  if (keyword_result.builtin_keyword_version != resource_keyword_version) {
    MergeEnginesFromPrepopulateData(prefs, service, template_urls,
                                    default_search_provider);
    *new_resource_keyword_version = resource_keyword_version;
  }
}

