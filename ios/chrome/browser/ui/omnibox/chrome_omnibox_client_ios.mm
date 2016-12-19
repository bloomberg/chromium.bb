// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/omnibox/chrome_omnibox_client_ios.h"

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/favicon/ios/web_favicon_driver.h"
#include "components/omnibox/browser/omnibox_edit_controller.h"
#include "components/search_engines/template_url_service.h"
#include "ios/chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "ios/chrome/browser/autocomplete/autocomplete_provider_client_impl.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/bookmarks/bookmarks_utils.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#include "ios/chrome/browser/sessions/ios_chrome_session_tab_helper.h"
#include "ios/chrome/browser/ui/omnibox/web_omnibox_edit_controller.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/web_state/web_state.h"
#include "url/gurl.h"

ChromeOmniboxClientIOS::ChromeOmniboxClientIOS(
    WebOmniboxEditController* controller,
    ios::ChromeBrowserState* browser_state)
    : controller_(controller), browser_state_(browser_state) {}

ChromeOmniboxClientIOS::~ChromeOmniboxClientIOS() {}

std::unique_ptr<AutocompleteProviderClient>
ChromeOmniboxClientIOS::CreateAutocompleteProviderClient() {
  return base::MakeUnique<AutocompleteProviderClientImpl>(browser_state_);
}

std::unique_ptr<OmniboxNavigationObserver>
ChromeOmniboxClientIOS::CreateOmniboxNavigationObserver(
    const base::string16& text,
    const AutocompleteMatch& match,
    const AutocompleteMatch& alternate_nav_match) {
  // TODO(blundell): Bring up an OmniboxNavigationObserver implementation on
  // iOS if/once iOS wants to start using the ShortcutsProvider.
  // crbug.com/511965
  return nullptr;
}

bool ChromeOmniboxClientIOS::CurrentPageExists() const {
  return (controller_->GetWebState() != nullptr);
}

const GURL& ChromeOmniboxClientIOS::GetURL() const {
  return CurrentPageExists() ? controller_->GetWebState()->GetVisibleURL()
                             : GURL::EmptyGURL();
}

bool ChromeOmniboxClientIOS::IsLoading() const {
  return controller_->GetWebState()->IsLoading();
}

bool ChromeOmniboxClientIOS::IsPasteAndGoEnabled() const {
  return false;
}

bool ChromeOmniboxClientIOS::IsInstantNTP() const {
  // This is currently only called by the OmniboxEditModel to determine if the
  // Google landing page is showing.
  // TODO(lliabraa): This should also check the user's default search engine
  // because if they're not using Google the Google landing page is not shown
  // (crbug/315563).
  GURL currentURL = controller_->GetWebState()->GetVisibleURL();
  return currentURL == GURL(kChromeUINewTabURL);
}

bool ChromeOmniboxClientIOS::IsSearchResultsPage() const {
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(
          controller_->GetWebState()->GetBrowserState());
  return ios::TemplateURLServiceFactory::GetForBrowserState(browser_state)
      ->IsSearchResultsPageFromDefaultSearchProvider(GetURL());
}

bool ChromeOmniboxClientIOS::IsNewTabPage(const std::string& url) const {
  return url == kChromeUINewTabURL;
}

bool ChromeOmniboxClientIOS::IsHomePage(const std::string& url) const {
  // iOS does not have a notion of home page.
  return false;
}

const SessionID& ChromeOmniboxClientIOS::GetSessionID() const {
  return IOSChromeSessionTabHelper::FromWebState(controller_->GetWebState())
      ->session_id();
}

bookmarks::BookmarkModel* ChromeOmniboxClientIOS::GetBookmarkModel() {
  return ios::BookmarkModelFactory::GetForBrowserState(browser_state_);
}

TemplateURLService* ChromeOmniboxClientIOS::GetTemplateURLService() {
  return ios::TemplateURLServiceFactory::GetForBrowserState(browser_state_);
}

const AutocompleteSchemeClassifier&
ChromeOmniboxClientIOS::GetSchemeClassifier() const {
  return scheme_classifier_;
}

AutocompleteClassifier* ChromeOmniboxClientIOS::GetAutocompleteClassifier() {
  return ios::AutocompleteClassifierFactory::GetForBrowserState(browser_state_);
}

gfx::Image ChromeOmniboxClientIOS::GetIconIfExtensionMatch(
    const AutocompleteMatch& match) const {
  // Extensions are not supported on iOS.
  return gfx::Image();
}

bool ChromeOmniboxClientIOS::ProcessExtensionKeyword(
    TemplateURL* template_url,
    const AutocompleteMatch& match,
    WindowOpenDisposition disposition,
    OmniboxNavigationObserver* observer) {
  // Extensions are not supported on iOS.
  return false;
}

void ChromeOmniboxClientIOS::OnInputStateChanged() {}

void ChromeOmniboxClientIOS::OnFocusChanged(OmniboxFocusState state,
                                            OmniboxFocusChangeReason reason) {}

void ChromeOmniboxClientIOS::OnResultChanged(
    const AutocompleteResult& result,
    bool default_match_changed,
    const base::Callback<void(const SkBitmap& bitmap)>& on_bitmap_fetched) {}

void ChromeOmniboxClientIOS::OnCurrentMatchChanged(const AutocompleteMatch&) {}

void ChromeOmniboxClientIOS::OnURLOpenedFromOmnibox(OmniboxLog* log) {}

void ChromeOmniboxClientIOS::OnBookmarkLaunched() {
  RecordBookmarkLaunch(BOOKMARK_LAUNCH_LOCATION_OMNIBOX);
}

void ChromeOmniboxClientIOS::DiscardNonCommittedNavigations() {
  controller_->GetWebState()
      ->GetNavigationManager()
      ->DiscardNonCommittedItems();
}

const base::string16& ChromeOmniboxClientIOS::GetTitle() const {
  return controller_->GetWebState()->GetTitle();
}

gfx::Image ChromeOmniboxClientIOS::GetFavicon() const {
  return favicon::WebFaviconDriver::FromWebState(controller_->GetWebState())
      ->GetFavicon();
}

void ChromeOmniboxClientIOS::OnTextChanged(
    const AutocompleteMatch& current_match,
    bool user_input_in_progress,
    base::string16& user_text,
    const AutocompleteResult& result,
    bool is_popup_open,
    bool has_focus) {}

void ChromeOmniboxClientIOS::OnInputAccepted(const AutocompleteMatch& match) {}

void ChromeOmniboxClientIOS::OnRevert() {}
