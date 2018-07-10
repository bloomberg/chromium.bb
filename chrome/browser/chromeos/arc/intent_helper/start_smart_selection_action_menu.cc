// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/intent_helper/start_smart_selection_action_menu.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/grit/generated_resources.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_features.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/common/intent_helper.mojom.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/renderer_context_menu/render_view_context_menu_proxy.h"
#include "content/public/common/context_menu_params.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace arc {

constexpr size_t kSmallIconSizeInDip = 16;
constexpr size_t kMaxIconSizeInPx = 200;

StartSmartSelectionActionMenu::StartSmartSelectionActionMenu(
    RenderViewContextMenuProxy* proxy)
    : proxy_(proxy) {}

StartSmartSelectionActionMenu::~StartSmartSelectionActionMenu() = default;

void StartSmartSelectionActionMenu::InitMenu(
    const content::ContextMenuParams& params) {
  if (!base::FeatureList::IsEnabled(kSmartTextSelectionFeature))
    return;

  if (params.selection_text.empty())
    return;

  auto* arc_service_manager = ArcServiceManager::Get();
  if (!arc_service_manager)
    return;
  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_service_manager->arc_bridge_service()->intent_helper(),
      ClassifySelection);
  if (!instance)
    return;

  instance->ClassifySelection(
      base::UTF16ToUTF8(params.selection_text),
      mojom::ScaleFactor(ui::GetSupportedScaleFactors().back()),
      base::BindOnce(&StartSmartSelectionActionMenu::OnSelectionClassified,
                     weak_ptr_factory_.GetWeakPtr()));

  // Add placeholder item.
  proxy_->AddMenuItem(IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION,
                      /*title=*/base::EmptyString16());
}

bool StartSmartSelectionActionMenu::IsCommandIdSupported(int command_id) {
  return command_id == IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION;
}

bool StartSmartSelectionActionMenu::IsCommandIdChecked(int command_id) {
  return false;
}

bool StartSmartSelectionActionMenu::IsCommandIdEnabled(int command_id) {
  return true;
}

void StartSmartSelectionActionMenu::ExecuteCommand(int command_id) {
  if (command_id != IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION)
    return;

  auto* arc_service_manager = ArcServiceManager::Get();
  if (!arc_service_manager)
    return;
  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_service_manager->arc_bridge_service()->intent_helper(), HandleIntent);
  if (!instance)
    return;

  if (text_selection_action_) {
    instance->HandleIntent(std::move(text_selection_action_->action_intent),
                           std::move(text_selection_action_->activity));
  }
}

void StartSmartSelectionActionMenu::OnSelectionClassified(
    mojom::TextSelectionActionPtr action) {
  if (!action) {
    // No actions returned from classification, hide item.
    proxy_->UpdateMenuItem(IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION,
                           /*enabled=*/false, /*hidden=*/true,
                           /*title=*/base::EmptyString16());
    return;
  }

  text_selection_action_ = std::move(action);

  // Update the menu item with the action's title.
  proxy_->UpdateMenuItem(
      IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION,
      /*enabled=*/true,
      /*hidden=*/false,
      /*title=*/base::UTF8ToUTF16(text_selection_action_->title));
  if (text_selection_action_->icon) {
    auto icon = GetIconImage(std::move(text_selection_action_->icon));
    if (icon) {
      proxy_->UpdateMenuIcon(IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION,
                             *icon.get());
    }
  }
  proxy_->AddSeparatorBelowMenuItem(
      IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION);
}

std::unique_ptr<gfx::Image> StartSmartSelectionActionMenu::GetIconImage(
    mojom::ActivityIconPtr icon) {
  constexpr size_t kBytesPerPixel = 4;  // BGRA
  if (icon->width > kMaxIconSizeInPx || icon->height > kMaxIconSizeInPx ||
      icon->width == 0 || icon->height == 0 ||
      icon->icon.size() != (icon->width * icon->height * kBytesPerPixel)) {
    return nullptr;
  }

  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(icon->width, icon->height));
  if (!bitmap.getPixels())
    return nullptr;
  DCHECK_GE(bitmap.computeByteSize(), icon->icon.size());
  memcpy(bitmap.getPixels(), &icon->icon.front(), icon->icon.size());

  gfx::ImageSkia original(gfx::ImageSkia::CreateFrom1xBitmap(bitmap));

  gfx::ImageSkia icon_small(gfx::ImageSkiaOperations::CreateResizedImage(
      original, skia::ImageOperations::RESIZE_BEST,
      gfx::Size(kSmallIconSizeInDip, kSmallIconSizeInDip)));

  return std::make_unique<gfx::Image>(icon_small);
}

}  // namespace arc
