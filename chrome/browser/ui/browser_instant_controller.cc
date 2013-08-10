// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_instant_controller.h"

#include "base/bind.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_model.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/search/instant_ntp.h"
#include "chrome/browser/ui/search/search_model.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/ntp/app_launcher_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"

using content::UserMetricsAction;

////////////////////////////////////////////////////////////////////////////////
// BrowserInstantController, public:

BrowserInstantController::BrowserInstantController(Browser* browser)
    : browser_(browser),
      instant_(this),
      instant_unload_handler_(browser) {
  profile_pref_registrar_.Init(profile()->GetPrefs());
  profile_pref_registrar_.Add(
      prefs::kDefaultSearchProviderID,
      base::Bind(&BrowserInstantController::OnDefaultSearchProviderChanged,
                 base::Unretained(this)));
  browser_->search_model()->AddObserver(this);

  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(profile());
  instant_service->OnBrowserInstantControllerCreated();
}

BrowserInstantController::~BrowserInstantController() {
  browser_->search_model()->RemoveObserver(this);

  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(profile());
  instant_service->OnBrowserInstantControllerDestroyed();
}

bool BrowserInstantController::MaybeSwapInInstantNTPContents(
    const GURL& url,
    content::WebContents* source_contents,
    content::WebContents** target_contents) {
  if (url != GURL(chrome::kChromeUINewTabURL))
    return false;

  GURL extension_url(url);
  if (ExtensionWebUI::HandleChromeURLOverride(&extension_url, profile())) {
    // If there is an extension overriding the NTP do not use the Instant NTP.
    return false;
  }

  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(profile());
  scoped_ptr<content::WebContents> instant_ntp =
      instant_service->ReleaseNTPContents();
  if (!instant_ntp)
    return false;

  *target_contents = instant_ntp.get();
  if (source_contents) {
    // If the Instant NTP hasn't yet committed an entry, we can't call
    // CopyStateFromAndPrune.  Instead, load the Local NTP URL directly in the
    // source contents.
    // TODO(sreeram): Always using the local URL is wrong in the case of the
    // first tab in a window where we might want to use the remote URL. Fix.
    if (!instant_ntp->GetController().CanPruneAllButVisible()) {
      source_contents->GetController().LoadURL(chrome::GetLocalInstantURL(
          profile()), content::Referrer(), content::PAGE_TRANSITION_GENERATED,
          std::string());
      *target_contents = source_contents;
    } else {
      instant_ntp->GetController().CopyStateFromAndPrune(
          &source_contents->GetController());
      ReplaceWebContentsAt(
          browser_->tab_strip_model()->GetIndexOfWebContents(source_contents),
          instant_ntp.Pass());
    }
  } else {
    // If the Instant NTP hasn't yet committed an entry, we can't call
    // PruneAllButVisible.  In that case, there shouldn't be any entries to
    // prune anyway.
    if (instant_ntp->GetController().CanPruneAllButVisible())
      instant_ntp->GetController().PruneAllButVisible();
    else
      CHECK(!instant_ntp->GetController().GetLastCommittedEntry());

    // If |source_contents| is NULL, then the caller is responsible for
    // inserting instant_ntp into the tabstrip and will take ownership.
    ignore_result(instant_ntp.release());
  }
  return true;
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

  // If we will not be replacing search terms from this URL, don't send to
  // InstantController.
  const string16& search_terms =
      chrome::GetSearchTermsFromURL(browser_->profile(), url);
  if (search_terms.empty())
    return false;

  return instant_.SubmitQuery(search_terms);
}

Profile* BrowserInstantController::profile() const {
  return browser_->profile();
}

void BrowserInstantController::ReplaceWebContentsAt(
    int index,
    scoped_ptr<content::WebContents> new_contents) {
  DCHECK_NE(TabStripModel::kNoTab, index);
  scoped_ptr<content::WebContents> old_contents(browser_->tab_strip_model()->
      ReplaceWebContentsAt(index, new_contents.release()));
  instant_unload_handler_.RunUnloadListenersOrDestroy(old_contents.Pass(),
                                                      index);
}

