// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/instant_ntp_prerenderer.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/content_settings_provider.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/search/instant_ntp.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/search_urls.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/network_change_notifier.h"

namespace {

void DeleteNTPSoon(scoped_ptr<InstantNTP> ntp) {
  if (!ntp)
    return;

  if (ntp->contents()) {
    base::MessageLoop::current()->DeleteSoon(
        FROM_HERE, ntp->ReleaseContents().release());
  }
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, ntp.release());
}

}  // namespace


InstantNTPPrerenderer::InstantNTPPrerenderer(Profile* profile,
                                             InstantService* instant_service,
                                             PrefService* prefs)
    : profile_(profile) {
  DCHECK(profile);

  // In unit tests, prefs may be NULL.
  if (prefs) {
    profile_pref_registrar_.Init(prefs);
    profile_pref_registrar_.Add(
        prefs::kSearchSuggestEnabled,
        base::Bind(&InstantNTPPrerenderer::ReloadInstantNTP,
                   base::Unretained(this)));
  }
  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);

  // Allow instant_service to be null for unit tets.
  if (instant_service)
    instant_service->AddObserver(this);
}

InstantNTPPrerenderer::~InstantNTPPrerenderer() {
  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(profile_);
  if (instant_service)
    instant_service->RemoveObserver(this);
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
}

void InstantNTPPrerenderer::ReloadInstantNTP() {
  ResetNTP(GetInstantURL());
}

scoped_ptr<content::WebContents> InstantNTPPrerenderer::ReleaseNTPContents() {
  if (!profile_ || profile_->IsOffTheRecord() ||
      !chrome::ShouldShowInstantNTP())
    return scoped_ptr<content::WebContents>();

  if (ShouldSwitchToLocalNTP())
    ResetNTP(GetLocalInstantURL());

  scoped_ptr<content::WebContents> ntp_contents = ntp_->ReleaseContents();

  // Preload a new InstantNTP.
  ReloadInstantNTP();
  return ntp_contents.Pass();
}

content::WebContents* InstantNTPPrerenderer::GetNTPContents() const {
  return ntp() ? ntp()->contents() : NULL;
}

void InstantNTPPrerenderer::DeleteNTPContents() {
  if (ntp_)
    ntp_.reset();
}

void InstantNTPPrerenderer::RenderProcessGone() {
  DeleteNTPSoon(ntp_.Pass());
}

void InstantNTPPrerenderer::LoadCompletedMainFrame() {
  if (!ntp_ || ntp_->supports_instant())
    return;

  content::WebContents* ntp_contents = ntp_->contents();
  DCHECK(ntp_contents);

  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(profile());
  if (instant_service &&
      instant_service->IsInstantProcess(
          ntp_contents->GetRenderProcessHost()->GetID())) {
    return;
  }
  InstantSupportDetermined(ntp_contents, false);
}

std::string InstantNTPPrerenderer::GetLocalInstantURL() const {
  return chrome::GetLocalInstantURL(profile_).spec();
}

std::string InstantNTPPrerenderer::GetInstantURL() const {
  if (net::NetworkChangeNotifier::IsOffline())
    return GetLocalInstantURL();

  // TODO(kmadhusu): Remove start margin param from chrome::GetInstantURL().
  const GURL instant_url = chrome::GetInstantURL(profile_,
                                                 chrome::kDisableStartMargin);
  if (!instant_url.is_valid())
    return GetLocalInstantURL();

  return instant_url.spec();
}

bool InstantNTPPrerenderer::IsJavascriptEnabled() const {
  GURL instant_url(GetInstantURL());
  GURL origin(instant_url.GetOrigin());
  ContentSetting js_setting = profile_->GetHostContentSettingsMap()->
      GetContentSetting(origin, origin, CONTENT_SETTINGS_TYPE_JAVASCRIPT,
                        NO_RESOURCE_IDENTIFIER);
  // Javascript can be disabled either in content settings or via a WebKit
  // preference, so check both. Disabling it through the Settings page affects
  // content settings. I'm not sure how to disable the WebKit preference, but
  // it's theoretically possible some users have it off.
  bool js_content_enabled =
      js_setting == CONTENT_SETTING_DEFAULT ||
      js_setting == CONTENT_SETTING_ALLOW;
  bool js_webkit_enabled = profile_->GetPrefs()->GetBoolean(
      prefs::kWebKitJavascriptEnabled);
  return js_content_enabled && js_webkit_enabled;
}

