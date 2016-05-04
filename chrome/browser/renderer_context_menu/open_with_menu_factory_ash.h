// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_CONTEXT_MENU_OPEN_WITH_MENU_FACTORY_ASH_H_
#define CHROME_BROWSER_RENDERER_CONTEXT_MENU_OPEN_WITH_MENU_FACTORY_ASH_H_

#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ash/link_handler_model.h"
#include "base/macros.h"
#include "components/renderer_context_menu/render_view_context_menu_observer.h"
#include "ui/base/models/simple_menu_model.h"
#include "url/gurl.h"

class RenderViewContextMenuProxy;

namespace ui {
class SimpleMenuModel;
}  // namespace ui

// An observer class which populates the "Open with <app>" menu items either
// synchronously or asynchronously.
class OpenWithMenuObserver : public RenderViewContextMenuObserver,
                             public ash::LinkHandlerModel::Observer {
 public:
  using HandlerMap = std::unordered_map<int, ash::LinkHandlerInfo>;

  class SubMenuDelegate : public ui::SimpleMenuModel::Delegate {
   public:
    explicit SubMenuDelegate(OpenWithMenuObserver* parent) : parent_(parent) {}
    ~SubMenuDelegate() override {}

    bool IsCommandIdChecked(int command_id) const override;
    bool IsCommandIdEnabled(int command_id) const override;
    bool GetAcceleratorForCommandId(int command_id,
                                    ui::Accelerator* accelerator) override;
    void ExecuteCommand(int command_id, int event_flags) override;

   private:
    OpenWithMenuObserver* const parent_;

    DISALLOW_COPY_AND_ASSIGN(SubMenuDelegate);
  };

  explicit OpenWithMenuObserver(RenderViewContextMenuProxy* proxy);
  ~OpenWithMenuObserver() override;

  // RenderViewContextMenuObserver overrides:
  void InitMenu(const content::ContextMenuParams& params) override;
  bool IsCommandIdSupported(int command_id) override;
  bool IsCommandIdChecked(int command_id) override;
  bool IsCommandIdEnabled(int command_id) override;
  void ExecuteCommand(int command_id) override;
  void OnMenuCancel() override {}

  // ash::OpenWithItems::Delegate overrides:
  void ModelChanged(const std::vector<ash::LinkHandlerInfo>& handlers) override;

  static void AddPlaceholderItemsForTesting(RenderViewContextMenuProxy* proxy,
                                            ui::SimpleMenuModel* submenu);
  static std::pair<HandlerMap, int> BuildHandlersMapForTesting(
      const std::vector<ash::LinkHandlerInfo>& handlers);

 private:
  // Adds placeholder items and the |submenu| to the |proxy|.
  static void AddPlaceholderItems(RenderViewContextMenuProxy* proxy,
                                  ui::SimpleMenuModel* submenu);

  // Converts |handlers| into HandlerMap which is a map from a command ID to a
  // LinkHandlerInfo and returns the map. Also returns a command id for the
  // parent of the submenu. When the submenu is not needed, the function
  // returns |kInvalidCommandId|.
  static std::pair<OpenWithMenuObserver::HandlerMap, int> BuildHandlersMap(
      const std::vector<ash::LinkHandlerInfo>& handlers);

  static const int kNumMainMenuCommands;
  static const int kNumSubMenuCommands;

  RenderViewContextMenuProxy* const proxy_;
  SubMenuDelegate submenu_delegate_;
  const base::string16 more_apps_label_;
  GURL link_url_;

  // A menu model received from Ash side.
  std::unique_ptr<ash::LinkHandlerModel> menu_model_;
  OpenWithMenuObserver::HandlerMap handlers_;
  // A submenu passed to Chrome side.
  std::unique_ptr<ui::MenuModel> submenu_;

  DISALLOW_COPY_AND_ASSIGN(OpenWithMenuObserver);
};

#endif  // CHROME_BROWSER_RENDERER_CONTEXT_MENU_OPEN_WITH_MENU_FACTORY_ASH_H_
