// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef CHROME_PERSONALIZATION

#include "chrome/browser/dom_ui/new_tab_page_sync_handler.h"

#include "base/json_writer.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/sync/personalization.h"
#include "chrome/browser/sync/personalization_strings.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "grit/browser_resources.h"
#include "net/base/cookie_monster.h"
#include "net/url_request/url_request_context.h"

// XPath expression for finding our p13n iframe.
static const wchar_t* kP13nIframeXpath = L"//iframe[@id='p13n']";

namespace Personalization {

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
    URLRequestContext* context = Profile::GetDefaultRequestContext();
    net::CookieStore* store = context->cookie_store();
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

}  // namespace Personalization

NewTabPageSyncHandler::NewTabPageSyncHandler() : sync_service_(NULL),
    waiting_for_initial_page_load_(true) {
}

NewTabPageSyncHandler::~NewTabPageSyncHandler() {
  sync_service_->RemoveObserver(this);
}

DOMMessageHandler* NewTabPageSyncHandler::Attach(DOMUI* dom_ui) {
  Profile* p = dom_ui->GetProfile();
  sync_service_ = p->GetProfilePersonalization()->sync_service();
  DCHECK(sync_service_);
  sync_service_->AddObserver(this);
  return DOMMessageHandler::Attach(dom_ui);
}

void NewTabPageSyncHandler::RegisterMessages() {
  dom_ui_->RegisterMessageCallback("GetSyncMessage",
      NewCallback(this, &NewTabPageSyncHandler::HandleGetSyncMessage));
  dom_ui_->RegisterMessageCallback("SyncLinkClicked",
      NewCallback(this, &NewTabPageSyncHandler::HandleSyncLinkClicked));
  dom_ui_->RegisterMessageCallback("ResizeP13N",
      NewCallback(this, &NewTabPageSyncHandler::HandleResizeP13N));
}

void NewTabPageSyncHandler::HandleResizeP13N(const Value* value) {
  // We just want to call back in to the new tab page on behalf of our
  // same-origin-policy crippled iframe, to tell it to resize the container.
  dom_ui_->CallJavascriptFunction(L"resizeP13N", *value);
}

void NewTabPageSyncHandler::BuildAndSendSyncStatus() {
  DCHECK(!waiting_for_initial_page_load_);

  if (!sync_service_->IsSyncEnabledByUser() &&
      !sync_service_->SetupInProgress()) {
    // Clear the page status, without showing the promotion or sync ui.
    // TODO(timsteele): This is fine, but if the page is refreshed or another
    // NTP is opened, we could end up showing the promo again. Not sure this is
    // desired if the user already signed up once and disabled.
    FundamentalValue value(0);
    dom_ui_->CallJavascriptFunction(L"resizeP13N", value);
    return;
  }

  // There are currently three supported "sync statuses" for the NTP, from
  // the users perspective:
  // "Synced to foo@gmail.com", when we are successfully authenticated and
  //                            connected to a sync server.
  // "Sync error", when we can't authenticate or establish a connection with
  //               the sync server (appropriate information appended to
  //               message).
  // "Authenticating", when credentials are in flight.
  SyncStatusUIHelper::MessageType type(SyncStatusUIHelper::PRE_SYNCED);
  std::wstring status_msg;
  std::wstring link_text;
  type = SyncStatusUIHelper::GetLabels(sync_service_, &status_msg, &link_text);
  SendSyncMessageToPage(type, WideToUTF8(status_msg), WideToUTF8(link_text));
}

void NewTabPageSyncHandler::HandleGetSyncMessage(const Value* value) {
    waiting_for_initial_page_load_ = false;

  if (!sync_service_->IsSyncEnabledByUser() &&
      !sync_service_->SetupInProgress()) {
    if (Personalization::IsGoogleGAIACookieInstalled()) {
      // Sync has not been enabled, and the user has logged in to GAIA.
      SendSyncMessageToPage(SyncStatusUIHelper::PRE_SYNCED, kSyncPromotionMsg,
                            kStartNowLinkText);
    }
    return;
  }

  BuildAndSendSyncStatus();
}

void NewTabPageSyncHandler::HandleSyncLinkClicked(const Value* value) {
  DCHECK(!waiting_for_initial_page_load_);
  if (sync_service_->IsSyncEnabledByUser()) {
    // User clicked 'Login again' link to re-authenticate.
    sync_service_->ShowLoginDialog();
  } else {
    // User clicked "Start now" link to begin syncing.
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
    SyncStatusUIHelper::MessageType type, std::string msg,
    const std::string& linktext) {
  DictionaryValue value;
  std::string title, msgtype;
  switch (type) {
    case SyncStatusUIHelper::PRE_SYNCED:
      title = kSyncSectionTitle;
      msgtype = "presynced";
      break;
    case SyncStatusUIHelper::SYNCED:
      msgtype = "synced";
      msg = msg.substr(0, msg.find(WideToUTF8(kLastSyncedLabel)));
      break;
    case SyncStatusUIHelper::SYNC_ERROR:
      title = kSyncErrorSectionTitle;
      msgtype = "error";
      break;
  }

  value.SetString(L"title", title);
  value.SetString(L"msg", msg);
  value.SetString(L"msgtype", msgtype);
  if (!linktext.empty())
    value.SetString(L"linktext", linktext);
  else
    value.SetBoolean(L"linktext", false);

  std::string json;
  JSONWriter::Write(&value, false, &json);
  std::wstring javascript = std::wstring(L"renderSyncMessage") +
      L"(" + UTF8ToWide(json) + L");";
  RenderViewHost* rvh = dom_ui_->tab_contents()->render_view_host();
  rvh->ExecuteJavascriptInWebFrame(kP13nIframeXpath, javascript);
}

#endif  // CHROME_PERSONALIZATION
