// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINES_UTIL_H_
#define CHROME_BROWSER_SEARCH_ENGINES_UTIL_H_

// This file contains utility functions for search engine functionality.
#include <set>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "chrome/browser/search_engines/template_url_service.h"

class Profile;
class TemplateURL;
class WDTypedResult;
class WebDataService;

// Returns the short name of the default search engine, or the empty string if
// none is set. |profile| may be NULL.
string16 GetDefaultSearchEngineName(Profile* profile);

// Modifies |prepopulated_url| so that it contains user-modified fields from
// |original_turl|. Both URLs must have the same prepopulate_id.
void MergeIntoPrepopulatedEngineData(TemplateURLData* prepopulated_url,
                                     const TemplateURL* original_turl);

// Processes the results of WebDataService::GetKeywords, combining it with
// prepopulated search providers to result in:
//  * a set of template_urls (search providers). The caller owns the
//    TemplateURL* returned in template_urls.
//  * the default search provider (and if *default_search_provider is not NULL,
//    it is contained in template_urls).
//  * whether there is a new resource keyword version (and the value).
//    |*new_resource_keyword_version| is set to 0 if no new value. Otherwise,
//    it is the new value.
// Only pass in a non-NULL value for service if the WebDataService should be
// updated. If |removed_keyword_guids| is not NULL, any TemplateURLs removed
// from the keyword table in the WebDataService will have their Sync GUIDs
// added to it.
void GetSearchProvidersUsingKeywordResult(
    const WDTypedResult& result,
    WebDataService* service,
    Profile* profile,
    TemplateURLService::TemplateURLVector* template_urls,
    TemplateURL** default_search_provider,
    int* new_resource_keyword_version,
    std::set<std::string>* removed_keyword_guids);

// Due to a bug, the |input_encodings| field of TemplateURLData could have
// contained duplicate entries.  This removes those entries and returns whether
// any were found.
bool DeDupeEncodings(std::vector<std::string>* encodings);

// Removes (and deletes) TemplateURLs from |template_urls| and |service| if they
// have duplicate prepopulate ids. If |removed_keyword_guids| is not NULL, the
// Sync GUID of each item removed from the DB will be added to it. This is a
// helper used by GetSearchProvidersUsingKeywordResult(), but is declared here
// so it's accessible by unittests.
void RemoveDuplicatePrepopulateIDs(
    WebDataService* service,
    const ScopedVector<TemplateURL>& prepopulated_urls,
    TemplateURL* default_search_provider,
    TemplateURLService::TemplateURLVector* template_urls,
    std::set<std::string>* removed_keyword_guids);

#endif  // CHROME_BROWSER_SEARCH_ENGINES_UTIL_H_
