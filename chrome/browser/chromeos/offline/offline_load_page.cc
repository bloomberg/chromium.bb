// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/offline/offline_load_page.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram.h"
#include "base/string_piece.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/notification_type.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"

namespace {

// Maximum time to show a blank page.
const int kMaxBlankPeriod = 3000;

// A utility function to set the dictionary's value given by |resource_id|.
void SetString(DictionaryValue* strings, const char* name, int resource_id) {
  strings->SetString(name, l10n_util::GetStringUTF16(resource_id));
}

}  // namespace

namespace chromeos {

// static
void OfflineLoadPage::Show(int process_host_id, int render_view_id,
                           const GURL& url, Delegate* delegate) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (NetworkStateNotifier::is_connected()) {
    // Check again in UI thread and proceed if it's connected.
    delegate->OnBlockingPageComplete(true);
  } else {
    TabContents* tab_contents =
        tab_util::GetTabContentsByID(process_host_id, render_view_id);
    DCHECK(tab_contents);
    (new OfflineLoadPage(tab_contents, url, delegate))->Show();
  }
}

OfflineLoadPage::OfflineLoadPage(TabContents* tab_contents,
                                 const GURL& url,
                                 Delegate* delegate)
    : InterstitialPage(tab_contents, true, url),
      delegate_(delegate) {
  registrar_.Add(this, NotificationType::NETWORK_STATE_CHANGED,
                 NotificationService::AllSources());
}

std::string OfflineLoadPage::GetHTMLContents() {
  DictionaryValue strings;
  SetString(&strings, "headLine", IDS_OFFLINE_LOAD_HEADLINE);
  SetString(&strings, "description", IDS_OFFLINE_LOAD_DESCRIPTION);
  SetString(&strings, "load_button", IDS_OFFLINE_LOAD_BUTTON);
  SetString(&strings, "back_button", IDS_OFFLINE_BACK_BUTTON);

  // TODO(oshima): tab()->GetTitle() always return url. This has to be
  // a cached title.
  strings.SetString("title", tab()->GetTitle());
  strings.SetString("textdirection", base::i18n::IsRTL() ? "rtl" : "ltr");
  strings.SetString("display_go_back",
                    tab()->controller().CanGoBack() ? "inline" : "none");
  int64 time_to_wait = std::max(
      static_cast<int64>(0),
      kMaxBlankPeriod -
          NetworkStateNotifier::GetOfflineDuration().InMilliseconds());
  strings.SetString("on_load",
                    WideToUTF16Hack(base::StringPrintf(L"startTimer(%ld)",
                                                       time_to_wait)));

  // TODO(oshima): thumbnail is not working yet. fix this.
  const std::string url = "chrome://thumb/" + GetURL().spec();
  strings.SetString("thumbnailUrl", "url(" + url + ")");

  base::StringPiece html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_OFFLINE_LOAD_HTML));
  return jstemplate_builder::GetI18nTemplateHtml(html, &strings);
}

void OfflineLoadPage::CommandReceived(const std::string& cmd) {
  std::string command(cmd);
  // The Jasonified response has quotes, remove them.
  if (command.length() > 1 && command[0] == '"') {
    command = command.substr(1, command.length() - 2);
  }
  // TODO(oshima): record action for metrics.
  if (command == "proceed") {
    Proceed();
  } else if (command == "dontproceed") {
    DontProceed();
  } else {
    LOG(WARNING) << "Unknown command:" << cmd;
  }
}

void OfflineLoadPage::Proceed() {
  delegate_->OnBlockingPageComplete(true);
  InterstitialPage::Proceed();
}

void OfflineLoadPage::DontProceed() {
  delegate_->OnBlockingPageComplete(false);
  InterstitialPage::DontProceed();
}

void OfflineLoadPage::Observe(NotificationType type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  if (type.value == NotificationType::NETWORK_STATE_CHANGED) {
    chromeos::NetworkStateDetails* state_details =
        Details<chromeos::NetworkStateDetails>(details).ptr();
    DVLOG(1) << "NetworkStateChanaged notification received: state="
             << state_details->state();
    if (state_details->state() ==
        chromeos::NetworkStateDetails::CONNECTED) {
      registrar_.Remove(this, NotificationType::NETWORK_STATE_CHANGED,
                        NotificationService::AllSources());
      Proceed();
    }
  } else {
    InterstitialPage::Observe(type, source, details);
  }
}

}  // namespace chromeos
