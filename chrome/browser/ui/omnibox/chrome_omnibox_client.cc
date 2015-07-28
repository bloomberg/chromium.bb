// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/chrome_omnibox_client.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/extensions/api/omnibox/omnibox_api.h"
#include "chrome/browser/predictors/autocomplete_action_predictor.h"
#include "chrome/browser/predictors/autocomplete_action_predictor_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_controller.h"
#include "chrome/browser/ui/search/instant_search_prerenderer.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/search/search.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

ChromeOmniboxClient::ChromeOmniboxClient(
    OmniboxEditController* controller,
    Profile* profile)
    : controller_(controller),
      profile_(profile) {}

ChromeOmniboxClient::~ChromeOmniboxClient() {}

bool ChromeOmniboxClient::CurrentPageExists() const {
  return (controller_->GetWebContents() != NULL);
}

const GURL& ChromeOmniboxClient::GetURL() const {
  return controller_->GetWebContents()->GetURL();
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

content::NavigationController&
    ChromeOmniboxClient::GetNavigationController() const {
  return controller_->GetWebContents()->GetController();
}

const SessionID& ChromeOmniboxClient::GetSessionID() const {
  return SessionTabHelper::FromWebContents(
      controller_->GetWebContents())->session_id();
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

void ChromeOmniboxClient::OnURLOpenedFromOmnibox(OmniboxLog* log) {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_OMNIBOX_OPENED_URL,
      content::Source<Profile>(profile_),
      content::Details<OmniboxLog>(log));
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
