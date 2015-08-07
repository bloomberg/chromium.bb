// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_management/providers/web_contents/extension_task.h"

#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/view_type.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace task_management {

namespace {

gfx::ImageSkia* g_default_icon = nullptr;

gfx::ImageSkia* GetDefaultIcon() {
  if (!ResourceBundle::HasSharedInstance())
    return nullptr;

  if (!g_default_icon) {
    g_default_icon = ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
        IDR_EXTENSIONS_FAVICON);
  }

  return g_default_icon;
}

}  // namespace

ExtensionTask::ExtensionTask(content::WebContents* web_contents,
                             const extensions::Extension* extension,
                             extensions::ViewType view_type)
    : RendererTask(GetExtensionTitle(web_contents, extension, view_type),
                   GetDefaultIcon(),
                   web_contents) {
}

ExtensionTask::~ExtensionTask() {
}

void ExtensionTask::OnTitleChanged(content::NavigationEntry* entry) {
  // The title of the extension should not change as a result of title change
  // in its WebContents, so we ignore this.
}

void ExtensionTask::OnFaviconChanged() {
  // For now we never change the favicon of the extension, we always use the
  // default one.
  // TODO(afakhry): In the future use the extensions' favicons.
}

Task::Type ExtensionTask::GetType() const {
  return Task::EXTENSION;
}

base::string16 ExtensionTask::GetExtensionTitle(
    content::WebContents* web_contents,
    const extensions::Extension* extension,
    extensions::ViewType view_type) const {
  DCHECK(web_contents);

  base::string16 title = extension ?
      base::UTF8ToUTF16(extension->name()) :
      RendererTask::GetTitleFromWebContents(web_contents);

  bool is_background =
      view_type == extensions::VIEW_TYPE_EXTENSION_BACKGROUND_PAGE;

  return RendererTask::PrefixRendererTitle(
      title,
      extension->is_app(),
      true,  // is_extension
      web_contents->GetBrowserContext()->IsOffTheRecord(),
      is_background);
}

}  // namespace task_management
