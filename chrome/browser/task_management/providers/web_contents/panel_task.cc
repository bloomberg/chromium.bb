// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_management/providers/web_contents/panel_task.h"

#include "base/i18n/rtl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/panels/panel.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "ui/gfx/image/image_skia.h"

namespace task_management {

namespace {

const gfx::ImageSkia* GetPanelIcon(Panel* panel) {
  const gfx::Image icon = panel->GetCurrentPageIcon();
  return !icon.IsEmpty() ? icon.ToImageSkia() : nullptr;
}

}  // namespace

PanelTask::PanelTask(Panel* panel, content::WebContents* web_contents)
    : RendererTask(GetCurrentPanelTitle(panel),
                   GetPanelIcon(panel),
                   web_contents,
                   web_contents->GetRenderProcessHost()),
      panel_(panel) {
}

PanelTask::~PanelTask() {
}

void PanelTask::UpdateTitle() {
  set_title(GetCurrentPanelTitle(panel_));
}

void PanelTask::UpdateFavicon() {
  const gfx::ImageSkia* icon = GetPanelIcon(panel_);
  set_icon(icon ? *icon : gfx::ImageSkia());
}

Task::Type PanelTask::GetType() const {
  return Task::EXTENSION;
}

base::string16 PanelTask::GetCurrentPanelTitle(Panel* panel) const {
  base::string16 title = panel->GetWindowTitle();
  base::i18n::AdjustStringForLocaleDirection(&title);

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(panel->profile());
  const extensions::Extension* extension =
      registry->enabled_extensions().GetByID(panel->extension_id());

  const bool is_app = extension && extension->is_app();
  const bool is_incognito = panel->profile()->IsOffTheRecord();

  return PrefixRendererTitle(title,
                             is_app,
                             true,  // is_extension.
                             is_incognito,
                             false);  // is_background.
}

}  // namespace task_management
