// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/avatar_menu_model.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/avatar_menu_model_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_info_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/ash/chrome_shell_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/site_instance.h"
#include "google_apis/gaia/gaia_urls.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(ENABLE_MANAGED_USERS)
#include "chrome/browser/managed_mode/managed_user_service.h"
#include "chrome/browser/managed_mode/managed_user_service_factory.h"
#endif

using content::BrowserThread;

namespace {

void OnProfileCreated(bool always_create,
                      chrome::HostDesktopType desktop_type,
                      Profile* profile,
                      Profile::CreateStatus status) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (status == Profile::CREATE_STATUS_INITIALIZED) {
    profiles::FindOrCreateNewWindowForProfile(
        profile,
        chrome::startup::IS_NOT_PROCESS_STARTUP,
        chrome::startup::IS_NOT_FIRST_RUN,
        desktop_type,
        always_create);
  }
}

// Constants for the show profile switcher experiment
const char kShowProfileSwitcherFieldTrialName[] = "ShowProfileSwitcher";
const char kAlwaysShowSwitcherGroupName[] = "AlwaysShow";


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

AvatarMenuModel::AvatarMenuModel(ProfileInfoInterface* profile_cache,
                                 AvatarMenuModelObserver* observer,
                                 Browser* browser)
    : profile_info_(profile_cache),
      observer_(observer),
      browser_(browser) {
  DCHECK(profile_info_);
  // Don't DCHECK(browser_) so that unit tests can reuse this ctor.

  // Register this as an observer of the info cache.
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_CACHED_INFO_CHANGED,
      content::NotificationService::AllSources());

  // Build the initial menu.
  RebuildMenu();
}

AvatarMenuModel::~AvatarMenuModel() {
  ClearMenu();
}

AvatarMenuModel::Item::Item(size_t model_index, const gfx::Image& icon)
    : icon(icon),
      active(false),
      signed_in(false),
      signin_required(false),
      model_index(model_index) {
}

AvatarMenuModel::Item::~Item() {
}

void AvatarMenuModel::SwitchToProfile(size_t index, bool always_create) {
  DCHECK(profiles::IsMultipleProfilesEnabled() ||
         index == GetActiveProfileIndex());
  const Item& item = GetItemAt(index);
  base::FilePath path =
      profile_info_->GetPathOfProfileAtIndex(item.model_index);

  chrome::HostDesktopType desktop_type = chrome::GetActiveDesktop();
  if (browser_)
    desktop_type = browser_->host_desktop_type();

  profiles::SwitchToProfile(path, desktop_type, always_create);
  ProfileMetrics::LogProfileSwitchUser(ProfileMetrics::SWITCH_PROFILE_ICON);
}

void AvatarMenuModel::EditProfile(size_t index) {
  Browser* browser = browser_;
  if (!browser) {
    Profile* profile = g_browser_process->profile_manager()->GetProfileByPath(
        profile_info_->GetPathOfProfileAtIndex(GetItemAt(index).model_index));
    browser = new Browser(Browser::CreateParams(profile,
                                                chrome::GetActiveDesktop()));
  }
  std::string page = chrome::kManageProfileSubPage;
  page += "#";
  page += base::IntToString(static_cast<int>(index));
  chrome::ShowSettingsSubPage(browser, page);
}

void AvatarMenuModel::AddNewProfile(ProfileMetrics::ProfileAdd type) {
  Browser* browser = browser_;
  if (!browser) {
    const Browser::CreateParams params(ProfileManager::GetLastUsedProfile(),
                                       chrome::GetActiveDesktop());
    browser = new Browser(params);
  }
  chrome::ShowSettingsSubPage(browser, chrome::kCreateProfileSubPage);
  ProfileMetrics::LogProfileAddNewUser(type);
}

base::FilePath AvatarMenuModel::GetProfilePath(size_t index) {
  const Item& item = GetItemAt(index);
  return profile_info_->GetPathOfProfileAtIndex(item.model_index);
}

// static
void AvatarMenuModel::SwitchToGuestProfileWindow(Browser* browser) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  profile_manager->CreateProfileAsync(ProfileManager::GetGuestProfilePath(),
                                      base::Bind(&OnProfileCreated,
                                                 false,
                                                 browser->host_desktop_type()),
                                      string16(),
                                      string16(),
                                      std::string());
}

size_t AvatarMenuModel::GetNumberOfItems() {
  return items_.size();
}

size_t AvatarMenuModel::GetActiveProfileIndex() {
  // During singleton profile deletion, this function can be called with no
  // profiles in the model - crbug.com/102278 .
  if (items_.size() == 0)
    return 0;

  Profile* active_profile = NULL;
  if (!browser_)
    active_profile = ProfileManager::GetLastUsedProfile();
  else
    active_profile = browser_->profile();

  size_t index =
      profile_info_->GetIndexOfProfileWithPath(active_profile->GetPath());

  DCHECK_LT(index, items_.size());
  return index;
}

