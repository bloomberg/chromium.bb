// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/instant_search_prerenderer.h"

#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/prerender/prerender_handle.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/search/search_tab_helper.h"

namespace {

// Returns true if the underlying page supports Instant search.
bool PageSupportsInstantSearch(content::WebContents* contents) {
  // Search results page supports Instant search.
  return SearchTabHelper::FromWebContents(contents)->IsSearchResultsPage();
}

}  // namespace

InstantSearchPrerenderer::InstantSearchPrerenderer(Profile* profile,
                                                   const GURL& url)
    : profile_(profile),
      prerender_url_(url) {
}

InstantSearchPrerenderer::~InstantSearchPrerenderer() {
  if (prerender_handle_)
    prerender_handle_->OnCancel();
}

// static
InstantSearchPrerenderer* InstantSearchPrerenderer::GetForProfile(
    Profile* profile) {
  DCHECK(profile);
  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(profile);
  return instant_service ? instant_service->instant_search_prerenderer() : NULL;
}

void InstantSearchPrerenderer::Init(
    const content::SessionStorageNamespaceMap& session_storage_namespace_map,
    const gfx::Size& size) {
  // TODO(kmadhusu): Enable Instant for Incognito profile.
  if (profile_->IsOffTheRecord())
    return;

  // Only cancel the old prerender after starting the new one, so if the URLs
  // are the same, the underlying prerender will be reused.
  scoped_ptr<prerender::PrerenderHandle> old_prerender_handle(
      prerender_handle_.release());
  prerender::PrerenderManager* prerender_manager =
      prerender::PrerenderManagerFactory::GetForProfile(profile_);
  if (prerender_manager) {
    content::SessionStorageNamespace* session_storage_namespace = NULL;
    content::SessionStorageNamespaceMap::const_iterator it =
        session_storage_namespace_map.find(std::string());
    if (it != session_storage_namespace_map.end())
      session_storage_namespace = it->second.get();

    prerender_handle_.reset(prerender_manager->AddPrerenderForInstant(
        prerender_url_, session_storage_namespace, size));
  }
  if (old_prerender_handle)
    old_prerender_handle->OnCancel();
}

void InstantSearchPrerenderer::Cancel() {
  if (!prerender_handle_)
    return;

  last_instant_suggestion_ = InstantSuggestion();
  prerender_handle_->OnCancel();
  prerender_handle_.reset();
}

void InstantSearchPrerenderer::Prerender(const InstantSuggestion& suggestion) {
  if (!prerender_handle_)
    return;

  if (last_instant_suggestion_.text == suggestion.text)
    return;

  if (last_instant_suggestion_.text.empty() &&
      !prerender_handle_->IsFinishedLoading())
    return;

  if (!prerender_contents())
    return;

  last_instant_suggestion_ = suggestion;
  SearchTabHelper::FromWebContents(prerender_contents())->
      SetSuggestionToPrefetch(suggestion);
}

void InstantSearchPrerenderer::Commit(const base::string16& query) {
  DCHECK(prerender_handle_);
  DCHECK(prerender_contents());
  SearchTabHelper::FromWebContents(prerender_contents())->Submit(query);
}

bool InstantSearchPrerenderer::CanCommitQuery(
    content::WebContents* source,
    const base::string16& query) const {
  if (!source || query.empty() || !prerender_handle_ ||
      !prerender_handle_->IsFinishedLoading() ||
      !prerender_contents() || !QueryMatchesPrefetch(query)) {
    return false;
  }

  // InstantSearchPrerenderer can commit query to the prerendered page only if
  // the underlying |source| page doesn't support Instant search.
  return !PageSupportsInstantSearch(source);
}

bool InstantSearchPrerenderer::UsePrerenderedPage(
    const GURL& url,
    chrome::NavigateParams* params) {
  base::string16 search_terms =
      chrome::ExtractSearchTermsFromURL(profile_, url);
  prerender::PrerenderManager* prerender_manager =
      prerender::PrerenderManagerFactory::GetForProfile(profile_);
  if (search_terms.empty() || !params->target_contents ||
      !prerender_contents() || !prerender_manager ||
      !QueryMatchesPrefetch(search_terms) ||
      params->disposition != CURRENT_TAB) {
    Cancel();
    return false;
  }

  bool success = prerender_manager->MaybeUsePrerenderedPage(
      prerender_contents()->GetURL(), params);
  prerender_handle_.reset();
  return success;
}

bool InstantSearchPrerenderer::IsAllowed(const AutocompleteMatch& match,
                                         content::WebContents* source) const {
  return source && AutocompleteMatch::IsSearchType(match.type) &&
      !PageSupportsInstantSearch(source);
}

content::WebContents* InstantSearchPrerenderer::prerender_contents() const {
  return (prerender_handle_ && prerender_handle_->contents()) ?
      prerender_handle_->contents()->prerender_contents() : NULL;
}

bool InstantSearchPrerenderer::QueryMatchesPrefetch(
    const base::string16& query) const {
  if (chrome::ShouldReuseInstantSearchBasePage())
    return true;
  return last_instant_suggestion_.text == query;
}
