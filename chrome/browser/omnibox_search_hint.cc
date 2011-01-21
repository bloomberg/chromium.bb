// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/omnibox_search_hint.h"

#include "app/l10n_util.h"
#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/task.h"
// TODO(avi): remove when conversions not needed any more
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"

// The URLs of search engines for which we want to trigger the infobar.
const char* kSearchEngineURLs[] = {
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
  virtual bool ShouldExpire(
      const NavigationController::LoadCommittedDetails& details) const;
  virtual void InfoBarDismissed();
  virtual void InfoBarClosed();
  virtual SkBitmap* GetIcon() const;
  virtual Type GetInfoBarType() const;
  virtual string16 GetMessageText() const;
  virtual int GetButtons() const;
  virtual string16 GetButtonLabel(InfoBarButton button) const;
  virtual bool Accept();

  // The omnibox hint that shows us.
  OmniboxSearchHint* omnibox_hint_;

  // Whether the user clicked one of the buttons.
  bool action_taken_;

  // Whether the info-bar should be dismissed on the next navigation.
  bool should_expire_;

  // Used to delay the expiration of the info-bar.
  ScopedRunnableMethodFactory<HintInfoBar> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(HintInfoBar);
};

HintInfoBar::HintInfoBar(OmniboxSearchHint* omnibox_hint)
    : ConfirmInfoBarDelegate(omnibox_hint->tab()),
      omnibox_hint_(omnibox_hint),
      action_taken_(false),
      should_expire_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
  // We want the info-bar to stick-around for few seconds and then be hidden
  // on the next navigation after that.
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      method_factory_.NewRunnableMethod(&HintInfoBar::AllowExpiry),
      8000);  // 8 seconds.
}
HintInfoBar::~HintInfoBar() {
}

bool HintInfoBar::ShouldExpire(
    const NavigationController::LoadCommittedDetails& details) const {
  return should_expire_;
}

void HintInfoBar::InfoBarDismissed() {
  action_taken_ = true;
  UMA_HISTOGRAM_COUNTS("OmniboxSearchHint.Closed", 1);
  // User closed the infobar, let's not bug him again with this in the future.
  omnibox_hint_->DisableHint();
}

void HintInfoBar::InfoBarClosed() {
  if (!action_taken_)
    UMA_HISTOGRAM_COUNTS("OmniboxSearchHint.Ignored", 1);
  delete this;
}

SkBitmap* HintInfoBar::GetIcon() const {
  return ResourceBundle::GetSharedInstance().GetBitmapNamed(
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


// OmniboxSearchHint ----------------------------------------------------------

OmniboxSearchHint::OmniboxSearchHint(TabContents* tab) : tab_(tab) {
  NavigationController* controller = &(tab->controller());
  notification_registrar_.Add(this,
                              NotificationType::NAV_ENTRY_COMMITTED,
                              Source<NavigationController>(controller));
  // Fill the search_engine_urls_ map, used for faster look-up (overkill?).
  for (size_t i = 0;
       i < sizeof(kSearchEngineURLs) / sizeof(kSearchEngineURLs[0]); ++i) {
    search_engine_urls_[kSearchEngineURLs[i]] = 1;
  }

  // Listen for omnibox to figure-out when the user searches from the omnibox.
  notification_registrar_.Add(this,
                              NotificationType::OMNIBOX_OPENED_URL,
                              Source<Profile>(tab->profile()));
}

OmniboxSearchHint::~OmniboxSearchHint() {
}

void OmniboxSearchHint::Observe(NotificationType type,
                                const NotificationSource& source,
                                const NotificationDetails& details) {
  if (type == NotificationType::NAV_ENTRY_COMMITTED) {
    NavigationEntry* entry = tab_->controller().GetActiveEntry();
    if (search_engine_urls_.find(entry->url().spec()) ==
        search_engine_urls_.end()) {
      // The search engine is not in our white-list, bail.
      return;
    }
    const TemplateURL* const default_provider =
        tab_->profile()->GetTemplateURLModel()->GetDefaultSearchProvider();
    if (!default_provider)
      return;

    const TemplateURLRef* const search_url = default_provider->url();
    if (search_url->GetHost() == entry->url().host())
      ShowInfoBar();
  } else if (type == NotificationType::OMNIBOX_OPENED_URL) {
    AutocompleteLog* log = Details<AutocompleteLog>(details).ptr();
    AutocompleteMatch::Type type =
        log->result.match_at(log->selected_index).type;
    if (type == AutocompleteMatch::SEARCH_WHAT_YOU_TYPED ||
        type == AutocompleteMatch::SEARCH_HISTORY ||
        type == AutocompleteMatch::SEARCH_SUGGEST) {
      // The user performed a search from the omnibox, don't show the infobar
      // again.
      DisableHint();
    }
  }
}

void OmniboxSearchHint::ShowInfoBar() {
  tab_->AddInfoBar(new HintInfoBar(this));
}

void OmniboxSearchHint::ShowEnteringQuery() {
  LocationBar* location_bar = BrowserList::GetLastActive()->window()->
      GetLocationBar();
  AutocompleteEditView*  edit_view = location_bar->location_entry();
  location_bar->FocusLocation(true);
  edit_view->SetUserText(UTF16ToWideHack(
      l10n_util::GetStringUTF16(IDS_OMNIBOX_SEARCH_HINT_OMNIBOX_TEXT)));
  edit_view->SelectAll(false);
  // Entering text in the autocomplete edit view triggers the suggestion popup
  // that we don't want to show in this case.
  edit_view->ClosePopup();
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
