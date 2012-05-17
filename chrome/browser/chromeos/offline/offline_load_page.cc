// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/offline/offline_load_page.h"

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram.h"
#include "base/string_piece.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "net/base/escape.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"

using content::BrowserThread;
using content::InterstitialPage;
using content::WebContents;

namespace {

// Maximum time to show a blank page.
const int kMaxBlankPeriod = 3000;

// A utility function to set the dictionary's value given by |resource_id|.
void SetString(DictionaryValue* strings, const char* name, int resource_id) {
  strings->SetString(name, l10n_util::GetStringUTF16(resource_id));
}

}  // namespace

namespace chromeos {

OfflineLoadPage::OfflineLoadPage(WebContents* web_contents,
                                 const GURL& url,
                                 const CompletionCallback& callback)
    : callback_(callback),
      proceeded_(false),
      web_contents_(web_contents),
      url_(url) {
  net::NetworkChangeNotifier::AddOnlineStateObserver(this);
  interstitial_page_ = InterstitialPage::Create(web_contents, true, url, this);
}

OfflineLoadPage::~OfflineLoadPage() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  net::NetworkChangeNotifier::RemoveOnlineStateObserver(this);
}

void OfflineLoadPage::Show() {
  interstitial_page_->Show();
}

std::string OfflineLoadPage::GetHTMLContents() {
  DictionaryValue strings;
  int64 time_to_wait = kMaxBlankPeriod;
  // Set the timeout to show the page.
  strings.SetInteger("time_to_wait", static_cast<int>(time_to_wait));
  // Button labels
  SetString(&strings, "heading", IDS_OFFLINE_LOAD_HEADLINE);
  SetString(&strings, "try_loading", IDS_OFFLINE_TRY_LOADING);
  SetString(&strings, "network_settings", IDS_OFFLINE_NETWORK_SETTINGS);

  // Activation
  strings.SetBoolean("show_activation", ShowActivationMessage());

  bool rtl = base::i18n::IsRTL();
  strings.SetString("textdirection", rtl ? "rtl" : "ltr");

  string16 failed_url(ASCIIToUTF16(url_.spec()));
  if (rtl)
    base::i18n::WrapStringWithLTRFormatting(&failed_url);
  strings.SetString("url", failed_url);

  // The offline page for app has icons and slightly different message.
  Profile* profile = Profile::FromBrowserContext(
      web_contents_->GetBrowserContext());
  DCHECK(profile);
  const Extension* extension = NULL;
  ExtensionService* extensions_service = profile->GetExtensionService();
  // Extension service does not exist in test.
  if (extensions_service)
    extension = extensions_service->extensions()->GetHostedAppByURL(
        ExtensionURLInfo(url_));

  if (extension)
    GetAppOfflineStrings(extension, failed_url, &strings);
  else
    GetNormalOfflineStrings(failed_url, &strings);

  base::StringPiece html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_OFFLINE_LOAD_HTML, ui::SCALE_FACTOR_NONE));
  return jstemplate_builder::GetI18nTemplateHtml(html, &strings);
}

 void OfflineLoadPage::OverrideRendererPrefs(
      content::RendererPreferences* prefs) {
  Profile* profile = Profile::FromBrowserContext(
      web_contents_->GetBrowserContext());
  renderer_preferences_util::UpdateFromSystemSettings(prefs, profile);
 }

void OfflineLoadPage::OnProceed() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  proceeded_ = true;
  NotifyBlockingPageComplete(true);
}

void OfflineLoadPage::OnDontProceed() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Ignore if it's already proceeded.
  if (proceeded_)
    return;
  NotifyBlockingPageComplete(false);
}

void OfflineLoadPage::GetAppOfflineStrings(
    const Extension* app,
    const string16& failed_url,
    DictionaryValue* strings) const {
  strings->SetString("title", app->name());

  GURL icon_url = app->GetIconURL(ExtensionIconSet::EXTENSION_ICON_LARGE,
                                  ExtensionIconSet::MATCH_BIGGER);
  if (icon_url.is_empty()) {
    strings->SetString("display_icon", "none");
    strings->SetString("icon", string16());
  } else {
    // Default icon is not accessible from interstitial page.
    // TODO(oshima): Figure out how to use default icon.
    strings->SetString("display_icon", "block");
    strings->SetString("icon", icon_url.spec());
  }

  strings->SetString(
      "msg",
      l10n_util::GetStringFUTF16(IDS_APP_OFFLINE_LOAD_DESCRIPTION,
                                 net::EscapeForHTML(failed_url)));
}

void OfflineLoadPage::GetNormalOfflineStrings(
    const string16& failed_url, DictionaryValue* strings) const {
  strings->SetString("title", web_contents_->GetTitle());

  // No icon for normal web site.
  strings->SetString("display_icon", "none");
  strings->SetString("icon", string16());

  strings->SetString(
      "msg",
      l10n_util::GetStringFUTF16(IDS_SITE_OFFLINE_LOAD_DESCRIPTION,
                                 net::EscapeForHTML(failed_url)));
}

void OfflineLoadPage::CommandReceived(const std::string& cmd) {
  std::string command(cmd);
  // The Jasonified response has quotes, remove them.
  if (command.length() > 1 && command[0] == '"') {
    command = command.substr(1, command.length() - 2);
  }
  // TODO(oshima): record action for metrics.
  if (command == "proceed") {
    interstitial_page_->Proceed();
  } else if (command == "dontproceed") {
    interstitial_page_->DontProceed();
  } else if (command == "open_network_settings") {
    Browser* browser = BrowserList::GetLastActive();
    DCHECK(browser);
    browser->ShowOptionsTab(chrome::kInternetOptionsSubPage);
  } else if (command == "open_activate_broadband") {
    ash::Shell::GetInstance()->delegate()->OpenMobileSetup();
  } else {
    LOG(WARNING) << "Unknown command:" << cmd;
  }
}

void OfflineLoadPage::NotifyBlockingPageComplete(bool proceed) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE, base::Bind(callback_, proceed));
}

void OfflineLoadPage::OnOnlineStateChanged(bool online) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DVLOG(1) << "OnlineStateObserver notification received: state="
           << (online ? "online" : "offline");
  if (online) {
    net::NetworkChangeNotifier::RemoveOnlineStateObserver(this);
    interstitial_page_->Proceed();
  }
}

bool OfflineLoadPage::ShowActivationMessage() {
  CrosLibrary* cros = CrosLibrary::Get();
  if (!cros || !cros->GetNetworkLibrary()->cellular_available())
    return false;

  const CellularNetworkVector& cell_networks =
      cros->GetNetworkLibrary()->cellular_networks();
  for (size_t i = 0; i < cell_networks.size(); ++i) {
    chromeos::ActivationState activation_state =
        cell_networks[i]->activation_state();
    if (activation_state == ACTIVATION_STATE_ACTIVATED)
      return false;
  }
  return true;
}

}  // namespace chromeos
