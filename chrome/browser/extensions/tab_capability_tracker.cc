// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/tab_capability_tracker.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/extensions/permissions/permission_set.h"
#include "chrome/common/extensions/permissions/permissions_data.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/user_script.h"

using content::RenderProcessHost;
using content::WebContentsObserver;

namespace extensions {

TabCapabilityTracker::TabCapabilityTracker(
    content::WebContents* web_contents, Profile* profile)
    : WebContentsObserver(web_contents) {
  registrar_.Add(this,
                 chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile));
}

TabCapabilityTracker::~TabCapabilityTracker() {}

void TabCapabilityTracker::Grant(const Extension* extension) {
  if (granted_extensions_.Contains(extension->id()))
    return;
  FOR_EACH_OBSERVER(Observer, observers_, OnGranted(extension));
  granted_extensions_.Insert(extension);
}

void TabCapabilityTracker::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void TabCapabilityTracker::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void TabCapabilityTracker::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  if (details.is_in_page)
    return;
  DCHECK(details.is_main_frame);  // important: sub-frames don't get granted!
  ClearActiveExtensionsAndNotify();
}

void TabCapabilityTracker::WebContentsDestroyed(
    content::WebContents* web_contents) {
  ClearActiveExtensionsAndNotify();
}

void TabCapabilityTracker::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(type, chrome::NOTIFICATION_EXTENSION_UNLOADED);
  const Extension* extension =
      content::Details<UnloadedExtensionInfo>(details)->extension;
  // Note: don't need to tell anybody about this because it's being unloaded
  // anyway, and all state should go away. Callers should track the unload
  // state themselves if they care.
  granted_extensions_.Remove(extension->id());
}

void TabCapabilityTracker::ClearActiveExtensionsAndNotify() {
  if (granted_extensions_.is_empty())
    return;
  FOR_EACH_OBSERVER(Observer, observers_, OnRevoked(&granted_extensions_));
  granted_extensions_.Clear();
}

}  // namespace extensions
