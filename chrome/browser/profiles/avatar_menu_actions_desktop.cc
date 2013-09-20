// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/avatar_menu_actions_desktop.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/site_instance.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"

namespace {

class SignoutTracker : public content::WebContentsObserver {
 public:
  SignoutTracker(Profile* profile, const GURL& signout_landing_url,
                 content::WebContents* contents);

  virtual void WebContentsDestroyed(content::WebContents* contents) OVERRIDE;
  virtual void DidStopLoading(content::RenderViewHost* render_view_host)
      OVERRIDE;

 private:
  scoped_ptr<content::WebContents> contents_;
  GURL signout_landing_url_;
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(SignoutTracker);
};

SignoutTracker::SignoutTracker(Profile* profile,
                               const GURL& signout_landing_url,
                               content::WebContents* contents)
  : WebContentsObserver(contents),
    contents_(contents),
    signout_landing_url_(signout_landing_url),
    profile_(profile) {
}

void SignoutTracker::DidStopLoading(content::RenderViewHost* render_view_host) {
  // Only close when we reach the final landing; ignore redirects until then.
  if (web_contents()->GetURL() == signout_landing_url_) {
    Observe(NULL);
    BrowserList::CloseAllBrowsersWithProfile(profile_);
    delete this;  /* success */
  }
}

void SignoutTracker::WebContentsDestroyed(content::WebContents* contents) {
  delete this;  /* failure */
}

}  // namespace

AvatarMenuActionsDesktop::AvatarMenuActionsDesktop() {
}

AvatarMenuActionsDesktop::~AvatarMenuActionsDesktop() {
}

// static
AvatarMenuActions* AvatarMenuActions::Create() {
  return new AvatarMenuActionsDesktop();
}

void AvatarMenuActionsDesktop::AddNewProfile(ProfileMetrics::ProfileAdd type) {
  // TODO: Remove dependency on Browser by delegating AddNewProfile and
  // and EditProfile actions.

  Browser* settings_browser = browser_;
  if (!settings_browser) {
    const Browser::CreateParams params(ProfileManager::GetLastUsedProfile(),
                                       chrome::GetActiveDesktop());
    settings_browser = new Browser(params);
  }
  chrome::ShowSettingsSubPage(settings_browser, chrome::kCreateProfileSubPage);
  ProfileMetrics::LogProfileAddNewUser(type);
}

void AvatarMenuActionsDesktop::EditProfile(Profile* profile, size_t index) {
  Browser* settings_browser = browser_;
  if (!settings_browser) {
    settings_browser = new Browser(
        Browser::CreateParams(profile, chrome::GetActiveDesktop()));
  }
  std::string page = chrome::kManageProfileSubPage;
  page += "#";
  page += base::IntToString(static_cast<int>(index));
  chrome::ShowSettingsSubPage(settings_browser, page);
}

bool AvatarMenuActionsDesktop::ShouldShowAddNewProfileLink() const {
  // |browser_| can be NULL in unit_tests.
  return !browser_ || !browser_->profile()->IsManaged();
}

bool AvatarMenuActionsDesktop::ShouldShowEditProfileLink() const {
  return true;
}

content::WebContents* AvatarMenuActionsDesktop::BeginSignOut() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* current_profile = browser_->profile();

  ProfileInfoCache& cache = profile_manager->GetProfileInfoCache();
  size_t index = cache.GetIndexOfProfileWithPath(current_profile->GetPath());
  cache.SetProfileSigninRequiredAtIndex(index, true);

  std::string landing_url = signin::GetLandingURL("close", 1).spec();
  GURL logout_url(GaiaUrls::GetInstance()->service_logout_url());
  logout_url = net::AppendQueryParameter(logout_url, "?continue=", landing_url);
  if (!logout_override_.empty()) {
    // We're testing...
    landing_url = logout_override_;
    logout_url = GURL(logout_override_);
  }

  content::WebContents::CreateParams create_params(current_profile);
  create_params.site_instance =
      content::SiteInstance::CreateForURL(current_profile, logout_url);
  content::WebContents* contents = content::WebContents::Create(create_params);
  contents->GetController().LoadURL(
    logout_url, content::Referrer(),
    content::PAGE_TRANSITION_GENERATED, std::string());

  // This object may be destructed when the menu closes but we need something
  // around to finish the sign-out process and close the profile windows.
  new SignoutTracker(current_profile, GURL(landing_url), contents);

  return contents;  // returned for testing purposes
}

void AvatarMenuActionsDesktop::SetLogoutURL(const std::string& logout_url) {
  logout_override_ = logout_url;
}

void AvatarMenuActionsDesktop::ActiveBrowserChanged(Browser* browser) {
  browser_ = browser;
}
