// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/new_tab_page_sync_handler.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_ui.h"
#include "net/cookies/cookie_monster.h"
#include "net/url_request/url_request_context.h"
#include "ui/base/l10n/l10n_util.h"

NewTabPageSyncHandler::NewTabPageSyncHandler() : sync_service_(NULL),
  waiting_for_initial_page_load_(true) {
}

NewTabPageSyncHandler::~NewTabPageSyncHandler() {
  if (sync_service_)
    sync_service_->RemoveObserver(this);
}

// static
NewTabPageSyncHandler::MessageType
    NewTabPageSyncHandler::FromSyncStatusMessageType(
        sync_ui_util::MessageType type) {
  switch (type) {
    case sync_ui_util::SYNC_ERROR:
      return SYNC_ERROR;
    case sync_ui_util::SYNC_PROMO:
      return SYNC_PROMO;
    case sync_ui_util::PRE_SYNCED:
    case sync_ui_util::SYNCED:
    default:
      return HIDE;
  }
}

void NewTabPageSyncHandler::RegisterMessages() {
  sync_service_ = ProfileSyncServiceFactory::GetInstance()->GetForProfile(
      Profile::FromWebUI(web_ui()));
  if (sync_service_)
    sync_service_->AddObserver(this);
  profile_pref_registrar_.Init(Profile::FromWebUI(web_ui())->GetPrefs());
  profile_pref_registrar_.Add(
      prefs::kSigninAllowed,
      base::Bind(&NewTabPageSyncHandler::OnSigninAllowedPrefChange,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback("GetSyncMessage",
      base::Bind(&NewTabPageSyncHandler::HandleGetSyncMessage,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("SyncLinkClicked",
      base::Bind(&NewTabPageSyncHandler::HandleSyncLinkClicked,
                 base::Unretained(this)));
}

void NewTabPageSyncHandler::HandleGetSyncMessage(const base::ListValue* args) {
  waiting_for_initial_page_load_ = false;
  BuildAndSendSyncStatus();
}

void NewTabPageSyncHandler::HideSyncStatusSection() {
  SendSyncMessageToPage(HIDE, std::string(), std::string());
}

void NewTabPageSyncHandler::BuildAndSendSyncStatus() {
  DCHECK(!waiting_for_initial_page_load_);
  SigninManagerBase* signin = SigninManagerFactory::GetForProfile(
      Profile::FromWebUI(web_ui()));

  // Hide the sync status section if sync is managed or disabled entirely.
  if (!sync_service_ ||
      sync_service_->IsManaged() ||
      !signin ||
      !signin->IsSigninAllowed()) {
    HideSyncStatusSection();
    return;
  }

  // Don't show sync status if setup is not complete.
  if (!sync_service_->IsFirstSetupComplete()) {
    return;
  }

  // Once sync has been enabled, the supported "sync statuses" for the NNTP
  // from the user's perspective are:
  //
  // "Sync error", when we can't authenticate or establish a connection with
  //               the sync server (appropriate information appended to
  //               message).
  base::string16 status_msg;
  base::string16 link_text;

  sync_ui_util::MessageType type = sync_ui_util::GetStatusLabelsForNewTabPage(
      Profile::FromWebUI(web_ui()), sync_service_, *signin, &status_msg,
      &link_text);
  SendSyncMessageToPage(FromSyncStatusMessageType(type),
                        base::UTF16ToUTF8(status_msg),
                        base::UTF16ToUTF8(link_text));
}

void NewTabPageSyncHandler::HandleSyncLinkClicked(const base::ListValue* args) {
  DCHECK(!waiting_for_initial_page_load_);
  if (!sync_service_ || !sync_service_->IsSyncAllowedByFlag())
    return;
  Browser* browser =
      chrome::FindBrowserWithWebContents(web_ui()->GetWebContents());
  if (!browser || browser->IsAttemptingToCloseBrowser())
    return;
  chrome::ShowBrowserSignin(browser,
                            signin_metrics::AccessPoint::ACCESS_POINT_NTP_LINK);

  if (sync_service_->IsFirstSetupComplete()) {
    base::string16 user = base::UTF8ToUTF16(
        SigninManagerFactory::GetForProfile(Profile::FromWebUI(web_ui()))
            ->GetAuthenticatedAccountInfo()
            .email);
    base::DictionaryValue value;
    value.SetString("syncEnabledMessage",
                    l10n_util::GetStringFUTF16(IDS_SYNC_NTP_SYNCED_TO,
                    user));
    web_ui()->CallJavascriptFunction("ntp.syncAlreadyEnabled", value);
  } else {
    ProfileSyncService::SyncEvent(ProfileSyncService::START_FROM_NTP);
  }
}

void NewTabPageSyncHandler::OnStateChanged() {
  // Don't do anything if the page has not yet loaded.
  if (waiting_for_initial_page_load_)
    return;
  BuildAndSendSyncStatus();
}

void NewTabPageSyncHandler::OnSigninAllowedPrefChange() {
  // Don't do anything if the page has not yet loaded.
  if (waiting_for_initial_page_load_)
    return;
  BuildAndSendSyncStatus();
}

void NewTabPageSyncHandler::SendSyncMessageToPage(MessageType type,
                                                  const std::string& msg,
                                                  const std::string& linktext) {
  base::DictionaryValue value;
  std::string user;
  std::string title;
  std::string linkurl;

  // If there is nothing to show, we should hide the sync section altogether.
  if (type == HIDE || (msg.empty() && linktext.empty())) {
    value.SetBoolean("syncsectionisvisible", false);
  } else {
    if (type == SYNC_ERROR)
      title = l10n_util::GetStringUTF8(IDS_SYNC_NTP_SYNC_SECTION_ERROR_TITLE);
    else if (type == SYNC_PROMO)
      title = l10n_util::GetStringUTF8(IDS_SYNC_NTP_SYNC_SECTION_PROMO_TITLE);
    else
      NOTREACHED();

    value.SetBoolean("syncsectionisvisible", true);
    value.SetString("msg", msg);
    value.SetString("title", title);
    if (linktext.empty()) {
      value.SetBoolean("linkisvisible", false);
    } else {
      value.SetBoolean("linkisvisible", true);
      value.SetString("linktext", linktext);

      // The only time we set the URL is when the user is synced and we need to
      // show a link to a web interface (e.g. http://docs.google.com). When we
      // set that URL, HandleSyncLinkClicked won't be called when the user
      // clicks on the link.
      if (linkurl.empty()) {
        value.SetBoolean("linkurlisset", false);
      } else {
        value.SetBoolean("linkurlisset", true);
        value.SetString("linkurl", linkurl);
      }
    }
  }
  web_ui()->CallJavascriptFunction("ntp.syncMessageChanged", value);
}
