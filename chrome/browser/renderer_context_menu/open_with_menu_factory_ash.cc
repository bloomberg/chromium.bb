// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/open_with_menu_factory.h"

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
#include "chrome/grit/generated_resources.h"
#include "components/renderer_context_menu/render_view_context_menu_observer.h"
#include "components/renderer_context_menu/render_view_context_menu_proxy.h"
#include "content/public/common/context_menu_params.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"

namespace {

using HandlerMap = std::unordered_map<int, ash::LinkHandlerInfo>;

// An observer class which populates the "Open with <app>" menu items either
// synchronously or asynchronously.
class OpenWithMenuObserver : public RenderViewContextMenuObserver,
                             public ash::LinkHandlerModel::Observer {
 public:
  class SubMenuDelegate : public ui::SimpleMenuModel::Delegate {
   public:
    explicit SubMenuDelegate(OpenWithMenuObserver* parent) : parent_(parent) {}
    ~SubMenuDelegate() override {}

    bool IsCommandIdChecked(int command_id) const override { return false; }
    bool IsCommandIdEnabled(int command_id) const override { return true; }

    bool GetAcceleratorForCommandId(int command_id,
                                    ui::Accelerator* accelerator) override {
      return false;
    }

    void ExecuteCommand(int command_id, int event_flags) override {
      parent_->ExecuteCommand(command_id);
    }

   private:
    OpenWithMenuObserver* const parent_;

    DISALLOW_COPY_AND_ASSIGN(SubMenuDelegate);
  };

  explicit OpenWithMenuObserver(RenderViewContextMenuProxy* proxy)
      : proxy_(proxy),
        submenu_delegate_(this),
        more_apps_label_(
            l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_MORE_APPS)) {}

  ~OpenWithMenuObserver() override {}

  // RenderViewContextMenuObserver overrides:
  void InitMenu(const content::ContextMenuParams& params) override {
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
    for (int i = 0; i < kNumSubMenuCommands; ++i) {
      const int command_id =
          IDC_CONTENT_CONTEXT_OPEN_WITH1 + kNumMainMenuCommands + i;
      submenu->AddItem(command_id, base::EmptyString16());
    }
    submenu_ = std::move(submenu);

    int command_id;
    for (int i = 0; i < kNumMainMenuCommands - 1; ++i) {
      command_id = IDC_CONTENT_CONTEXT_OPEN_WITH1 + i;
      proxy_->AddMenuItem(command_id, base::EmptyString16());
    }
    proxy_->AddSubMenu(++command_id, more_apps_label_, submenu_.get());

    menu_model_->AddObserver(this);
  }

  bool IsCommandIdSupported(int command_id) override {
    return command_id >= IDC_CONTENT_CONTEXT_OPEN_WITH1 &&
           command_id <= IDC_CONTENT_CONTEXT_OPEN_WITH_LAST;
  }

  bool IsCommandIdChecked(int command_id) override { return false; }
  bool IsCommandIdEnabled(int command_id) override { return true; }

  void ExecuteCommand(int command_id) override {
    // Note: SubmenuDelegate also calls this method with a command_id for the
    // submenu.
    const auto it = handlers_.find(command_id);
    if (it == handlers_.end())
      return;
    menu_model_->OpenLinkWithHandler(link_url_, it->second.id);
  }

  void OnMenuCancel() override {}

  // ash::OpenWithItems::Delegate overrides:
  void ModelChanged(
      const std::vector<ash::LinkHandlerInfo>& handlers) override {
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

  static std::pair<HandlerMap, int> BuildHandlersMapForTesting(
      const std::vector<ash::LinkHandlerInfo>& handlers) {
    return BuildHandlersMap(handlers);
  }

 private:
  // Converts |handlers| into HandlerMap which is a map from a command ID to a
  // LinkHandlerInfo and returns the map. Also returns a command id for the
  // parent of the submenu. When the submenu is not needed, the function
  // returns |kInvalidCommandId|.
  static std::pair<HandlerMap, int> BuildHandlersMap(
      const std::vector<ash::LinkHandlerInfo>& handlers) {
    const int kInvalidCommandId = -1;
    const int submenu_id_start =
        IDC_CONTENT_CONTEXT_OPEN_WITH1 + kNumMainMenuCommands;

    HandlerMap handler_map;
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

  static const int kNumMainMenuCommands;
  static const int kNumSubMenuCommands;

  RenderViewContextMenuProxy* const proxy_;
  SubMenuDelegate submenu_delegate_;
  const base::string16 more_apps_label_;
  GURL link_url_;

  // A menu model received from Ash side.
  std::unique_ptr<ash::LinkHandlerModel> menu_model_;
  HandlerMap handlers_;
  // A submenu passed to Chrome side.
  std::unique_ptr<ui::MenuModel> submenu_;

  DISALLOW_COPY_AND_ASSIGN(OpenWithMenuObserver);
};

const int OpenWithMenuObserver::kNumMainMenuCommands = 4;
const int OpenWithMenuObserver::kNumSubMenuCommands = 10;

}  // namespace

std::pair<HandlerMap, int> BuildHandlersMapForTesting(
    const std::vector<ash::LinkHandlerInfo>& handlers) {
  return OpenWithMenuObserver::BuildHandlersMapForTesting(handlers);
}

RenderViewContextMenuObserver* OpenWithMenuFactory::CreateMenu(
    RenderViewContextMenuProxy* proxy) {
  return new OpenWithMenuObserver(proxy);
}
