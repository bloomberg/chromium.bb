// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/crostini/crostini_app_model_builder.h"

#include "ash/resources/grit/ash_resources.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/crostini/crostini_app_item.h"
#include "components/crx_file/id_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {
const char kCrostiniTerminalAppName[] = "Terminal";
}  // namespace

CrostiniAppModelBuilder::CrostiniAppModelBuilder(
    AppListControllerDelegate* controller)
    : AppListModelBuilder(controller, CrostiniAppItem::kItemType) {}

CrostiniAppModelBuilder::~CrostiniAppModelBuilder() = default;

void CrostiniAppModelBuilder::BuildModel() {
  static const std::string kCrostiniTerminalId =
      crx_file::id_util::GenerateId(kCrostiniTerminalAppName);
  InsertApp(std::make_unique<CrostiniAppItem>(
      profile(), GetSyncItem(kCrostiniTerminalId), kCrostiniTerminalId,
      kCrostiniTerminalAppName,
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_LOGO_CROSTINI_TERMINAL)));
}
