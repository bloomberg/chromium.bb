// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/profile_menu_model.h"

#include "base/path_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

class SwitchProfileMenuModel : public ui::SimpleMenuModel,
                               public ui::SimpleMenuModel::Delegate {
 public:
  SwitchProfileMenuModel(ui::SimpleMenuModel::Delegate* delegate,
                         Browser* browser);

  // Overridden from ui::SimpleMenuModel::Delegate.
  virtual void ExecuteCommand(int command_id) OVERRIDE;
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;

 private:
  bool IsSwitchProfileCommand(int command_id) const;
  size_t GetProfileIndexFromSwitchProfileCommand(int command_id) const;

  Browser* browser_;
  ui::SimpleMenuModel::Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(SwitchProfileMenuModel);
};

class ProfileSwitchObserver : public ProfileManagerObserver {
  virtual void OnProfileCreated(Profile* profile, Status status) OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    if (status == STATUS_INITIALIZED) {
      DCHECK(profile);
      Browser* browser = BrowserList::FindTabbedBrowser(profile, false);
      if (browser)
        browser->window()->Activate();
      else
        Browser::NewWindowWithProfile(profile);
    }
  }

  virtual bool DeleteAfter() OVERRIDE { return true; }
};

SwitchProfileMenuModel::SwitchProfileMenuModel(
    ui::SimpleMenuModel::Delegate* delegate,
    Browser* browser)
    : SimpleMenuModel(this),
      browser_(browser),
      delegate_(delegate) {
  ProfileInfoInterface& cache =
      g_browser_process->profile_manager()->GetProfileInfo();
  size_t count = cache.GetNumberOfProfiles();
  for (size_t i = 0; i < count; ++i) {
    AddCheckItem(ProfileMenuModel::COMMAND_SWITCH_TO_PROFILE + i,
                 cache.GetNameOfProfileAtIndex(i));
  }

  AddSeparator();

  AddItemWithStringId(ProfileMenuModel::COMMAND_CREATE_NEW_PROFILE,
                      IDS_PROFILES_CREATE_NEW_PROFILE_OPTION);
}

void SwitchProfileMenuModel::ExecuteCommand(int command_id) {
  ProfileInfoInterface& cache =
      g_browser_process->profile_manager()->GetProfileInfo();
  if (IsSwitchProfileCommand(command_id)) {
    size_t index = GetProfileIndexFromSwitchProfileCommand(command_id);
    FilePath profile_path = cache.GetPathOfProfileAtIndex(index);
    ProfileSwitchObserver* observer = new ProfileSwitchObserver;
    // The observer is deleted by the manager when profile creation is finished.
    g_browser_process->profile_manager()->CreateProfileAsync(
        profile_path, observer);
  } else {
    delegate_->ExecuteCommand(command_id);
  }
}

bool SwitchProfileMenuModel::IsCommandIdChecked(int command_id) const {
  ProfileInfoInterface& cache =
      g_browser_process->profile_manager()->GetProfileInfo();
  if (IsSwitchProfileCommand(command_id)) {
    size_t index = GetProfileIndexFromSwitchProfileCommand(command_id);
    FilePath userDataFolder;
    PathService::Get(chrome::DIR_USER_DATA, &userDataFolder);
    FilePath profile_path = cache.GetPathOfProfileAtIndex(index);
    return browser_->GetProfile()->GetPath() == profile_path;
  }
  return delegate_->IsCommandIdChecked(command_id);
}

bool SwitchProfileMenuModel::IsCommandIdEnabled(int command_id) const {
  if (IsSwitchProfileCommand(command_id))
    return true;
  return delegate_->IsCommandIdEnabled(command_id);
}

bool SwitchProfileMenuModel::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) {
  if (IsSwitchProfileCommand(command_id))
    return false;
  return delegate_->GetAcceleratorForCommandId(command_id, accelerator);
}

bool SwitchProfileMenuModel::IsSwitchProfileCommand(int command_id) const {
  return command_id >= ProfileMenuModel::COMMAND_SWITCH_TO_PROFILE;
}

size_t SwitchProfileMenuModel::GetProfileIndexFromSwitchProfileCommand(
    int command_id) const {
  DCHECK(IsSwitchProfileCommand(command_id));
  return command_id - ProfileMenuModel::COMMAND_SWITCH_TO_PROFILE;
}

} // namespace

ProfileMenuModel::ProfileMenuModel(Browser* browser)
    : ALLOW_THIS_IN_INITIALIZER_LIST(ui::SimpleMenuModel(this)),
      browser_(browser) {
  ProfileInfoInterface& cache =
      g_browser_process->profile_manager()->GetProfileInfo();
  size_t profile_index = cache.GetIndexOfProfileWithPath(
      browser_->profile()->GetPath());
  AddItem(COMMAND_PROFILE_NAME, cache.GetNameOfProfileAtIndex(profile_index));

  // TODO(sail): Need to implement an icon chooser on other platforms too.
#if defined(TOOLKIT_VIEWS)
  AddItem(COMMAND_CHOOSE_AVATAR_ICON, string16());
#endif

  AddItemWithStringId(COMMAND_CUSTOMIZE_PROFILE,
                      IDS_PROFILES_CUSTOMIZE_PROFILE);
  AddSeparator();

  if (cache.GetNumberOfProfiles() > 1) {
    switch_profiles_sub_menu_model_.reset(
        new SwitchProfileMenuModel(this, browser_));
    AddSubMenuWithStringId(COMMAND_SWITCH_PROFILE_MENU, IDS_PROFILES_MENU,
                           switch_profiles_sub_menu_model_.get());
  } else {
    switch_profiles_sub_menu_model_.reset();
    AddItemWithStringId(COMMAND_CREATE_NEW_PROFILE,
                        IDS_PROFILES_CREATE_NEW_PROFILE_OPTION);
  }
}

ProfileMenuModel::~ProfileMenuModel() {
}

// ui::SimpleMenuModel::Delegate implementation
bool ProfileMenuModel::IsCommandIdChecked(int command_id) const {
  return false;
}

bool ProfileMenuModel::IsCommandIdEnabled(int command_id) const {
  switch (command_id) {
    case COMMAND_PROFILE_NAME:
      return false;
    default:
      return true;
  }
}

bool ProfileMenuModel::GetAcceleratorForCommandId(int command_id,
    ui::Accelerator* accelerator) {
  return false;
}

void ProfileMenuModel::ExecuteCommand(int command_id) {
  switch (command_id) {
    case COMMAND_CUSTOMIZE_PROFILE:
      browser_->ShowOptionsTab(chrome::kPersonalOptionsSubPage);
      break;
    case COMMAND_CHOOSE_AVATAR_ICON:
      break;
    case COMMAND_CREATE_NEW_PROFILE:
      ProfileManager::CreateMultiProfileAsync();
      break;
    default:
      NOTREACHED();
      break;
  }
}
