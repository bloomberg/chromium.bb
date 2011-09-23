// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/search_engines_helper.h"

#include <vector>

#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"

using sync_datatype_helper::test;

namespace search_engines_helper {

TemplateURLService* GetServiceForProfile(int index) {
  return TemplateURLServiceFactory::GetForProfile(test()->GetProfile(index));
}

TemplateURLService* GetVerifierService() {
  return TemplateURLServiceFactory::GetForProfile(test()->verifier());
}

GUIDToTURLMap CreateGUIDToTURLMap(TemplateURLService* service) {
  CHECK(service);

  GUIDToTURLMap map;
  std::vector<const TemplateURL*> turls = service->GetTemplateURLs();
  for (std::vector<const TemplateURL*>::iterator it = turls.begin();
       it != turls.end(); ++it) {
    CHECK(*it);
    CHECK(map.find((*it)->sync_guid()) == map.end());
    map[(*it)->sync_guid()] = *it;
  }

  return map;
}

std::string GetTURLInfoString(const TemplateURL* turl) {
  DCHECK(turl);
  std::string shortname = UTF16ToASCII(turl->short_name());
  std::string keyword = UTF16ToASCII(turl->keyword());
  return StringPrintf("TemplateURL: shortname: %s keyword: %s url: %s",
      shortname.c_str(), keyword.c_str(),
      (turl->url() ? turl->url()->url().c_str() : "NULL"));
}

bool TURLsMatch(const TemplateURL* turl1, const TemplateURL* turl2) {
  CHECK(turl1);
  CHECK(turl2);

  // Either both TemplateURLRefs are NULL or they're both valid and have the
  // same raw URL value.
  bool urls_match = ((!turl1->url() && !turl1->url()) ||
      (turl1->url() && turl2->url() &&
      turl1->url()->url() == turl2->url()->url()));

  // Compare all major fields.
  bool result = (urls_match && turl1->keyword() == turl2->keyword() &&
                 turl1->short_name() == turl2->short_name());

  // Print some useful debug info.
  if (!result) {
    LOG(ERROR) << "TemplateURLs did not match: " << GetTURLInfoString(turl1)
               << " vs " << GetTURLInfoString(turl2);
  }

  return result;
}

bool ServiceMatchesVerifier(int profile) {
  TemplateURLService* verifier = GetVerifierService();
  TemplateURLService* other = GetServiceForProfile(profile);

  CHECK(verifier);
  CHECK(other);

  std::vector<const TemplateURL*> verifier_turls = verifier->GetTemplateURLs();
  if (verifier_turls.size() != other->GetTemplateURLs().size()) {
    LOG(ERROR) << "verifier and other service have a different count of TURLs: "
               << verifier_turls.size() << " vs "
               << other->GetTemplateURLs().size() << " respectively.";
    return false;
  }

  for (size_t i = 0; i < verifier_turls.size(); ++i) {
    const TemplateURL* verifier_turl = verifier_turls.at(i);
    CHECK(verifier_turl);
    const TemplateURL* other_turl = other->GetTemplateURLForKeyword(
        verifier_turl->keyword());

    if (!other_turl) {
      LOG(ERROR) << "The other service did not contain a TURL with keyword: "
                 << verifier_turl->keyword();
      return false;
    }
    if (!TURLsMatch(verifier_turl, other_turl))
      return false;
  }

  return true;
}

bool ServicesMatch(int profile_a, int profile_b) {
  TemplateURLService* service_a = GetServiceForProfile(profile_a);
  TemplateURLService* service_b = GetServiceForProfile(profile_b);
  CHECK(service_a);
  CHECK(service_b);

  // Services that have synced should have identical TURLs, including the GUIDs.
  // Make sure we compare those fields in addition to the user-visible fields.
  GUIDToTURLMap a_turls = CreateGUIDToTURLMap(service_a);
  GUIDToTURLMap b_turls = CreateGUIDToTURLMap(service_b);

  if (a_turls.size() != b_turls.size()) {
    LOG(ERROR) << "Service a and b do not match in size: " << a_turls.size()
               << " vs " << b_turls.size() << " respectively.";
    return false;
  }

  for (GUIDToTURLMap::iterator it = a_turls.begin();
       it != a_turls.end(); ++it) {
    if (b_turls.find(it->first) == b_turls.end()) {
      LOG(ERROR) << "TURL GUID from a not found in b's TURLs: " << it->first;
      return false;
    }
    if (!TURLsMatch(b_turls[it->first], it->second))
      return false;
  }

  return true;
}

bool AllServicesMatch() {
  // Use 0 as the baseline.
  if (!ServiceMatchesVerifier(0))
    return false;

  for (int it = 0; it < test()->num_clients(); ++it) {
    if (!ServicesMatch(0, it)) {
      LOG(ERROR) << "TemplateURLService " << it << " does not match with "
                 << "service 0.";
      return false;
    }
  }
  return true;
}

TemplateURL* CreateTestTemplateURL(int seed) {
  TemplateURL* turl = new TemplateURL();
  turl->SetURL(base::StringPrintf("http://www.test%d.com/", seed), 0, 0);
  turl->set_keyword(ASCIIToUTF16(base::StringPrintf("test%d", seed)));
  turl->set_short_name(ASCIIToUTF16(base::StringPrintf("test%d", seed)));
  turl->set_safe_for_autoreplace(true);
  GURL favicon_url("http://favicon.url");
  turl->SetFaviconURL(favicon_url);
  turl->set_date_created(base::Time::FromTimeT(100));
  turl->set_last_modified(base::Time::FromTimeT(100));
  turl->SetPrepopulateId(999999);
  turl->set_sync_guid(base::StringPrintf("0000-0000-0000-%04d", seed));
  return turl;
}

}  // namespace search_engines_helper
