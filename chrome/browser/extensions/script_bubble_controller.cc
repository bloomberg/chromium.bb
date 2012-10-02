// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/script_bubble_controller.h"

#include "base/string_number_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/location_bar_controller.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_action.h"
#include "third_party/skia/include/core/SkColor.h"

namespace extensions {

namespace {

const SkColor kBadgeBackgroundColor = 0xEEEEDD00;

}  // namespace

ScriptBubbleController::ScriptBubbleController(TabHelper* tab_helper)
    : TabHelper::ContentScriptObserver(tab_helper) {
}

void ScriptBubbleController::OnContentScriptsExecuting(
      const content::WebContents* web_contents,
      const ExecutingScriptsMap& extension_ids,
      int32 page_id,
      const GURL& on_url) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  ComponentLoader* loader =
      ExtensionSystem::Get(profile)->extension_service()->component_loader();
  const Extension* extension = loader->GetScriptBubble();
  if (!extension)
    return;

  int tab_id = ExtensionTabUtil::GetTabId(web_contents);
  ExtensionAction* page_action = extension->page_action();

  page_action->SetAppearance(tab_id, ExtensionAction::ACTIVE);
  page_action->SetBadgeText(tab_id, base::UintToString(extension_ids.size()));
  page_action->SetBadgeBackgroundColor(tab_id, kBadgeBackgroundColor);

  tab_helper_->location_bar_controller()->NotifyChange();
}

}  // namespace extensions