const AvatarMenuModel::Item& AvatarMenuModel::GetItemAt(size_t index) {
  DCHECK_LT(index, items_.size());
  return *items_[index];
}

bool AvatarMenuModel::ShouldShowAddNewProfileLink() const {
  // |browser_| can be NULL in unit_tests.
  return !browser_ || !browser_->profile()->IsManaged();
}

base::string16 AvatarMenuModel::GetManagedUserInformation() const {
  // |browser_| can be NULL in unit_tests.
  if (browser_ && browser_->profile()->IsManaged()) {
#if defined(ENABLE_MANAGED_USERS)
    ManagedUserService* service = ManagedUserServiceFactory::GetForProfile(
        browser_->profile());
    base::string16 custodian = UTF8ToUTF16(service->GetCustodianEmailAddress());
    return l10n_util::GetStringFUTF16(IDS_MANAGED_USER_INFO, custodian);
#endif
  }
  return base::string16();
}

const gfx::Image& AvatarMenuModel::GetManagedUserIcon() const {
  return ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      IDR_MANAGED_USER_ICON);
}

void AvatarMenuModel::Observe(int type,
                              const content::NotificationSource& source,
                              const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_PROFILE_CACHED_INFO_CHANGED, type);
  RebuildMenu();
  if (observer_)
    observer_->OnAvatarMenuModelChanged(this);
}

// static
bool AvatarMenuModel::ShouldShowAvatarMenu() {
  if (base::FieldTrialList::FindFullName(kShowProfileSwitcherFieldTrialName) ==
      kAlwaysShowSwitcherGroupName) {
    // We should only be in this group when multi-profiles is enabled.
    DCHECK(profiles::IsMultipleProfilesEnabled());
    return true;
  }
  if (profiles::IsMultipleProfilesEnabled()) {
#if defined(OS_CHROMEOS)
    // On ChromeOS the menu will be always visible when it is possible to have
    // two users logged in at the same time.
    return ChromeShellDelegate::instance() &&
           ChromeShellDelegate::instance()->IsMultiProfilesEnabled();
#else
    return profiles::IsNewProfileManagementEnabled() ||
           (g_browser_process->profile_manager() &&
            g_browser_process->profile_manager()->GetNumberOfProfiles() > 1);
#endif
  }
  return false;
}

void AvatarMenuModel::RebuildMenu() {
  ClearMenu();

  const size_t count = profile_info_->GetNumberOfProfiles();
  for (size_t i = 0; i < count; ++i) {
    bool is_gaia_picture =
        profile_info_->IsUsingGAIAPictureOfProfileAtIndex(i) &&
        profile_info_->GetGAIAPictureOfProfileAtIndex(i);

    gfx::Image icon = profile_info_->GetAvatarIconOfProfileAtIndex(i);
    if (!CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kNewProfileManagement)) {
      // old avatar menu uses resized-small images
      icon = profiles::GetAvatarIconForMenu(icon, is_gaia_picture);
    }

    Item* item = new Item(i, icon);
    item->name = profile_info_->GetNameOfProfileAtIndex(i);
    item->sync_state = profile_info_->GetUserNameOfProfileAtIndex(i);
    item->signed_in = !item->sync_state.empty();
    if (!item->signed_in) {
      item->sync_state = l10n_util::GetStringUTF16(
          profile_info_->ProfileIsManagedAtIndex(i) ?
              IDS_MANAGED_USER_AVATAR_LABEL : IDS_PROFILES_LOCAL_PROFILE_STATE);
    }
    if (browser_) {
      base::FilePath path = profile_info_->GetPathOfProfileAtIndex(i);
      item->active = browser_->profile()->GetPath() == path;
    }
    item->signin_required = profile_info_->ProfileIsSigninRequiredAtIndex(i);
    items_.push_back(item);
  }
}

void AvatarMenuModel::ClearMenu() {
  STLDeleteElements(&items_);
}


content::WebContents* AvatarMenuModel::BeginSignOut() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* current_profile = browser_->profile();

  ProfileInfoCache& cache = profile_manager->GetProfileInfoCache();
  size_t index = cache.GetIndexOfProfileWithPath(current_profile->GetPath());
  cache.SetProfileSigninRequiredAtIndex(index, true);

  std::string landing_url = signin::GetLandingURL("close", 1).spec();
  GURL logout_url(GaiaUrls::GetInstance()->service_logout_url().Resolve(
                  "?continue=" + landing_url));
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
