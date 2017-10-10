// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/chrome_omnibox_client.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_service_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/bookmark_stats.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/extensions/api/omnibox/omnibox_api.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/net/predictor.h"
#include "chrome/browser/predictors/autocomplete_action_predictor.h"
#include "chrome/browser/predictors/autocomplete_action_predictor_factory.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/prerender/prerender_field_trial.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/omnibox/chrome_omnibox_edit_controller.h"
#include "chrome/browser/ui/omnibox/chrome_omnibox_navigation_observer.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/search/instant_types.h"
#include "chrome/common/url_constants.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "components/feature_engagement/features.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/search_provider.h"
#include "components/prefs/pref_service.h"
#include "components/search/search.h"
#include "components/search_engines/template_url_service.h"
#include "components/toolbar/toolbar_model.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/constants.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_DESKTOP_IN_PRODUCT_HELP)
#include "chrome/browser/feature_engagement/new_tab/new_tab_tracker.h"
#include "chrome/browser/feature_engagement/new_tab/new_tab_tracker_factory.h"
#endif

using predictors::AutocompleteActionPredictor;

namespace {

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

void OnFaviconFetched(const FaviconFetchedCallback& on_favicon_fetched,
                      const favicon_base::FaviconImageResult& result) {
  on_favicon_fetched.Run(result.image);
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

std::unique_ptr<AutocompleteProviderClient>
ChromeOmniboxClient::CreateAutocompleteProviderClient() {
  return base::MakeUnique<ChromeAutocompleteProviderClient>(profile_);
}

std::unique_ptr<OmniboxNavigationObserver>
ChromeOmniboxClient::CreateOmniboxNavigationObserver(
    const base::string16& text,
    const AutocompleteMatch& match,
    const AutocompleteMatch& alternate_nav_match) {
  return base::MakeUnique<ChromeOmniboxNavigationObserver>(
      profile_, text, match, alternate_nav_match);
}

bool ChromeOmniboxClient::CurrentPageExists() const {
  return (controller_->GetWebContents() != nullptr);
}

const GURL& ChromeOmniboxClient::GetURL() const {
  return CurrentPageExists() ? controller_->GetWebContents()->GetVisibleURL()
                             : GURL::EmptyGURL();
}

const base::string16& ChromeOmniboxClient::GetTitle() const {
  return CurrentPageExists() ? controller_->GetWebContents()->GetTitle()
                             : base::EmptyString16();
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

bool ChromeOmniboxClient::IsNewTabPage(const GURL& url) const {
  return url.spec() == chrome::kChromeUINewTabURL;
}

bool ChromeOmniboxClient::IsHomePage(const GURL& url) const {
  return url.spec() == profile_->GetPrefs()->GetString(prefs::kHomePage);
}

const SessionID& ChromeOmniboxClient::GetSessionID() const {
  return SessionTabHelper::FromWebContents(
      controller_->GetWebContents())->session_id();
}

bookmarks::BookmarkModel* ChromeOmniboxClient::GetBookmarkModel() {
  return BookmarkModelFactory::GetForBrowserContext(profile_);
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
      (template_url->type() == TemplateURL::OMNIBOX_API_EXTENSION)) {
    return extensions::OmniboxAPI::Get(profile_)->GetOmniboxIcon(
        template_url->GetExtensionId());
  }
  return gfx::Image();
}

bool ChromeOmniboxClient::ProcessExtensionKeyword(
    const TemplateURL* template_url,
    const AutocompleteMatch& match,
    WindowOpenDisposition disposition,
    OmniboxNavigationObserver* observer) {
  if (template_url->type() != TemplateURL::OMNIBOX_API_EXTENSION)
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
    const BitmapFetchedCallback& on_bitmap_fetched) {
  const auto match = std::find_if(
      result.begin(), result.end(),
      [](const AutocompleteMatch& current) { return !!current.answer; });
  if (match != result.end()) {
    BitmapFetcherService* image_service =
        BitmapFetcherServiceFactory::GetForBrowserContext(profile_);
    if (image_service) {
      image_service->CancelRequest(request_id_);

      // TODO(jdonnelly, rhalavati): Create a helper function with Callback to
      // create annotation and pass it to image_service, merging this annotation
      // and the one in
      // chrome/browser/autocomplete/chrome_autocomplete_provider_client.cc
      net::NetworkTrafficAnnotationTag traffic_annotation =
          net::DefineNetworkTrafficAnnotation("omnibox_result_change", R"(
            semantics {
              sender: "Omnibox"
              description:
                "Chromium provides answers in the suggestion list for "
                "certain queries that user types in the omnibox. This request "
                "retrieves a small image (for example, an icon illustrating "
                "the current weather conditions) when this can add information "
                "to an answer."
              trigger:
                "Change of results for the query typed by the user in the "
                "omnibox."
              data:
                "The only data sent is the path to an image. No user data is "
                "included, although some might be inferrable (e.g. whether the "
                "weather is sunny or rainy in the user's current location) "
                "from the name of the image in the path."
              destination: WEBSITE
            }
            policy {
              cookies_allowed: YES
              cookies_store: "user"
              setting:
                "You can enable or disable this feature via 'Use a prediction "
                "service to help complete searches and URLs typed in the "
                "address bar.' in Chromium's settings under Advanced. The "
                "feature is enabled by default."
              chrome_policy {
                SearchSuggestEnabled {
                    policy_options {mode: MANDATORY}
                    SearchSuggestEnabled: false
                }
              }
            })");

      request_id_ = image_service->RequestImage(
          match->answer->second_line().image_url(),
          new AnswerImageObserver(
              base::Bind(&ChromeOmniboxClient::OnBitmapFetched,
                         base::Unretained(this), on_bitmap_fetched)),
          traffic_annotation);
    }
  }
}

void ChromeOmniboxClient::GetFaviconForPageUrl(
    base::CancelableTaskTracker* tracker,
    const GURL& page_url,
    const FaviconFetchedCallback& on_favicon_fetched) {
  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile_,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  if (!favicon_service)
    return;

  // TODO(tommycli): Investigate using the version of this method that specifies
  // the desired size.
  favicon_service->GetFaviconImageForPageURL(
      page_url, base::Bind(&OnFaviconFetched, on_favicon_fetched), tracker);
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
    AutocompleteActionPredictor* action_predictor =
        predictors::AutocompleteActionPredictorFactory::GetForProfile(profile_);
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

void ChromeOmniboxClient::OnRevert() {
  AutocompleteActionPredictor* action_predictor =
      predictors::AutocompleteActionPredictorFactory::GetForProfile(profile_);
  action_predictor->ClearTransitionalMatches();
  action_predictor->CancelPrerender();
}

void ChromeOmniboxClient::OnURLOpenedFromOmnibox(OmniboxLog* log) {
// The new tab tracker tracks when a user starts a session in the same
// tab as a previous one. If ShouldDisplayURL() is true, that's a good
// signal that the previous page was part of some other session.
// We could go further to try to analyze the difference between the previous
// and current URLs, but users edit URLs rarely enough that this is a
// reasonable approximation.
#if BUILDFLAG(ENABLE_DESKTOP_IN_PRODUCT_HELP)
  if (controller_->GetToolbarModel()->ShouldDisplayURL()) {
    feature_engagement::NewTabTrackerFactory::GetInstance()
        ->GetForProfile(profile_)
        ->OnOmniboxNavigation();
  }
#endif

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
  auto* loading_predictor =
      predictors::LoadingPredictorFactory::GetForProfile(profile_);
  if (loading_predictor) {
    loading_predictor->PrepareForPageLoad(
        match.destination_url, predictors::HintOrigin::OMNIBOX,
        predictors::AutocompleteActionPredictor::IsPreconnectable(match));
  } else if (profile_->GetNetworkPredictor()) {
    profile_->GetNetworkPredictor()->AnticipateOmniboxUrl(
        match.destination_url,
        predictors::AutocompleteActionPredictor::IsPreconnectable(match));
  }
  // We could prefetch the alternate nav URL, if any, but because there
  // can be many of these as a user types an initial series of characters,
  // the OS DNS cache could suffer eviction problems for minimal gain.
}

void ChromeOmniboxClient::OnBitmapFetched(const BitmapFetchedCallback& callback,
                                          const SkBitmap& bitmap) {
  request_id_ = BitmapFetcherService::REQUEST_ID_INVALID;
  callback.Run(bitmap);
}
