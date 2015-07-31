// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/chrome_omnibox_client.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_service_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/extensions/api/omnibox/omnibox_api.h"
#include "chrome/browser/net/predictor.h"
#include "chrome/browser/predictors/autocomplete_action_predictor.h"
#include "chrome/browser/predictors/autocomplete_action_predictor_factory.h"
#include "chrome/browser/prerender/prerender_field_trial.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_controller.h"
#include "chrome/browser/ui/search/instant_search_prerenderer.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/common/instant_types.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/search_provider.h"
#include "components/search/search.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/constants.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace {

// Returns the AutocompleteMatch that the InstantController should prefetch, if
// any.
//
// The SearchProvider may mark some suggestions to be prefetched based on
// instructions from the suggest server. If such a match ranks sufficiently
// highly or if kAllowPrefetchNonDefaultMatch field trial is enabled, we'll
// return it.
//
// If the kAllowPrefetchNonDefaultMatch field trial is enabled we return the
// prefetch suggestion even if it is not the default match. Otherwise we only
// care about matches that are the default or the second entry in the dropdown
// (which can happen for non-default matches when a top verbatim match is
// shown); for other matches, we think the likelihood of the user selecting
// them is low enough that prefetching isn't worth doing.
const AutocompleteMatch* GetMatchToPrefetch(const AutocompleteResult& result) {
  if (chrome::ShouldAllowPrefetchNonDefaultMatch()) {
    const AutocompleteResult::const_iterator prefetch_match = std::find_if(
        result.begin(), result.end(), SearchProvider::ShouldPrefetch);
    return prefetch_match != result.end() ? &(*prefetch_match) : NULL;
  }

  // If the default match should be prefetched, do that.
  const auto default_match = result.default_match();
  if ((default_match != result.end()) &&
      SearchProvider::ShouldPrefetch(*default_match))
    return &(*default_match);

  // Otherwise, if the top match is a verbatim match and the very next match
  // is prefetchable, fetch that.
  if (result.TopMatchIsStandaloneVerbatimMatch() && (result.size() > 1) &&
      SearchProvider::ShouldPrefetch(result.match_at(1)))
    return &result.match_at(1);

  return NULL;
}

// Calls the specified callback when the requested image is downloaded.  This
// is a separate class instead of being implemented on ChromeOmniboxClient
// because BitmapFetcherService currently takes ownership of this object.
// TODO(dschuyler): Make BitmapFetcherService use the more typical non-owning
// ObserverList pattern and have ChromeOmniboxClient implement the Observer
// call directly.
class AnswerImageObserver : public BitmapFetcherService::Observer {
 public:
  explicit AnswerImageObserver(const BitmapFetchedCallback& callback)
      : callback_(callback) {}

  void OnImageChanged(BitmapFetcherService::RequestId request_id,
                      const SkBitmap& image) override;

 private:
  const BitmapFetchedCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(AnswerImageObserver);
};

void AnswerImageObserver::OnImageChanged(
    BitmapFetcherService::RequestId request_id,
    const SkBitmap& image) {
  DCHECK(!image.empty());
  callback_.Run(image);
}

}  // namespace

ChromeOmniboxClient::ChromeOmniboxClient(OmniboxEditController* controller,
                                         Profile* profile)
    : controller_(controller),
      profile_(profile),
      request_id_(BitmapFetcherService::REQUEST_ID_INVALID) {}

ChromeOmniboxClient::~ChromeOmniboxClient() {
  BitmapFetcherService* image_service =
      BitmapFetcherServiceFactory::GetForBrowserContext(profile_);
  if (image_service)
    image_service->CancelRequest(request_id_);
}

scoped_ptr<AutocompleteProviderClient>
ChromeOmniboxClient::CreateAutocompleteProviderClient() {
  return make_scoped_ptr(new ChromeAutocompleteProviderClient(profile_));
}

bool ChromeOmniboxClient::CurrentPageExists() const {
  return (controller_->GetWebContents() != NULL);
}

const GURL& ChromeOmniboxClient::GetURL() const {
  return controller_->GetWebContents()->GetVisibleURL();
}

