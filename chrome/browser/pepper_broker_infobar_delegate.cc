// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pepper_broker_infobar_delegate.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/infobars/infobar.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/plugins/plugin_finder.h"
#include "chrome/browser/plugins/plugin_metadata.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "content/public/common/webplugininfo.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(GOOGLE_TV)
#include "base/android/context_types.h"
#endif


// static
void PepperBrokerInfoBarDelegate::Create(
    content::WebContents* web_contents,
    const GURL& url,
    const base::FilePath& plugin_path,
    const base::Callback<void(bool)>& callback) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  // TODO(wad): Add ephemeral device ID support for broker in guest mode.
  if (profile->IsGuestSession()) {
    callback.Run(false);
    return;
  }

  TabSpecificContentSettings* tab_content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);

#if defined(OS_CHROMEOS)
  // On ChromeOS, we're ok with granting broker access to the Netflix and
  // Widevine plugins, since they can only come installed with the OS.
  const char kWidevinePluginFileName[] = "libwidevinecdmadapter.so";
  const char kNetflixDomain[] = "netflix.com";

  base::FilePath plugin_file_name = plugin_path.BaseName();
  if (plugin_file_name.value() == FILE_PATH_LITERAL(kWidevinePluginFileName) &&
      url.DomainIs(kNetflixDomain)) {
    tab_content_settings->SetPepperBrokerAllowed(true);
    callback.Run(true);
    return;
  }
#endif

#if defined(GOOGLE_TV)
  // On GoogleTV, Netflix crypto/mdx plugin and DRM server adapter plugin can
  // only come pre-installed with the OS, so we're willing to auto-grant access
  // to them. PluginRootName should be matched with PEPPER_PLUGIN_ROOT
  // in PepperPluginManager.java.
  const char kPluginRootName[] = "/system/lib/pepperplugin";
  const char kNetflixCryptoPluginFileName[] = "libnrddpicrypto.so";
  const char kNetflixMdxPluginFileName[] = "libnrdmdx.so";
  const char kDrmServerAdapterPluginFileName[] = "libdrmserveradapter.so";
  base::FilePath::StringType plugin_dir_name = plugin_path.DirName().value();
  base::FilePath::StringType plugin_file_name = plugin_path.BaseName().value();
  if (base::android::IsRunningInWebapp() &&
      plugin_dir_name == FILE_PATH_LITERAL(kPluginRootName) &&
      (plugin_file_name == FILE_PATH_LITERAL(kNetflixCryptoPluginFileName) ||
       plugin_file_name == FILE_PATH_LITERAL(kNetflixMdxPluginFileName) ||
       plugin_file_name ==
          FILE_PATH_LITERAL(kDrmServerAdapterPluginFileName))) {
    tab_content_settings->SetPepperBrokerAllowed(true);
    callback.Run(true);
    return;
  }
#endif

  HostContentSettingsMap* content_settings =
      profile->GetHostContentSettingsMap();
  ContentSetting setting =
      content_settings->GetContentSetting(url, url,
                                          CONTENT_SETTINGS_TYPE_PPAPI_BROKER,
                                          std::string());

  if (setting == CONTENT_SETTING_ASK) {
    content::RecordAction(
        content::UserMetricsAction("PPAPI.BrokerInfobarDisplayed"));
    InfoBarService* infobar_service =
        InfoBarService::FromWebContents(web_contents);
    infobar_service->AddInfoBar(ConfirmInfoBarDelegate::CreateInfoBar(
        scoped_ptr<ConfirmInfoBarDelegate>(new PepperBrokerInfoBarDelegate(
            url, plugin_path,
            profile->GetPrefs()->GetString(prefs::kAcceptLanguages),
            content_settings, tab_content_settings, callback))));
    return;
  }

  bool allowed = (setting == CONTENT_SETTING_ALLOW);
  content::RecordAction(allowed ?
      content::UserMetricsAction("PPAPI.BrokerSettingAllow") :
      content::UserMetricsAction("PPAPI.BrokerSettingDeny"));
  tab_content_settings->SetPepperBrokerAllowed(allowed);
  callback.Run(allowed);
}

PepperBrokerInfoBarDelegate::PepperBrokerInfoBarDelegate(
    const GURL& url,
    const base::FilePath& plugin_path,
    const std::string& languages,
    HostContentSettingsMap* content_settings,
    TabSpecificContentSettings* tab_content_settings,
    const base::Callback<void(bool)>& callback)
    : ConfirmInfoBarDelegate(),
      url_(url),
      plugin_path_(plugin_path),
      languages_(languages),
      content_settings_(content_settings),
      tab_content_settings_(tab_content_settings),
      callback_(callback) {
}

PepperBrokerInfoBarDelegate::~PepperBrokerInfoBarDelegate() {
  if (!callback_.is_null())
    callback_.Run(false);
}

int PepperBrokerInfoBarDelegate::GetIconID() const {
  return IDR_INFOBAR_PLUGIN_INSTALL;
}

base::string16 PepperBrokerInfoBarDelegate::GetMessageText() const {
  content::PluginService* plugin_service =
      content::PluginService::GetInstance();
  content::WebPluginInfo plugin;
  bool success = plugin_service->GetPluginInfoByPath(plugin_path_, &plugin);
  DCHECK(success);
  scoped_ptr<PluginMetadata> plugin_metadata(
      PluginFinder::GetInstance()->GetPluginMetadata(plugin));
  return l10n_util::GetStringFUTF16(IDS_PEPPER_BROKER_MESSAGE,
                                    plugin_metadata->name(),
                                    net::FormatUrl(url_.GetOrigin(),
                                                   languages_));
}

base::string16 PepperBrokerInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_PEPPER_BROKER_ALLOW_BUTTON : IDS_PEPPER_BROKER_DENY_BUTTON);
}

bool PepperBrokerInfoBarDelegate::Accept() {
  DispatchCallback(true);
  return true;
}

bool PepperBrokerInfoBarDelegate::Cancel() {
  DispatchCallback(false);
  return true;
}

base::string16 PepperBrokerInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

bool PepperBrokerInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  GURL learn_more_url("https://support.google.com/chrome/?p=ib_pepper_broker");
  web_contents()->OpenURL(content::OpenURLParams(
      learn_more_url, content::Referrer(),
      (disposition == CURRENT_TAB) ? NEW_FOREGROUND_TAB : disposition,
      content::PAGE_TRANSITION_LINK, false));
  return false;
}

void PepperBrokerInfoBarDelegate::DispatchCallback(bool result) {
  content::RecordAction(result ?
      content::UserMetricsAction("PPAPI.BrokerInfobarClickedAllow") :
      content::UserMetricsAction("PPAPI.BrokerInfobarClickedDeny"));
  callback_.Run(result);
  callback_ = base::Callback<void(bool)>();
  content_settings_->SetContentSetting(
      ContentSettingsPattern::FromURLNoWildcard(url_),
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_PPAPI_BROKER,
      std::string(), result ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK);
  tab_content_settings_->SetPepperBrokerAllowed(result);
}
