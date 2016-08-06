// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/web_contents/printing_task.h"

#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace task_manager {

namespace {

base::string16 PrefixTitle(const base::string16& title) {
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_PRINT_PREFIX, title);
}

}  // namespace

PrintingTask::PrintingTask(content::WebContents* web_contents)
    : RendererTask(
        PrefixTitle(RendererTask::GetTitleFromWebContents(web_contents)),
        RendererTask::GetFaviconFromWebContents(web_contents),
        web_contents,
        web_contents->GetRenderProcessHost()) {
}

PrintingTask::~PrintingTask() {
}

void PrintingTask::UpdateTitle() {
  set_title(PrefixTitle(RendererTask::GetTitleFromWebContents(web_contents())));
}

void PrintingTask::UpdateFavicon() {
  const gfx::ImageSkia* icon =
      RendererTask::GetFaviconFromWebContents(web_contents());
  set_icon(icon ? *icon : gfx::ImageSkia());
}

}  // namespace task_manager
