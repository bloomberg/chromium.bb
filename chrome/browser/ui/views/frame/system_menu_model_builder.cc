// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/system_menu_model_builder.h"

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/toolbar/wrench_menu_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/models/simple_menu_model.h"

#if defined(OS_CHROMEOS)
#include "ash/session/session_state_delegate.h"
#include "ash/shell.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/user_manager/user_info.h"
#include "ui/base/l10n/l10n_util.h"
#endif

SystemMenuModelBuilder::SystemMenuModelBuilder(
    ui::AcceleratorProvider* provider,
    Browser* browser)
    : menu_delegate_(provider, browser) {
}

SystemMenuModelBuilder::~SystemMenuModelBuilder() {
}

void SystemMenuModelBuilder::Init() {
  ui::SimpleMenuModel* model = new ui::SimpleMenuModel(&menu_delegate_);
  menu_model_.reset(model);
  BuildMenu(model);
#if defined(OS_WIN)
  // On Windows with HOST_DESKTOP_TYPE_NATIVE we put the menu items in the
  // system menu (not at the end). Doing this necessitates adding a trailing
  // separator.
  if (browser()->host_desktop_type() == chrome::HOST_DESKTOP_TYPE_NATIVE)
    model->AddSeparator(ui::NORMAL_SEPARATOR);
#endif
}

void SystemMenuModelBuilder::BuildMenu(ui::SimpleMenuModel* model) {
  // We add the menu items in reverse order so that insertion_index never needs
  // to change.
  if (browser()->is_type_tabbed())
    BuildSystemMenuForBrowserWindow(model);
  else
    BuildSystemMenuForAppOrPopupWindow(model);
  AddFrameToggleItems(model);
}

void SystemMenuModelBuilder::BuildSystemMenuForBrowserWindow(
    ui::SimpleMenuModel* model) {
  model->AddItemWithStringId(IDC_NEW_TAB, IDS_NEW_TAB);
  model->AddItemWithStringId(IDC_RESTORE_TAB, IDS_RESTORE_TAB);
  if (chrome::CanOpenTaskManager()) {
    model->AddSeparator(ui::NORMAL_SEPARATOR);
    model->AddItemWithStringId(IDC_TASK_MANAGER, IDS_TASK_MANAGER);
  }
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  model->AddSeparator(ui::NORMAL_SEPARATOR);
  model->AddCheckItemWithStringId(IDC_USE_SYSTEM_TITLE_BAR,
                                  IDS_SHOW_WINDOW_DECORATIONS_MENU);
#endif
  AppendTeleportMenu(model);
  // If it's a regular browser window with tabs, we don't add any more items,
  // since it already has menus (Page, Chrome).
}

void SystemMenuModelBuilder::BuildSystemMenuForAppOrPopupWindow(
    ui::SimpleMenuModel* model) {
  model->AddItemWithStringId(IDC_BACK, IDS_CONTENT_CONTEXT_BACK);
  model->AddItemWithStringId(IDC_FORWARD, IDS_CONTENT_CONTEXT_FORWARD);
  model->AddItemWithStringId(IDC_RELOAD, IDS_APP_MENU_RELOAD);
  model->AddSeparator(ui::NORMAL_SEPARATOR);
  if (browser()->is_app())
    model->AddItemWithStringId(IDC_NEW_TAB, IDS_APP_MENU_NEW_WEB_PAGE);
  else
    model->AddItemWithStringId(IDC_SHOW_AS_TAB, IDS_SHOW_AS_TAB);
  model->AddSeparator(ui::NORMAL_SEPARATOR);
  model->AddItemWithStringId(IDC_CUT, IDS_CUT);
  model->AddItemWithStringId(IDC_COPY, IDS_COPY);
  model->AddItemWithStringId(IDC_PASTE, IDS_PASTE);
  model->AddSeparator(ui::NORMAL_SEPARATOR);
  model->AddItemWithStringId(IDC_FIND, IDS_FIND);
  model->AddItemWithStringId(IDC_PRINT, IDS_PRINT);
  zoom_menu_contents_.reset(new ZoomMenuModel(&menu_delegate_));
  model->AddSubMenuWithStringId(IDC_ZOOM_MENU, IDS_ZOOM_MENU,
                                zoom_menu_contents_.get());
  encoding_menu_contents_.reset(new EncodingMenuModel(browser()));
  model->AddSubMenuWithStringId(IDC_ENCODING_MENU,
                                IDS_ENCODING_MENU,
                                encoding_menu_contents_.get());
  if (browser()->is_app() && chrome::CanOpenTaskManager()) {
    model->AddSeparator(ui::NORMAL_SEPARATOR);
    model->AddItemWithStringId(IDC_TASK_MANAGER, IDS_TASK_MANAGER);
  }
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  model->AddSeparator(ui::NORMAL_SEPARATOR);
  model->AddItemWithStringId(IDC_CLOSE_WINDOW, IDS_CLOSE);
#endif

  AppendTeleportMenu(model);
}

void SystemMenuModelBuilder::AddFrameToggleItems(ui::SimpleMenuModel* model) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDebugEnableFrameToggle)) {
    model->AddSeparator(ui::NORMAL_SEPARATOR);
    model->AddItem(IDC_DEBUG_FRAME_TOGGLE,
                   base::ASCIIToUTF16("Toggle Frame Type"));
  }
}

void SystemMenuModelBuilder::AppendTeleportMenu(ui::SimpleMenuModel* model) {
#if defined(OS_CHROMEOS)
  DCHECK(browser()->window());
  // If there is no manager, we are not in the proper multi user mode.
  if (chrome::MultiUserWindowManager::GetMultiProfileMode() !=
          chrome::MultiUserWindowManager::MULTI_PROFILE_MODE_SEPARATED)
    return;

  // Don't show the menu for incognito windows.
  if (browser()->profile()->IsOffTheRecord())
    return;

  // To show the menu we need at least two logged in users.
  ash::SessionStateDelegate* delegate =
      ash::Shell::GetInstance()->session_state_delegate();
  int logged_in_users = delegate->NumberOfLoggedInUsers();
  if (logged_in_users <= 1)
    return;

  // If this does not belong to a profile or there is no window, or the window
  // is not owned by anyone, we don't show the menu addition.
  chrome::MultiUserWindowManager* manager =
      chrome::MultiUserWindowManager::GetInstance();
  const std::string user_id =
      multi_user_util::GetUserIDFromProfile(browser()->profile());
  aura::Window* window = browser()->window()->GetNativeWindow();
  if (user_id.empty() || !window || manager->GetWindowOwner(window).empty())
    return;

  model->AddSeparator(ui::NORMAL_SEPARATOR);
  DCHECK(logged_in_users <= 3);
  for (int user_index = 1; user_index < logged_in_users; ++user_index) {
    const user_manager::UserInfo* user_info = delegate->GetUserInfo(user_index);
    model->AddItem(
        user_index == 1 ? IDC_VISIT_DESKTOP_OF_LRU_USER_2
                        : IDC_VISIT_DESKTOP_OF_LRU_USER_3,
        l10n_util::GetStringFUTF16(IDS_VISIT_DESKTOP_OF_LRU_USER,
                                   user_info->GetDisplayName(),
                                   base::ASCIIToUTF16(user_info->GetEmail())));
  }
#endif
}