bool ChromeOmniboxClient::IsInstantNTP() const {
  return chrome::IsInstantNTP(controller_->GetWebContents());
}

bool ChromeOmniboxClient::IsSearchResultsPage() const {
  Profile* profile = Profile::FromBrowserContext(
      controller_->GetWebContents()->GetBrowserContext());
  return TemplateURLServiceFactory::GetForProfile(profile)->
      IsSearchResultsPageFromDefaultSearchProvider(GetURL());
}

bool ChromeOmniboxClient::IsLoading() const {
  return controller_->GetWebContents()->IsLoading();
}

bool ChromeOmniboxClient::IsPasteAndGoEnabled() const {
  return controller_->command_updater()->IsCommandEnabled(IDC_OPEN_CURRENT_URL);
}

const SessionID& ChromeOmniboxClient::GetSessionID() const {
  return SessionTabHelper::FromWebContents(
      controller_->GetWebContents())->session_id();
}

bookmarks::BookmarkModel* ChromeOmniboxClient::GetBookmarkModel() {
  return BookmarkModelFactory::GetForProfile(profile_);
}

TemplateURLService* ChromeOmniboxClient::GetTemplateURLService() {
  return TemplateURLServiceFactory::GetForProfile(profile_);
}

gfx::Image ChromeOmniboxClient::GetIconIfExtensionMatch(
    const AutocompleteMatch& match) const {
  TemplateURLService* service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  const TemplateURL* template_url = match.GetTemplateURL(service, false);
  if (template_url &&
      (template_url->GetType() == TemplateURL::OMNIBOX_API_EXTENSION)) {
    return extensions::OmniboxAPI::Get(profile_)
        ->GetOmniboxPopupIcon(template_url->GetExtensionId());
  }
  return gfx::Image();
}

bool ChromeOmniboxClient::ProcessExtensionKeyword(
    TemplateURL* template_url,
    const AutocompleteMatch& match,
    WindowOpenDisposition disposition) {
  if (template_url->GetType() != TemplateURL::OMNIBOX_API_EXTENSION)
    return false;

  // Strip the keyword + leading space off the input, but don't exceed
  // fill_into_edit.  An obvious case is that the user may not have entered
  // a leading space and is asking to launch this extension without any
  // additional input.
  size_t prefix_length =
      std::min(match.keyword.length() + 1, match.fill_into_edit.length());
  extensions::ExtensionOmniboxEventRouter::OnInputEntered(
      controller_->GetWebContents(),
      template_url->GetExtensionId(),
      base::UTF16ToUTF8(match.fill_into_edit.substr(prefix_length)),
      disposition);

  return true;
}

void ChromeOmniboxClient::OnInputStateChanged() {
  if (!controller_->GetWebContents())
    return;
  SearchTabHelper::FromWebContents(
      controller_->GetWebContents())->OmniboxInputStateChanged();
}

void ChromeOmniboxClient::OnFocusChanged(
    OmniboxFocusState state,
    OmniboxFocusChangeReason reason) {
  if (!controller_->GetWebContents())
    return;
  SearchTabHelper::FromWebContents(
      controller_->GetWebContents())->OmniboxFocusChanged(state, reason);
}

void ChromeOmniboxClient::OnResultChanged(
    const AutocompleteResult& result,
    bool default_match_changed,
    const base::Callback<void(const SkBitmap& bitmap)>& on_bitmap_fetched) {
  if (chrome::IsInstantExtendedAPIEnabled() &&
      ((default_match_changed && result.default_match() != result.end()) ||
       (chrome::ShouldAllowPrefetchNonDefaultMatch() && !result.empty()))) {
    InstantSuggestion prefetch_suggestion;
    const AutocompleteMatch* match_to_prefetch = GetMatchToPrefetch(result);
    if (match_to_prefetch) {
      prefetch_suggestion.text = match_to_prefetch->contents;
      prefetch_suggestion.metadata =
          SearchProvider::GetSuggestMetadata(*match_to_prefetch);
    }
    // Send the prefetch suggestion unconditionally to the InstantPage. If
    // there is no suggestion to prefetch, we need to send a blank query to
    // clear the prefetched results.
    SetSuggestionToPrefetch(prefetch_suggestion);
  }

  const auto match = std::find_if(
      result.begin(), result.end(),
      [](const AutocompleteMatch& current)->bool { return current.answer; });
  if (match != result.end()) {
    BitmapFetcherService* image_service =
        BitmapFetcherServiceFactory::GetForBrowserContext(profile_);
    if (image_service) {
      image_service->CancelRequest(request_id_);
      request_id_ = image_service->RequestImage(
          match->answer->second_line().image_url(),
          new AnswerImageObserver(
              base::Bind(&ChromeOmniboxClient::OnBitmapFetched,
                         base::Unretained(this), on_bitmap_fetched)));
    }
  }
}

