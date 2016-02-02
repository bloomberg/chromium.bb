// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/chrome_omnibox_client.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_service_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/bookmark_stats.h"
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
#include "chrome/browser/ui/omnibox/chrome_omnibox_edit_controller.h"
#include "chrome/browser/ui/omnibox/chrome_omnibox_navigation_observer.h"
#include "chrome/browser/ui/search/instant_search_prerenderer.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/common/instant_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/search_provider.h"
#include "components/prefs/pref_service.h"
#include "components/search/search.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/constants.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

using predictors::AutocompleteActionPredictor;

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
  if (search::ShouldAllowPrefetchNonDefaultMatch()) {
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
    : controller_(static_cast<ChromeOmniboxEditController*>(controller)),
      profile_(profile),
      scheme_classifier_(profile),
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

scoped_ptr<OmniboxNavigationObserver>
ChromeOmniboxClient::CreateOmniboxNavigationObserver(
    const base::string16& text,
    const AutocompleteMatch& match,
    const AutocompleteMatch& alternate_nav_match) {
  return make_scoped_ptr(new ChromeOmniboxNavigationObserver(
      profile_, text, match, alternate_nav_match));
}

bool ChromeOmniboxClient::CurrentPageExists() const {
  return (controller_->GetWebContents() != NULL);
}

const GURL& ChromeOmniboxClient::GetURL() const {
  return controller_->GetWebContents()->GetVisibleURL();
}

const base::string16& ChromeOmniboxClient::GetTitle() const {
  return controller_->GetWebContents()->GetTitle();
}

gfx::Image ChromeOmniboxClient::GetFavicon() const {
  return favicon::ContentFaviconDriver::FromWebContents(
             controller_->GetWebContents())
      ->GetFavicon();
}

bool ChromeOmniboxClient::IsInstantNTP() const {
  return search::IsInstantNTP(controller_->GetWebContents());
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

bool ChromeOmniboxClient::IsNewTabPage(const std::string& url) const {
  return url == chrome::kChromeUINewTabURL;
}

bool ChromeOmniboxClient::IsHomePage(const std::string& url) const {
  return url == profile_->GetPrefs()->GetString(prefs::kHomePage);
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

const AutocompleteSchemeClassifier&
    ChromeOmniboxClient::GetSchemeClassifier() const {
  return scheme_classifier_;
}

AutocompleteClassifier* ChromeOmniboxClient::GetAutocompleteClassifier() {
  return AutocompleteClassifierFactory::GetForProfile(profile_);
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
    WindowOpenDisposition disposition,
    OmniboxNavigationObserver* observer) {
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

  static_cast<ChromeOmniboxNavigationObserver*>(observer)
      ->OnSuccessfulNavigation();
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
  if (search::IsInstantExtendedAPIEnabled() &&
      ((default_match_changed && result.default_match() != result.end()) ||
       (search::ShouldAllowPrefetchNonDefaultMatch() && !result.empty()))) {
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
      [](const AutocompleteMatch& current) { return !!current.answer; });
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

void ChromeOmniboxClient::OnTextChanged(const AutocompleteMatch& current_match,
                                        bool user_input_in_progress,
                                        base::string16& user_text,
                                        const AutocompleteResult& result,
                                        bool is_popup_open,
                                        bool has_focus) {
  AutocompleteActionPredictor::Action recommended_action =
      AutocompleteActionPredictor::ACTION_NONE;
  if (user_input_in_progress) {
    InstantSearchPrerenderer* prerenderer =
        InstantSearchPrerenderer::GetForProfile(profile_);
    if (prerenderer &&
        prerenderer->IsAllowed(current_match, controller_->GetWebContents()) &&
        is_popup_open && has_focus) {
      recommended_action = AutocompleteActionPredictor::ACTION_PRERENDER;
    } else {
      AutocompleteActionPredictor* action_predictor =
          predictors::AutocompleteActionPredictorFactory::GetForProfile(
              profile_);
      action_predictor->RegisterTransitionalMatches(user_text, result);
      // Confer with the AutocompleteActionPredictor to determine what action,
      // if any, we should take. Get the recommended action here even if we
      // don't need it so we can get stats for anyone who is opted in to UMA,
      // but only get it if the user has actually typed something to avoid
      // constructing it before it's needed. Note: This event is triggered as
      // part of startup when the initial tab transitions to the start page.
      recommended_action =
          action_predictor->RecommendAction(user_text, current_match);
    }
  }

  UMA_HISTOGRAM_ENUMERATION("AutocompleteActionPredictor.Action",
                            recommended_action,
                            AutocompleteActionPredictor::LAST_PREDICT_ACTION);

  switch (recommended_action) {
    case AutocompleteActionPredictor::ACTION_PRERENDER:
      // It's possible that there is no current page, for instance if the tab
      // has been closed or on return from a sleep state.
      // (http://crbug.com/105689)
      if (!CurrentPageExists())
        break;
      // Ask for prerendering if the destination URL is different than the
      // current URL.
      if (current_match.destination_url != GetURL())
        DoPrerender(current_match);
      break;
    case AutocompleteActionPredictor::ACTION_PRECONNECT:
      DoPreconnect(current_match);
      break;
    case AutocompleteActionPredictor::ACTION_NONE:
      break;
  }
}

void ChromeOmniboxClient::OnInputAccepted(const AutocompleteMatch& match) {
  // While the user is typing, the instant search base page may be prerendered
  // in the background. Even though certain inputs may not be eligible for
  // prerendering, the prerender isn't automatically cancelled as the user
  // continues typing, in hopes the final input will end up making use of the
  // prerenderer. Intermediate inputs that are legal for prerendering will be
  // sent to the prerendered page to keep it up to date; then once the user
  // commits a navigation, it will trigger code in chrome::Navigate() to swap in
  // the prerenderer.
  //
  // Unfortunately, that swap code only has the navigated URL, so it doesn't
  // actually know whether the prerenderer has been sent the relevant input
  // already, or whether instead the user manually navigated to something that
  // looks like a search URL (which won't have been sent to the prerenderer).
  // In this case, we need to ensure the prerenderer is cancelled here so that
  // code can't attempt to wrongly swap-in, or it could swap in an empty page in
  // place of the correct navigation.
  //
  // This would be clearer if we could swap in the prerenderer here instead of
  // over in chrome::Navigate(), but we have to wait until then because the
  // final decision about whether to use the prerendered page depends on other
  // parts of the chrome::NavigateParams struct not available until then.
  InstantSearchPrerenderer* prerenderer =
      InstantSearchPrerenderer::GetForProfile(profile_);
  if (prerenderer &&
      !prerenderer->IsAllowed(match, controller_->GetWebContents()))
    prerenderer->Cancel();
}

void ChromeOmniboxClient::OnRevert() {
  AutocompleteActionPredictor* action_predictor =
      predictors::AutocompleteActionPredictorFactory::GetForProfile(profile_);
  action_predictor->ClearTransitionalMatches();
  action_predictor->CancelPrerender();
}

void ChromeOmniboxClient::OnURLOpenedFromOmnibox(OmniboxLog* log) {
  predictors::AutocompleteActionPredictorFactory::GetForProfile(profile_)
      ->OnOmniboxOpenedUrl(*log);
}

void ChromeOmniboxClient::OnBookmarkLaunched() {
  RecordBookmarkLaunch(NULL, BOOKMARK_LAUNCH_LOCATION_OMNIBOX);
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
  DCHECK(search::IsInstantExtendedAPIEnabled());
  content::WebContents* web_contents = controller_->GetWebContents();
  if (web_contents &&
      SearchTabHelper::FromWebContents(web_contents)->IsSearchResultsPage()) {
    if (search::ShouldPrefetchSearchResultsOnSRP()) {
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
