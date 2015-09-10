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
#include "ui/base/l10n/l10n_util.h"

namespace task_management {

namespace {

base::string16 AdjustTitle(const content::SiteInstance* site_instance) {
  DCHECK(site_instance);
  int message_id = site_instance->GetBrowserContext()->IsOffTheRecord() ?
      IDS_TASK_MANAGER_SUBFRAME_INCOGNITO_PREFIX :
      IDS_TASK_MANAGER_SUBFRAME_PREFIX;

  return l10n_util::GetStringFUTF16(message_id, base::UTF8ToUTF16(
      site_instance->GetSiteURL().spec()));
}

}  // namespace

SubframeTask::SubframeTask(content::RenderFrameHost* render_frame_host,
                           content::WebContents* web_contents)
    : RendererTask(AdjustTitle(render_frame_host->GetSiteInstance()),
                   nullptr,
                   web_contents,
                   render_frame_host->GetProcess()) {
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

}  // namespace task_management
