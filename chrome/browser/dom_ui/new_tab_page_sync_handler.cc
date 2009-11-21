// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/new_tab_page_sync_handler.h"

#include "app/l10n_util.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "net/base/cookie_monster.h"
#include "net/url_request/url_request_context.h"

// Default URL for the sync web interface.
//
// TODO(idana): when we figure out how we are going to allow third parties to
// plug in their own sync engine, we should allow this value to be
// configurable.
static const char kSyncDefaultViewOnlineUrl[] = "http://docs.google.com";

// TODO(idana): the following code was originally copied from
// toolbar_importer.h/cc and it needs to be moved to a common Google Accounts
// utility.

// A simple pair of fields that identify a set of Google cookies, used to
// filter from a larger set.
struct GoogleCookieFilter {
  // The generalized, fully qualified URL of pages where
  // cookies with id |cookie_id| are obtained / accessed.
  const char* url;
  // The id of the cookie this filter is selecting,
  // with name/value delimiter (i.e '=').
  const char* cookie_id;
};

// Filters to select Google GAIA cookies.
static const GoogleCookieFilter kGAIACookieFilters[] = {
  { "http://.google.com/",       "SID=" },     // Gmail.
  // Add filters here for other interesting cookies that should result in
  // showing the promotions (e.g ASIDAS for dasher accounts).
};

bool IsGoogleGAIACookieInstalled() {
  for (size_t i = 0; i < arraysize(kGAIACookieFilters); ++i) {
    // Since we are running on the UI thread don't call GetURLRequestContext().
    net::CookieStore* store =
        Profile::GetDefaultRequestContext()->GetCookieStore();
    GURL url(kGAIACookieFilters[i].url);
    net::CookieOptions options;
    options.set_include_httponly();  // The SID cookie might be httponly.
    std::string cookies = store->GetCookiesWithOptions(url, options);
    std::vector<std::string> cookie_list;
    SplitString(cookies, ';', &cookie_list);
    for (std::vector<std::string>::iterator current = cookie_list.begin();
         current != cookie_list.end();
         ++current) {
      size_t position =
          current->find(kGAIACookieFilters[i].cookie_id);
      if (0 == position)
        return true;
    }
  }
  return false;
}

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
        SyncStatusUIHelper::MessageType type) {
  switch (type) {
    case SyncStatusUIHelper::SYNC_ERROR:
      return SYNC_ERROR;
    case SyncStatusUIHelper::PRE_SYNCED:
    case SyncStatusUIHelper::SYNCED:
    default:
      return HIDE;
  }
}

DOMMessageHandler* NewTabPageSyncHandler::Attach(DOMUI* dom_ui) {
  sync_service_ = dom_ui->GetProfile()->GetProfileSyncService();
  DCHECK(sync_service_);  // This shouldn't get called by an incognito NTP.
  sync_service_->AddObserver(this);
  return DOMMessageHandler::Attach(dom_ui);
}

void NewTabPageSyncHandler::RegisterMessages() {
  dom_ui_->RegisterMessageCallback("GetSyncMessage",
      NewCallback(this, &NewTabPageSyncHandler::HandleGetSyncMessage));
  dom_ui_->RegisterMessageCallback("SyncLinkClicked",
      NewCallback(this, &NewTabPageSyncHandler::HandleSyncLinkClicked));
}

void NewTabPageSyncHandler::HandleGetSyncMessage(const Value* value) {
  waiting_for_initial_page_load_ = false;
  BuildAndSendSyncStatus();
}

void NewTabPageSyncHandler::HideSyncStatusSection() {
  SendSyncMessageToPage(HIDE, std::string(), std::string());
}

void NewTabPageSyncHandler::BuildAndSendSyncStatus() {
  DCHECK(!waiting_for_initial_page_load_);

  // Hide the sync status section if sync is disabled entirely.
  if (!sync_service_) {
    HideSyncStatusSection();
    return;
  }

  // Don't show sync status if setup is not complete.
  if (!sync_service_->HasSyncSetupCompleted()) {
    return;
  }

  // Once sync has been enabled, the supported "sync statuses" for the NNTP
  // from the user's perspective are:
  //
  // "Sync error", when we can't authenticate or establish a connection with
  //               the sync server (appropriate information appended to
  //               message).
  string16 status_msg;
  string16 link_text;
  SyncStatusUIHelper::MessageType type =
      SyncStatusUIHelper::GetLabels(sync_service_, &status_msg, &link_text);
  SendSyncMessageToPage(FromSyncStatusMessageType(type),
                        UTF16ToUTF8(status_msg), UTF16ToUTF8(link_text));
}

void NewTabPageSyncHandler::HandleSyncLinkClicked(const Value* value) {
  DCHECK(!waiting_for_initial_page_load_);
  DCHECK(sync_service_);
  if (sync_service_->HasSyncSetupCompleted()) {
    // User clicked the 'Login again' link to re-authenticate.
    sync_service_->ShowLoginDialog();
  } else {
    // User clicked the 'Start now' link to begin syncing.
    ProfileSyncService::SyncEvent(ProfileSyncService::START_FROM_NTP);
    sync_service_->EnableForUser();
  }
}

void NewTabPageSyncHandler::OnStateChanged() {
  // Don't do anything if the page has not yet loaded.
  if (waiting_for_initial_page_load_)
    return;
  BuildAndSendSyncStatus();
}

void NewTabPageSyncHandler::SendSyncMessageToPage(
    MessageType type, std::string msg,
    std::string linktext) {
  DictionaryValue value;
  std::string msgtype;
  std::wstring user;
  std::string title =
      WideToUTF8(l10n_util::GetString(IDS_SYNC_NTP_SYNC_SECTION_TITLE));
  std::string linkurl;
  switch (type) {
    case HIDE:
      msgtype = "presynced";
      break;
    case SYNC_ERROR:
      title =
          WideToUTF8(
              l10n_util::GetString(IDS_SYNC_NTP_SYNC_SECTION_ERROR_TITLE));
      msgtype = "error";
      break;
    default:
      NOTREACHED();
      break;
  }

  // If there is no message to show, we should hide the sync section
  // altogether.
  if (type == HIDE || msg.empty()) {
    value.SetBoolean(L"syncsectionisvisible", false);
  } else {
    value.SetBoolean(L"syncsectionisvisible", true);
    value.SetString(L"msg", msg);
    value.SetString(L"title", title);
    value.SetString(L"msgtype", msgtype);
    if (linktext.empty()) {
      value.SetBoolean(L"linkisvisible", false);
    } else {
      value.SetBoolean(L"linkisvisible", true);
      value.SetString(L"linktext", linktext);

      // The only time we set the URL is when the user is synced and we need to
      // show a link to a web interface (e.g. http://docs.google.com). When we
      // set that URL, HandleSyncLinkClicked won't be called when the user
      // clicks on the link.
      if (linkurl.empty()) {
        value.SetBoolean(L"linkurlisset", false);
      } else {
        value.SetBoolean(L"linkurlisset", true);
        value.SetString(L"linkurl", linkurl);
      }
    }
  }
  dom_ui_->CallJavascriptFunction(L"syncMessageChanged", value);
}
