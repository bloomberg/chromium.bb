// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/hosted_app_menu_model.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"

HostedAppMenuModel::HostedAppMenuModel(ui::AcceleratorProvider* provider,
                                       Browser* browser)
    : AppMenuModel(provider, browser) {}

HostedAppMenuModel::~HostedAppMenuModel() {}

void HostedAppMenuModel::Build() {
  AddItemWithStringId(IDC_COPY_URL, IDS_COPY_URL);
  AddItemWithStringId(IDC_OPEN_IN_CHROME, IDS_OPEN_IN_CHROME);
  CreateActionToolbarOverflowMenu();
  CreateZoomMenu();
  AddItemWithStringId(IDC_PRINT, IDS_PRINT);
  AddItemWithStringId(IDC_FIND, IDS_FIND);
  if (media_router::MediaRouterEnabled(browser()->profile()))
    AddItemWithStringId(IDC_ROUTE_MEDIA, IDS_MEDIA_ROUTER_MENU_ITEM_TITLE);
  CreateCutCopyPasteMenu();
  AddItemWithStringId(IDC_SITE_SETTINGS, IDS_SITE_SETTINGS);
  AddItemWithStringId(IDC_APP_INFO, IDS_APP_CONTEXT_MENU_SHOW_INFO);
}
