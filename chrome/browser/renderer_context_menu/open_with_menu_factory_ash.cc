// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/open_with_menu_factory_ash.h"

#include <algorithm>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ash/link_handler_model.h"
#include "ash/link_handler_model_factory.h"
#include "ash/shell.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/renderer_context_menu/open_with_menu_factory.h"
#include "chrome/grit/generated_resources.h"
#include "components/renderer_context_menu/render_view_context_menu_proxy.h"
#include "content/public/common/context_menu_params.h"
#include "ui/base/l10n/l10n_util.h"

const int OpenWithMenuObserver::kNumMainMenuCommands = 4;
const int OpenWithMenuObserver::kNumSubMenuCommands = 10;

bool OpenWithMenuObserver::SubMenuDelegate::IsCommandIdChecked(
    int command_id) const {
  return false;
}

bool OpenWithMenuObserver::SubMenuDelegate::IsCommandIdEnabled(
    int command_id) const {
  return true;
}

void OpenWithMenuObserver::SubMenuDelegate::ExecuteCommand(int command_id,
                                                           int event_flags) {
  parent_->ExecuteCommand(command_id);
}

OpenWithMenuObserver::OpenWithMenuObserver(RenderViewContextMenuProxy* proxy)
    : proxy_(proxy),
      submenu_delegate_(this),
      more_apps_label_(
          l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_MORE_APPS)) {}

OpenWithMenuObserver::~OpenWithMenuObserver() {}

void OpenWithMenuObserver::InitMenu(const content::ContextMenuParams& params) {
  if (!ash::Shell::HasInstance())
    return;
  ash::LinkHandlerModelFactory* factory =
      ash::Shell::GetInstance()->link_handler_model_factory();
  if (!factory)
    return;

  link_url_ = params.link_url;
  menu_model_ = factory->CreateModel(link_url_);
  if (!menu_model_)
    return;

  // Add placeholder items.
  std::unique_ptr<ui::SimpleMenuModel> submenu(
      new ui::SimpleMenuModel(&submenu_delegate_));
  AddPlaceholderItems(proxy_, submenu.get());
  submenu_ = std::move(submenu);

  menu_model_->AddObserver(this);
}

bool OpenWithMenuObserver::IsCommandIdSupported(int command_id) {
  return command_id >= IDC_CONTENT_CONTEXT_OPEN_WITH1 &&
         command_id <= IDC_CONTENT_CONTEXT_OPEN_WITH_LAST;
}

bool OpenWithMenuObserver::IsCommandIdChecked(int command_id) {
  return false;
}

bool OpenWithMenuObserver::IsCommandIdEnabled(int command_id) {
  return true;
}

void OpenWithMenuObserver::ExecuteCommand(int command_id) {
  // Note: SubmenuDelegate also calls this method with a command_id for the
  // submenu.
  const auto it = handlers_.find(command_id);
  if (it == handlers_.end())
    return;
  menu_model_->OpenLinkWithHandler(link_url_, it->second.id);
}

void OpenWithMenuObserver::ModelChanged(
    const std::vector<ash::LinkHandlerInfo>& handlers) {
  auto result = BuildHandlersMap(handlers);
  handlers_ = std::move(result.first);
  const int submenu_parent_id = result.second;
  for (int command_id = IDC_CONTENT_CONTEXT_OPEN_WITH1;
       command_id <= IDC_CONTENT_CONTEXT_OPEN_WITH_LAST; ++command_id) {
    const auto it = handlers_.find(command_id);
    if (command_id == submenu_parent_id) {
      // Show the submenu parent.
      proxy_->UpdateMenuItem(command_id, true, false, more_apps_label_);
    } else if (it == handlers_.end()) {
      // Hide the menu or submenu parent.
      proxy_->UpdateMenuItem(command_id, false, true, base::EmptyString16());
    } else {
      // Update the menu with the new model.
      const base::string16 label =
          l10n_util::GetStringFUTF16(IDS_CONTENT_CONTEXT_OPEN_WITH_APP,
                                     base::UTF8ToUTF16(it->second.name));
      proxy_->UpdateMenuItem(command_id, true, false, label);
      if (!it->second.icon.IsEmpty())
        proxy_->UpdateMenuIcon(command_id, it->second.icon);
    }
  }
}

void OpenWithMenuObserver::AddPlaceholderItemsForTesting(
    RenderViewContextMenuProxy* proxy,
    ui::SimpleMenuModel* submenu) {
  return AddPlaceholderItems(proxy, submenu);
}

std::pair<OpenWithMenuObserver::HandlerMap, int>
OpenWithMenuObserver::BuildHandlersMapForTesting(
    const std::vector<ash::LinkHandlerInfo>& handlers) {
  return BuildHandlersMap(handlers);
}

void OpenWithMenuObserver::AddPlaceholderItems(
    RenderViewContextMenuProxy* proxy,
    ui::SimpleMenuModel* submenu) {
  for (int i = 0; i < kNumSubMenuCommands; ++i) {
    const int command_id =
        IDC_CONTENT_CONTEXT_OPEN_WITH1 + kNumMainMenuCommands + i;
    submenu->AddItem(command_id, base::EmptyString16());
  }
  int command_id;
  for (int i = 0; i < kNumMainMenuCommands - 1; ++i) {
    command_id = IDC_CONTENT_CONTEXT_OPEN_WITH1 + i;
    proxy->AddMenuItem(command_id, base::EmptyString16());
  }
  proxy->AddSubMenu(++command_id, base::EmptyString16(), submenu);
}

std::pair<OpenWithMenuObserver::HandlerMap, int>
OpenWithMenuObserver::BuildHandlersMap(
    const std::vector<ash::LinkHandlerInfo>& handlers) {
  const int kInvalidCommandId = -1;
  const int submenu_id_start =
      IDC_CONTENT_CONTEXT_OPEN_WITH1 + kNumMainMenuCommands;

  OpenWithMenuObserver::HandlerMap handler_map;
  int submenu_parent_command_id = kInvalidCommandId;

  const int num_apps = handlers.size();
  size_t handlers_index = 0;
  // We use the last item in the main menu (IDC_CONTENT_CONTEXT_OPEN_WITH1 +
  // kNumMainMenuCommands- 1) as a parent of a submenu, and others as regular
  // menu items.
  if (num_apps < kNumMainMenuCommands) {
    // All apps can be shown with the regular main menu items.
    for (int i = 0; i < num_apps; ++i) {
      handler_map[IDC_CONTENT_CONTEXT_OPEN_WITH1 + i] =
          handlers[handlers_index++];
    }
  } else {
    // Otherwise, use the submenu too. In this case, disable the last item of
    // the regular main menu (hence '-2').
    for (int i = 0; i < kNumMainMenuCommands - 2; ++i) {
      handler_map[IDC_CONTENT_CONTEXT_OPEN_WITH1 + i] =
          handlers[handlers_index++];
    }
    submenu_parent_command_id =
        IDC_CONTENT_CONTEXT_OPEN_WITH1 + kNumMainMenuCommands - 1;
    const int sub_items =
        std::min(num_apps - (kNumMainMenuCommands - 2), kNumSubMenuCommands);
    for (int i = 0; i < sub_items; ++i) {
      handler_map[submenu_id_start + i] = handlers[handlers_index++];
    }
  }

  return std::make_pair(std::move(handler_map), submenu_parent_command_id);
}

RenderViewContextMenuObserver* OpenWithMenuFactory::CreateMenu(
    RenderViewContextMenuProxy* proxy) {
  return new OpenWithMenuObserver(proxy);
}
