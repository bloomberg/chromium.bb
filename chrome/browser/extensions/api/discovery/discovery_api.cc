// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/discovery/discovery_api.h"

#include "base/values.h"
#include "chrome/browser/extensions/api/discovery/suggested_link.h"
#include "chrome/browser/extensions/api/discovery/suggested_links_registry.h"
#include "chrome/browser/extensions/api/discovery/suggested_links_registry_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/experimental_discovery.h"
#include "chrome/common/extensions/extension_error_utils.h"

namespace discovery = extensions::api::experimental_discovery;

namespace {
const char kInvalidScore[] = "Invalid score, must be between 0 and 1.";
}  // namespace

namespace extensions {

bool DiscoverySuggestFunction::RunImpl() {
  scoped_ptr<discovery::Suggest::Params> params(
      discovery::Suggest::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  double score = 1.0;
  if (params->details.score != NULL) {
    score = *params->details.score;
    if (score < 0.0 || score > 1.0) {
      error_ = kInvalidScore;
      return false;
    }
  }

  extensions::SuggestedLinksRegistry* registry =
      extensions::SuggestedLinksRegistryFactory::GetForProfile(profile());
  scoped_ptr<extensions::SuggestedLink> suggested_link(
      new extensions::SuggestedLink(params->details.link_url,
                                    params->details.link_text, score));
  registry->Add(extension_id(), suggested_link.Pass());
  return true;
}

bool DiscoveryRemoveSuggestionFunction::RunImpl() {
  scoped_ptr<discovery::RemoveSuggestion::Params> params(
      discovery::RemoveSuggestion::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  extensions::SuggestedLinksRegistry* registry =
      extensions::SuggestedLinksRegistryFactory::GetForProfile(profile());
  registry->Remove(extension_id(), params->link_url);

  return true;
}

bool DiscoveryClearAllSuggestionsFunction::RunImpl() {
  extensions::SuggestedLinksRegistry* registry =
      extensions::SuggestedLinksRegistryFactory::GetForProfile(profile());
  registry->ClearAll(extension_id());

  return true;
}

}  // namespace extensions

