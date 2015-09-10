// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_management/providers/web_contents/guest_task.h"

#include "components/guest_view/browser/guest_view_base.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace task_management {

GuestTask::GuestTask(content::WebContents* web_contents)
    : RendererTask(GetCurrentTitle(web_contents),
                   GetFaviconFromWebContents(web_contents),
                   web_contents,
                   web_contents->GetRenderProcessHost()) {
}

GuestTask::~GuestTask() {
}

void GuestTask::UpdateTitle() {
  set_title(GetCurrentTitle(web_contents()));
}

void GuestTask::UpdateFavicon() {
  const gfx::ImageSkia* icon = GetFaviconFromWebContents(web_contents());
  set_icon(icon ? *icon : gfx::ImageSkia());
}

Task::Type GuestTask::GetType() const {
  return Task::GUEST;
}

base::string16 GuestTask::GetCurrentTitle(
    content::WebContents* web_contents) const {
  DCHECK(web_contents);

  guest_view::GuestViewBase* guest =
      guest_view::GuestViewBase::FromWebContents(web_contents);

  DCHECK(guest);

  base::string16 title =
      l10n_util::GetStringFUTF16(guest->GetTaskPrefix(),
                                 RendererTask::GetTitleFromWebContents(
                                     web_contents));

  return title;
}

}  // namespace task_management

