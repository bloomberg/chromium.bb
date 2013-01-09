// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/system_menu_model_builder.h"

#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/toolbar/wrench_menu_model.h"
#include "chrome/common/chrome_switches.h"
#include "grit/generated_resources.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/models/simple_menu_model.h"

#if defined(OS_WIN)
#include "chrome/browser/ui/views/frame/system_menu_model.h"
#endif

SystemMenuModelBuilder::SystemMenuModelBuilder(
    ui::AcceleratorProvider* provider,
    Browser* browser)
    : menu_delegate_(provider, browser) {
}

SystemMenuModelBuilder::~SystemMenuModelBuilder() {
}

void SystemMenuModelBuilder::Init() {
  bool needs_trailing_separator = false;
  ui::SimpleMenuModel* model = CreateMenuModel(&needs_trailing_separator);
  menu_model_.reset(model);
  BuildMenu(model);
  if (needs_trailing_separator)
    model->AddSeparator(ui::NORMAL_SEPARATOR);
}

ui::SimpleMenuModel* SystemMenuModelBuilder::CreateMenuModel(
    bool* needs_trailing_separator) {
#if defined(OS_WIN)
  if (browser()->host_desktop_type() == chrome::HOST_DESKTOP_TYPE_NATIVE) {
    *needs_trailing_separator = true;
    return new SystemMenuModel(&menu_delegate_);
  }
#endif
  *needs_trailing_separator = false;
  return new ui::SimpleMenuModel(&menu_delegate_);
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
}

void SystemMenuModelBuilder::AddFrameToggleItems(ui::SimpleMenuModel* model) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDebugEnableFrameToggle)) {
    model->AddSeparator(ui::NORMAL_SEPARATOR);
    model->AddItem(IDC_DEBUG_FRAME_TOGGLE, ASCIIToUTF16("Toggle Frame Type"));
  }
}