bool InstantNTPPrerenderer::InStartup() const {
#if !defined(OS_ANDROID)
  // TODO(kmadhusu): This is not completely reliable. Find a better way to
  // detect startup time.
  Browser* browser = chrome::FindBrowserWithProfile(profile_,
                                                    chrome::GetActiveDesktop());
  return !browser || !browser->tab_strip_model()->GetActiveWebContents();
#endif
  return false;
}

InstantNTP* InstantNTPPrerenderer::ntp() const {
  return ntp_.get();
}

void InstantNTPPrerenderer::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  // Not interested in events conveying change to offline.
  if (type == net::NetworkChangeNotifier::CONNECTION_NONE)
    return;

  if (!ntp() || ntp()->IsLocal())
    ReloadInstantNTP();
}

void InstantNTPPrerenderer::InstantSupportDetermined(
    const content::WebContents* contents,
    bool supports_instant) {
  DCHECK(ntp() && ntp()->contents() == contents);

  if (!supports_instant) {
    bool is_local = ntp()->IsLocal();
    DeleteNTPSoon(ntp_.Pass());
    if (!is_local)
      ResetNTP(GetLocalInstantURL());
  }

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_INSTANT_NTP_SUPPORT_DETERMINED,
      content::Source<InstantNTPPrerenderer>(this),
      content::NotificationService::NoDetails());
}

void InstantNTPPrerenderer::InstantPageAboutToNavigateMainFrame(
    const content::WebContents* /* contents */,
    const GURL& /* url */) {
  NOTREACHED();
}

void InstantNTPPrerenderer::NavigateToURL(
    const content::WebContents* /* contents */,
    const GURL& /* url */,
    content::PageTransition /* transition */,
    WindowOpenDisposition /* disposition */,
    bool /* is_search_type */) {
  NOTREACHED();
}

void InstantNTPPrerenderer::PasteIntoOmnibox(
    const content::WebContents* /* contents */,
    const string16& /* text */) {
  NOTREACHED();
}

void InstantNTPPrerenderer::InstantPageLoadFailed(
    content::WebContents* contents) {
  DCHECK(ntp() && ntp()->contents() == contents);

  bool is_local = ntp()->IsLocal();
  DeleteNTPSoon(ntp_.Pass());
  if (!is_local)
    ResetNTP(GetLocalInstantURL());
}

void InstantNTPPrerenderer::ResetNTP(const std::string& instant_url) {
  // Instant NTP is only used in extended mode so we should always have a
  // non-empty URL to use.
  DCHECK(!instant_url.empty());
  if (!chrome::ShouldUseCacheableNTP()) {
    ntp_.reset(new InstantNTP(this, instant_url, profile_));
    ntp_->InitContents(base::Bind(&InstantNTPPrerenderer::ReloadInstantNTP,
                                  base::Unretained(this)));
  }
}

bool InstantNTPPrerenderer::PageIsCurrent() const {
  const std::string& instant_url = GetInstantURL();
  if (instant_url.empty() ||
      !search::MatchesOriginAndPath(GURL(ntp()->instant_url()),
                                    GURL(instant_url)))
    return false;

  return ntp()->supports_instant();
}

bool InstantNTPPrerenderer::ShouldSwitchToLocalNTP() const {
  if (!ntp())
    return true;

  // Assume users with Javascript disabled do not want the online experience.
  if (!IsJavascriptEnabled())
    return true;

  // Already a local page. Not calling IsLocal() because we want to distinguish
  // between the Google-specific and generic local NTP.
  if (ntp()->instant_url() == GetLocalInstantURL())
    return false;

  if (PageIsCurrent())
    return false;

  // The preloaded NTP does not support instant yet. If we're not in startup,
  // always fall back to the local NTP. If we are in startup, use the local NTP
  // (unless the finch flag to use the remote NTP is set).
  return !(InStartup() && chrome::ShouldPreferRemoteNTPOnStartup());
}

void InstantNTPPrerenderer::DefaultSearchProviderChanged() {
  ReloadInstantNTP();
}

void InstantNTPPrerenderer::GoogleURLUpdated() {
  ReloadInstantNTP();
}
