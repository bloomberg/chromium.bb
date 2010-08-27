// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/util.h"

#include <set>
#include <vector>

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/search_engines/template_url_prepopulate_data.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profile.h"

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
  return WideToUTF16(default_provider->short_name());
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
  DCHECK(service == NULL || ChromeThread::CurrentlyOn(ChromeThread::UI));

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
  DCHECK(service == NULL || ChromeThread::CurrentlyOn(ChromeThread::UI));
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

  std::vector<TemplateURL*> loaded_urls;
  size_t default_search_index;
  TemplateURLPrepopulateData::GetPrepopulatedEngines(prefs,
                                                     &loaded_urls,
                                                     &default_search_index);

  std::set<int> updated_ids;
  for (size_t i = 0; i < loaded_urls.size(); ++i) {
    // We take ownership of |t_url|.
    scoped_ptr<TemplateURL> t_url(loaded_urls[i]);
    const int t_url_id = t_url->prepopulate_id();
    if (!t_url_id || updated_ids.count(t_url_id)) {
      // Prepopulate engines need a unique id.
      NOTREACHED();
      continue;
    }

    TemplateURL* current_t_url = t_url.get();
    IDMap::iterator existing_url_iter(id_to_turl.find(t_url_id));
    if (existing_url_iter != id_to_turl.end()) {
      current_t_url = existing_url_iter->second;
      if (!current_t_url->safe_for_autoreplace()) {
        // User edited the entry, preserve the keyword and description.
        t_url->set_safe_for_autoreplace(false);
        t_url->set_keyword(current_t_url->keyword());
        t_url->set_autogenerate_keyword(
            current_t_url->autogenerate_keyword());
        t_url->set_short_name(current_t_url->short_name());
      }
      t_url->set_id(current_t_url->id());

      *current_t_url = *t_url;
      if (service) {
        service->UpdateKeyword(*current_t_url);
      }
      id_to_turl.erase(existing_url_iter);
    } else {
      template_urls->push_back(t_url.release());
    }
    if (i == default_search_index && !*default_search_provider)
      *default_search_provider = current_t_url;

    updated_ids.insert(t_url_id);
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
  DCHECK(service == NULL || ChromeThread::CurrentlyOn(ChromeThread::UI));
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