void ChromeOmniboxClient::OnCurrentMatchChanged(
    const AutocompleteMatch& match) {
  if (!prerender::IsOmniboxEnabled(profile_))
    DoPreconnect(match);
}

void ChromeOmniboxClient::OnURLOpenedFromOmnibox(OmniboxLog* log) {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_OMNIBOX_OPENED_URL,
      content::Source<Profile>(profile_),
      content::Details<OmniboxLog>(log));
}

void ChromeOmniboxClient::DiscardNonCommittedNavigations() {
  controller_->GetWebContents()->GetController().DiscardNonCommittedEntries();
}

void ChromeOmniboxClient::DoPrerender(
    const AutocompleteMatch& match) {
  content::WebContents* web_contents = controller_->GetWebContents();
  gfx::Rect container_bounds = web_contents->GetContainerBounds();

  InstantSearchPrerenderer* prerenderer =
      InstantSearchPrerenderer::GetForProfile(profile_);
  if (prerenderer && prerenderer->IsAllowed(match, web_contents)) {
    prerenderer->Init(
        web_contents->GetController().GetDefaultSessionStorageNamespace(),
        container_bounds.size());
    return;
  }

  predictors::AutocompleteActionPredictorFactory::GetForProfile(profile_)->
      StartPrerendering(
          match.destination_url,
          web_contents->GetController().GetDefaultSessionStorageNamespace(),
          container_bounds.size());
}

void ChromeOmniboxClient::DoPreconnect(const AutocompleteMatch& match) {
  if (match.destination_url.SchemeIs(extensions::kExtensionScheme))
    return;

  // Warm up DNS Prefetch cache, or preconnect to a search service.
  UMA_HISTOGRAM_ENUMERATION("Autocomplete.MatchType", match.type,
                            AutocompleteMatchType::NUM_TYPES);
  if (profile_->GetNetworkPredictor()) {
    profile_->GetNetworkPredictor()->AnticipateOmniboxUrl(
        match.destination_url,
        predictors::AutocompleteActionPredictor::IsPreconnectable(match));
  }
  // We could prefetch the alternate nav URL, if any, but because there
  // can be many of these as a user types an initial series of characters,
  // the OS DNS cache could suffer eviction problems for minimal gain.
}

void ChromeOmniboxClient::SetSuggestionToPrefetch(
      const InstantSuggestion& suggestion) {
  DCHECK(chrome::IsInstantExtendedAPIEnabled());
  content::WebContents* web_contents = controller_->GetWebContents();
  if (web_contents &&
      SearchTabHelper::FromWebContents(web_contents)->IsSearchResultsPage()) {
    if (chrome::ShouldPrefetchSearchResultsOnSRP()) {
      SearchTabHelper::FromWebContents(web_contents)->
          SetSuggestionToPrefetch(suggestion);
    }
  } else {
    InstantSearchPrerenderer* prerenderer =
        InstantSearchPrerenderer::GetForProfile(profile_);
    if (prerenderer)
      prerenderer->Prerender(suggestion);
  }
}

void ChromeOmniboxClient::OnBitmapFetched(const BitmapFetchedCallback& callback,
                                          const SkBitmap& bitmap) {
  request_id_ = BitmapFetcherService::REQUEST_ID_INVALID;
  callback.Run(bitmap);
}
