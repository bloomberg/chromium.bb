// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/webrtc_log_util.h"

#include <vector>

#include "base/task_scheduler/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/webrtc_logging/browser/log_cleanup.h"
#include "components/webrtc_logging/browser/log_list.h"
#include "content/public/browser/browser_thread.h"

// static
void WebRtcLogUtil::DeleteOldWebRtcLogFilesForAllProfiles() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::vector<ProfileAttributesEntry*> entries =
      g_browser_process->profile_manager()->GetProfileAttributesStorage().
          GetAllProfilesAttributes();
  for (ProfileAttributesEntry* entry : entries) {
    base::PostTaskWithTraits(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BACKGROUND},
        base::BindOnce(
            &webrtc_logging::DeleteOldWebRtcLogFiles,
            webrtc_logging::LogList::GetWebRtcLogDirectoryForBrowserContextPath(
                entry->GetPath())));
  }
}
