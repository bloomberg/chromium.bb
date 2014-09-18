// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_instant_controller.h"

#include "base/bind.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_model.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/search/instant_search_prerenderer.h"
#include "chrome/browser/ui/search/search_model.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/ntp/app_launcher_handler.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"


// Helpers --------------------------------------------------------------------

namespace {

InstantSearchPrerenderer* GetInstantSearchPrerenderer(Profile* profile) {
  DCHECK(profile);
  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(profile);
  return instant_service ? instant_service->instant_search_prerenderer() : NULL;
}

}  // namespace


// BrowserInstantController ---------------------------------------------------

BrowserInstantController::BrowserInstantController(Browser* browser)
    : browser_(browser),
      instant_(this) {
  browser_->search_model()->AddObserver(this);

  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(profile());
  instant_service->AddObserver(this);
}

BrowserInstantController::~BrowserInstantController() {
  browser_->search_model()->RemoveObserver(this);

  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(profile());
  instant_service->RemoveObserver(this);
}

bool BrowserInstantController::OpenInstant(WindowOpenDisposition disposition,
                                           const GURL& url) {
  // Unsupported dispositions.
  if (disposition == NEW_BACKGROUND_TAB || disposition == NEW_WINDOW ||
      disposition == NEW_FOREGROUND_TAB)
    return false;

  // The omnibox currently doesn't use other dispositions, so we don't attempt
  // to handle them. If you hit this DCHECK file a bug and I'll (sky) add
  // support for the new disposition.
  DCHECK(disposition == CURRENT_TAB) << disposition;

  const base::string16& search_terms =
      chrome::ExtractSearchTermsFromURL(profile(), url);
  if (search_terms.empty())
    return false;

  InstantSearchPrerenderer* prerenderer =
      GetInstantSearchPrerenderer(profile());
  if (prerenderer) {
    if (prerenderer->CanCommitQuery(GetActiveWebContents(), search_terms)) {
      // Submit query to render the prefetched results. Browser will swap the
      // prerendered contents with the active tab contents.
      prerenderer->Commit(search_terms);
      return false;
    } else {
      prerenderer->Cancel();
    }
  }

  // If we will not be replacing search terms from this URL, don't send to
  // InstantController.
  if (!chrome::IsQueryExtractionAllowedForURL(profile(), url))
    return false;

  return instant_.SubmitQuery(search_terms);
}

Profile* BrowserInstantController::profile() const {
  return browser_->profile();
}

content::WebContents* BrowserInstantController::GetActiveWebContents() const {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

void BrowserInstantController::ActiveTabChanged() {
  instant_.ActiveTabChanged();
}

void BrowserInstantController::TabDeactivated(content::WebContents* contents) {
  instant_.TabDeactivated(contents);

  InstantSearchPrerenderer* prerenderer =
      GetInstantSearchPrerenderer(profile());
  if (prerenderer)
    prerenderer->Cancel();
}

void BrowserInstantController::ModelChanged(
    const SearchModel::State& old_state,
    const SearchModel::State& new_state) {
  if (old_state.mode != new_state.mode) {
    const SearchMode& new_mode = new_state.mode;

    // Record some actions corresponding to the mode change. Note that to get
    // the full story, it's necessary to look at other UMA actions as well,
    // such as tab switches.
    if (new_mode.is_search_results())
      content::RecordAction(base::UserMetricsAction("InstantExtended.ShowSRP"));
    else if (new_mode.is_ntp())
      content::RecordAction(base::UserMetricsAction("InstantExtended.ShowNTP"));

    instant_.SearchModeChanged(old_state.mode, new_mode);
  }

  if (old_state.instant_support != new_state.instant_support)
    instant_.InstantSupportChanged(new_state.instant_support);
}

void BrowserInstantController::DefaultSearchProviderChanged() {
  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(profile());
  if (!instant_service)
    return;

  TabStripModel* tab_model = browser_->tab_strip_model();
  int count = tab_model->count();
  for (int index = 0; index < count; ++index) {
    content::WebContents* contents = tab_model->GetWebContentsAt(index);
    if (!contents)
      continue;

    // Send new search URLs to the renderer.
    content::RenderProcessHost* rph = contents->GetRenderProcessHost();
    instant_service->SendSearchURLsToRenderer(rph);

    // Reload the contents to ensure that it gets assigned to a non-priviledged
    // renderer.
    if (!instant_service->IsInstantProcess(rph->GetID()))
      continue;

    contents->GetController().Reload(false);

    // As the reload was not triggered by the user we don't want to close any
    // infobars. We have to tell the InfoBarService after the reload, otherwise
    // it would ignore this call when
    // WebContentsObserver::DidStartNavigationToPendingEntry is invoked.
    InfoBarService::FromWebContents(contents)->set_ignore_next_reload();
  }
}
