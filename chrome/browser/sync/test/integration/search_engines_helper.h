// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_SEARCH_ENGINES_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_SEARCH_ENGINES_HELPER_H_
#pragma once

#include <map>
#include <string>

class TemplateURL;
class TemplateURLService;

typedef std::map<std::string, const TemplateURL*> GUIDToTURLMap;

namespace search_engines_helper {

// Used to access the search engines within a particular sync profile.
TemplateURLService* GetServiceForProfile(int index);

// Used to access the search engines within the verifier sync profile.
TemplateURLService* GetVerifierService();

// Returns a mapping of |service|'s TemplateURL collection with their sync
// GUIDs as keys.
GUIDToTURLMap CreateGUIDToTURLMap(TemplateURLService* service);

// Returns true iff the major user-visible fields of |turl1| and |turl2| match.
bool TURLsMatch(const TemplateURL* turl1, const TemplateURL* turl2);

// Compared a single TemplateURLService for a given profile to the verifier.
// Retrns true iff their user-visible fields match.
bool ServiceMatchesVerifier(int profile);

// Returns true iff |other|'s TemplateURLs matches the verifier's TemplateURLs
// by sync GUIDs and user-visible fields.
bool ServicesMatch(TemplateURLService* other);

// Returns true iff all TemplateURLServices match with the verifier.
bool AllServicesMatch();

// Create a TemplateURL with some test values based on |seed|. The caller owns
// the returned TemplateURL*.
TemplateURL* CreateTestTemplateURL(int seed);

// Add a search engine based on a seed to the service at index |profile| and the
// verifier if it is used.
void AddSearchEngine(int profile, int seed);

// Retrieves a search engine from the service at index |profile| with original
// keyword |keyword| and changes its user-visible fields. Does the same to the
// verifier, if it is used.
void EditSearchEngine(int profile,
                      const std::string& keyword,
                      const std::string& short_name,
                      const std::string& new_keyword,
                      const std::string& url);

// Deletes a search engine from the service at index |profile| with original
// keyword |keyword|. Does the same to the verifier, if it is used.
void DeleteSearchEngine(int profile, const std::string& keyword);

}  // namespace search_engines_helper

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_SEARCH_ENGINES_HELPER_H_