void BrowserInstantController::FocusOmnibox(OmniboxFocusState state) {
  OmniboxView* omnibox_view = browser_->window()->GetLocationBar()->
      GetLocationEntry();

  // Do not add a default case in the switch block for the following reasons:
  // (1) Explicitly handle the new states. If new states are added in the
  // OmniboxFocusState, the compiler will warn the developer to handle the new
  // states.
  // (2) An attacker may control the renderer and sends the browser process a
  // malformed IPC. This function responds to the invalid |state| values by
  // doing nothing instead of crashing the browser process (intentional no-op).
  switch (state) {
    case OMNIBOX_FOCUS_VISIBLE:
      omnibox_view->SetFocus();
      omnibox_view->model()->SetCaretVisibility(true);
      break;
    case OMNIBOX_FOCUS_INVISIBLE:
      omnibox_view->SetFocus();
      omnibox_view->model()->SetCaretVisibility(false);
      // If the user clicked on the fakebox, any text already in the omnibox
      // should get cleared when they start typing. Selecting all the existing
      // text is a convenient way to accomplish this. It also gives a slight
      // visual cue to users who really understand selection state about what
      // will happen if they start typing.
      omnibox_view->SelectAll(false);
      break;
    case OMNIBOX_FOCUS_NONE:
      // Remove focus only if the popup is closed. This will prevent someone
      // from changing the omnibox value and closing the popup without user
      // interaction.
      if (!omnibox_view->model()->popup_model()->IsOpen()) {
        content::WebContents* contents = GetActiveWebContents();
        if (contents)
          contents->GetView()->Focus();
      }
      break;
  }
}

content::WebContents* BrowserInstantController::GetActiveWebContents() const {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

void BrowserInstantController::ActiveTabChanged() {
  instant_.ActiveTabChanged();
}

void BrowserInstantController::TabDeactivated(content::WebContents* contents) {
  instant_.TabDeactivated(contents);
}

void BrowserInstantController::OpenURL(
    const GURL& url,
    content::PageTransition transition,
    WindowOpenDisposition disposition) {
  browser_->OpenURL(content::OpenURLParams(url,
                                           content::Referrer(),
                                           disposition,
                                           transition,
                                           false));
}

void BrowserInstantController::PasteIntoOmnibox(const string16& text) {
  OmniboxView* omnibox_view = browser_->window()->GetLocationBar()->
      GetLocationEntry();
  // The first case is for right click to paste, where the text is retrieved
  // from the clipboard already sanitized. The second case is needed to handle
  // drag-and-drop value and it has to be sanitazed before setting it into the
  // omnibox.
  string16 text_to_paste = text.empty() ?
      omnibox_view->GetClipboardText() :
      omnibox_view->SanitizeTextForPaste(text);

  if (!text_to_paste.empty()) {
    if (!omnibox_view->model()->has_focus())
      omnibox_view->SetFocus();
    omnibox_view->OnBeforePossibleChange();
    omnibox_view->model()->on_paste();
    omnibox_view->SetUserText(text_to_paste);
    omnibox_view->OnAfterPossibleChange();
  }
}

void BrowserInstantController::SetOmniboxBounds(const gfx::Rect& bounds) {
  instant_.SetOmniboxBounds(bounds);
}

void BrowserInstantController::ToggleVoiceSearch() {
  instant_.ToggleVoiceSearch();
}

////////////////////////////////////////////////////////////////////////////////
// BrowserInstantController, SearchModelObserver implementation:

void BrowserInstantController::ModelChanged(
    const SearchModel::State& old_state,
    const SearchModel::State& new_state) {
  if (old_state.mode != new_state.mode) {
    const SearchMode& new_mode = new_state.mode;

    // Record some actions corresponding to the mode change. Note that to get
    // the full story, it's necessary to look at other UMA actions as well,
    // such as tab switches.
    if (new_mode.is_search_results())
      content::RecordAction(UserMetricsAction("InstantExtended.ShowSRP"));
    else if (new_mode.is_ntp())
      content::RecordAction(UserMetricsAction("InstantExtended.ShowNTP"));

    instant_.SearchModeChanged(old_state.mode, new_mode);
  }

  if (old_state.instant_support != new_state.instant_support)
    instant_.InstantSupportChanged(new_state.instant_support);
}

void BrowserInstantController::OnDefaultSearchProviderChanged(
    const std::string& pref_name) {
  DCHECK_EQ(pref_name, std::string(prefs::kDefaultSearchProviderID));

  Profile* browser_profile = profile();
  const TemplateURL* template_url =
      TemplateURLServiceFactory::GetForProfile(browser_profile)->
          GetDefaultSearchProvider();
  if (!template_url) {
    // A NULL |template_url| could mean either this notification is sent during
    // the browser start up operation or the user now has no default search
    // provider. There is no way for the user to reach this state using the
    // Chrome settings. Only explicitly poking at the DB or bugs in the Sync
    // could cause that, neither of which we support.
    return;
  }

  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(browser_profile);
  if (!instant_service)
    return;

  TabStripModel* tab_model = browser_->tab_strip_model();
  int count = tab_model->count();
  for (int index = 0; index < count; ++index) {
    content::WebContents* contents = tab_model->GetWebContentsAt(index);
    if (!contents)
      continue;

    if (!instant_service->IsInstantProcess(
            contents->GetRenderProcessHost()->GetID()))
      continue;

    // Reload the contents to ensure that it gets assigned to a non-priviledged
    // renderer.
    contents->GetController().Reload(false);
  }
}
