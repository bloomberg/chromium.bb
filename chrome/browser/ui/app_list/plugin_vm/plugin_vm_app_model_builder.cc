// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/plugin_vm/plugin_vm_app_model_builder.h"

#include "base/no_destructor.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/plugin_vm/plugin_vm_app_item.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "components/crx_file/id_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {
const char kPluginVmTerminalAppName[] = "PluginVm";
}  // namespace

PluginVmAppModelBuilder::PluginVmAppModelBuilder(
    AppListControllerDelegate* controller)
    : AppListModelBuilder(controller, PluginVmAppItem::kItemType) {}

PluginVmAppModelBuilder::~PluginVmAppModelBuilder() = default;

void PluginVmAppModelBuilder::BuildModel() {
  static const base::NoDestructor<std::string> kPluginVmTerminalId(
      crx_file::id_util::GenerateId(kPluginVmTerminalAppName));
  InsertApp(std::make_unique<PluginVmAppItem>(
      profile(), model_updater(), GetSyncItem(*kPluginVmTerminalId),
      *kPluginVmTerminalId, kPluginVmTerminalAppName,
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_LOGO_PLUGIN_VM_LAUNCHER)));
}
