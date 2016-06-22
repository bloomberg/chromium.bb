// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_management/providers/web_contents/subframe_task.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "ui/base/l10n/l10n_util.h"

namespace task_management {

namespace {

base::string16 AdjustTitle(const content::SiteInstance* site_instance) {
  DCHECK(site_instance);

  // By default, subframe rows display the site, like this:
  //     "Subframe: http://example.com/"
  const GURL& site_url = site_instance->GetSiteURL();
  std::string name = site_url.spec();

  // If |site_url| wraps a chrome extension id, we can display the extension
  // name instead, which is more human-readable.
  if (site_url.SchemeIs(extensions::kExtensionScheme)) {
    const extensions::Extension* extension =
        extensions::ExtensionRegistry::Get(site_instance->GetBrowserContext())
            ->enabled_extensions()
            .GetExtensionOrAppByURL(site_url);
    if (extension)
      name = extension->name();
  }

  int message_id = site_instance->GetBrowserContext()->IsOffTheRecord() ?
      IDS_TASK_MANAGER_SUBFRAME_INCOGNITO_PREFIX :
      IDS_TASK_MANAGER_SUBFRAME_PREFIX;
  return l10n_util::GetStringFUTF16(message_id, base::UTF8ToUTF16(name));
}

}  // namespace

SubframeTask::SubframeTask(content::RenderFrameHost* render_frame_host,
                           content::WebContents* web_contents,
                           RendererTask* main_task)
    : RendererTask(AdjustTitle(render_frame_host->GetSiteInstance()),
                   nullptr,
                   web_contents,
                   render_frame_host->GetProcess()),
      main_task_(main_task) {
  // Note that we didn't get the RenderProcessHost from the WebContents, but
  // rather from the RenderFrameHost. Out-of-process iframes reside on
  // different processes than that of their main frame.
}

SubframeTask::~SubframeTask() {
}

void SubframeTask::UpdateTitle() {
  // This will be called when the title changes on the WebContents's main frame,
  // but this Task represents other frames, so we don't care.
}

void SubframeTask::UpdateFavicon() {
  // This will be called when the favicon changes on the WebContents's main
  // frame, but this Task represents other frames, so we don't care.
}

Task* SubframeTask::GetParentTask() const {
  return main_task_;
}

void SubframeTask::Activate() {
  // Activate the root task.
  main_task_->Activate();
}

}  // namespace task_management
