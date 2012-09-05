// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/omnibox_search_hint.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/api/infobars/confirm_infobar_delegate.h"
#include "chrome/browser/autocomplete/autocomplete_log.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/autocomplete_result.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

using content::NavigationController;
using content::NavigationEntry;

// The URLs of search engines for which we want to trigger the infobar.
const char* const kSearchEngineURLs[] = {
    "http://www.google.com/",
    "http://www.yahoo.com/",
    "http://www.bing.com/",
    "http://www.altavista.com/",
    "http://www.ask.com/",
    "http://www.wolframalpha.com/",
};


// HintInfoBar ----------------------------------------------------------------

class HintInfoBar : public ConfirmInfoBarDelegate {
 public:
  explicit HintInfoBar(OmniboxSearchHint* omnibox_hint);

 private:
  virtual ~HintInfoBar();

  void AllowExpiry() { should_expire_ = true; }

  // ConfirmInfoBarDelegate:
  virtual void InfoBarDismissed() OVERRIDE;
  virtual gfx::Image* GetIcon() const OVERRIDE;
  virtual Type GetInfoBarType() const OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;
  virtual int GetButtons() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool ShouldExpireInternal(
      const content::LoadCommittedDetails& details) const OVERRIDE;

  // The omnibox hint that shows us.
  OmniboxSearchHint* omnibox_hint_;

  // Whether the user clicked one of the buttons.
  bool action_taken_;

  // Whether the info-bar should be dismissed on the next navigation.
  bool should_expire_;

  // Used to delay the expiration of the info-bar.
  base::WeakPtrFactory<HintInfoBar> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(HintInfoBar);
};

HintInfoBar::HintInfoBar(OmniboxSearchHint* omnibox_hint)
    : ConfirmInfoBarDelegate(omnibox_hint->tab()->infobar_tab_helper()),
      omnibox_hint_(omnibox_hint),
      action_taken_(false),
      should_expire_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  // We want the info-bar to stick-around for few seconds and then be hidden
  // on the next navigation after that.
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&HintInfoBar::AllowExpiry, weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(8));
}

HintInfoBar::~HintInfoBar() {
  if (!action_taken_)
    UMA_HISTOGRAM_COUNTS("OmniboxSearchHint.Ignored", 1);
}

void HintInfoBar::InfoBarDismissed() {
  action_taken_ = true;
  UMA_HISTOGRAM_COUNTS("OmniboxSearchHint.Closed", 1);
  // User closed the infobar, let's not bug him again with this in the future.
  omnibox_hint_->DisableHint();
}

gfx::Image* HintInfoBar::GetIcon() const {
  return &ResourceBundle::GetSharedInstance().GetNativeImageNamed(
     IDR_INFOBAR_QUESTION_MARK);
}

InfoBarDelegate::Type HintInfoBar::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

string16 HintInfoBar::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_OMNIBOX_SEARCH_HINT_INFOBAR_TEXT);
}

int HintInfoBar::GetButtons() const {
  return BUTTON_OK;
}

string16 HintInfoBar::GetButtonLabel(InfoBarButton button) const {
  DCHECK_EQ(BUTTON_OK, button);
  return l10n_util::GetStringUTF16(
      IDS_OMNIBOX_SEARCH_HINT_INFOBAR_BUTTON_LABEL);
}

bool HintInfoBar::Accept() {
  action_taken_ = true;
  UMA_HISTOGRAM_COUNTS("OmniboxSearchHint.ShowMe", 1);
  omnibox_hint_->DisableHint();
  omnibox_hint_->ShowEnteringQuery();
  return true;
}

bool HintInfoBar::ShouldExpireInternal(
    const content::LoadCommittedDetails& details) const {
  return should_expire_;
}


// OmniboxSearchHint ----------------------------------------------------------

OmniboxSearchHint::OmniboxSearchHint(TabContents* tab) : tab_(tab) {
  NavigationController* controller = &(tab->web_contents()->GetController());
  notification_registrar_.Add(
      this,
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::Source<NavigationController>(controller));
  // Fill the search_engine_urls_ map, used for faster look-up (overkill?).
  for (size_t i = 0; i < arraysize(kSearchEngineURLs); ++i)
    search_engine_urls_[kSearchEngineURLs[i]] = 1;

  // Listen for omnibox to figure-out when the user searches from the omnibox.
  notification_registrar_.Add(this,
                              chrome::NOTIFICATION_OMNIBOX_OPENED_URL,
                              content::Source<Profile>(tab->profile()));
}

OmniboxSearchHint::~OmniboxSearchHint() {
}

void OmniboxSearchHint::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  if (type == content::NOTIFICATION_NAV_ENTRY_COMMITTED) {
    content::NavigationEntry* entry =
        tab_->web_contents()->GetController().GetActiveEntry();
    if (search_engine_urls_.find(entry->GetURL().spec()) ==
        search_engine_urls_.end()) {
      // The search engine is not in our white-list, bail.
      return;
    }
    const TemplateURL* const default_provider =
        TemplateURLServiceFactory::GetForProfile(tab_->profile())->
        GetDefaultSearchProvider();
    if (!default_provider)
      return;

    if (default_provider->url_ref().GetHost() == entry->GetURL().host())
      ShowInfoBar();
  } else if (type == chrome::NOTIFICATION_OMNIBOX_OPENED_URL) {
    AutocompleteLog* log = content::Details<AutocompleteLog>(details).ptr();
    AutocompleteMatch::Type type =
        log->result.match_at(log->selected_index).type;
    if (AutocompleteMatch::IsSearchType(type)) {
      // The user performed a search from the omnibox, don't show the infobar
      // again.
      DisableHint();
    }
  }
}

void OmniboxSearchHint::ShowInfoBar() {
  tab_->infobar_tab_helper()->AddInfoBar(new HintInfoBar(this));
}

void OmniboxSearchHint::ShowEnteringQuery() {
  LocationBar* location_bar = browser::FindBrowserWithWebContents(
      tab_->web_contents())->window()->GetLocationBar();
  OmniboxView* omnibox_view = location_bar->GetLocationEntry();
  location_bar->FocusLocation(true);
  omnibox_view->SetUserText(
      l10n_util::GetStringUTF16(IDS_OMNIBOX_SEARCH_HINT_OMNIBOX_TEXT));
  omnibox_view->SelectAll(false);
  // Entering text in the omnibox view triggers the suggestion popup that we
  // don't want to show in this case.
  omnibox_view->CloseOmniboxPopup();
}

void OmniboxSearchHint::DisableHint() {
  // The NAV_ENTRY_COMMITTED notification was needed to show the infobar, the
  // OMNIBOX_OPENED_URL notification was there to set the kShowOmniboxSearchHint
  // prefs to false, none of them are needed anymore.
  notification_registrar_.RemoveAll();
  tab_->profile()->GetPrefs()->SetBoolean(prefs::kShowOmniboxSearchHint,
                                          false);
}

// static
bool OmniboxSearchHint::IsEnabled(Profile* profile) {
  // The infobar can only be shown if the correct switch has been provided and
  // the user did not dismiss the infobar before.
  return profile->GetPrefs()->GetBoolean(prefs::kShowOmniboxSearchHint) &&
      CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kSearchInOmniboxHint);
}
