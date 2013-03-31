// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/merge_session_load_page.h"

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram.h"
#include "base/stringprintf.h"
#include "base/strings/string_piece.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "net/base/escape.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/webui/jstemplate_builder.h"

using content::BrowserThread;
using content::InterstitialPage;
using content::WebContents;

namespace {

// Delay time for showing interstitial page.
const int kShowDelayTimeMS = 1000;

// Maximum time for showing interstitial page.
const int kTotalWaitTimeMS = 10000;

}  // namespace

namespace chromeos {

MergeSessionLoadPage::MergeSessionLoadPage(WebContents* web_contents,
                                           const GURL& url,
                                           const CompletionCallback& callback)
    : callback_(callback),
      proceeded_(false),
      web_contents_(web_contents),
      url_(url) {
  UserManager::Get()->AddObserver(this);
  interstitial_page_ = InterstitialPage::Create(web_contents, true, url, this);
}

MergeSessionLoadPage::~MergeSessionLoadPage() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  UserManager::Get()->RemoveObserver(this);
}

void MergeSessionLoadPage::Show() {
  interstitial_page_->Show();
}

std::string MergeSessionLoadPage::GetHTMLContents() {
  DictionaryValue strings;
  strings.SetString("title", web_contents_->GetTitle());
  // Set the timeout to show the page.
  strings.SetInteger("show_delay_time", kShowDelayTimeMS);
  strings.SetInteger("total_wait_time", kTotalWaitTimeMS);
  // TODO(zelidrag): Flip the message to IDS_MERGE_SESSION_LOAD_HEADLINE
  // after merge.
  strings.SetString("heading",
                    l10n_util::GetStringUTF16(IDS_TAB_LOADING_TITLE));

  bool rtl = base::i18n::IsRTL();
  strings.SetString("textdirection", rtl ? "rtl" : "ltr");

  base::StringPiece html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_MERGE_SESSION_LOAD_HTML));
  return webui::GetI18nTemplateHtml(html, &strings);
}

void MergeSessionLoadPage::OverrideRendererPrefs(
      content::RendererPreferences* prefs) {
  Profile* profile = Profile::FromBrowserContext(
      web_contents_->GetBrowserContext());
  renderer_preferences_util::UpdateFromSystemSettings(prefs, profile);
}

void MergeSessionLoadPage::OnProceed() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  proceeded_ = true;
  NotifyBlockingPageComplete();
}

void MergeSessionLoadPage::OnDontProceed() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Ignore if it's already proceeded.
  if (proceeded_)
    return;
  NotifyBlockingPageComplete();
}

void MergeSessionLoadPage::CommandReceived(const std::string& cmd) {
  std::string command(cmd);
  // The Jasonified response has quotes, remove them.
  if (command.length() > 1 && command[0] == '"')
    command = command.substr(1, command.length() - 2);

  if (command == "proceed") {
    interstitial_page_->Proceed();
  } else {
    DVLOG(1) << "Unknown command:" << cmd;
  }
}

void MergeSessionLoadPage::NotifyBlockingPageComplete() {
  if (!callback_.is_null()) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE, callback_);
  }
}

void MergeSessionLoadPage::MergeSessionStateChanged(
    UserManager::MergeSessionState state) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DVLOG(1) << "Merge session is "
           << (state !=  UserManager:: MERGE_STATUS_IN_PROCESS ?
                  " NOT " : "")
           << " in progress, "
           << state;
  if (state !=  UserManager:: MERGE_STATUS_IN_PROCESS) {
    UserManager::Get()->RemoveObserver(this);
    interstitial_page_->Proceed();
  }
}

}  // namespace chromeos
