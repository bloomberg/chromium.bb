// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/message_center/web_notification_tray.h"

#include <windows.h>
#include "base/win/windows_version.h"
#include "chrome/browser/app_icon_win.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/common/url_constants.h"
#include "grit/chromium_strings.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

namespace message_center {
void WebNotificationTray::OnBalloonClicked() {
  chrome::ScopedTabbedBrowserDisplayer displayer(
      ProfileManager::GetLastUsedProfileAllowedByPolicy(),
      chrome::GetActiveDesktop());
  chrome::ShowSingletonTab(displayer.browser(),
                           GURL(chrome::kNotificationsHelpURL));
}

void WebNotificationTray::DisplayFirstRunBalloon() {
  // We should never be calling DisplayFirstRunBalloon without a status icon.
  // The status icon must have been created before this call.
  DCHECK(status_icon_);

  base::win::Version win_version = base::win::GetVersion();
  if (win_version == base::win::VERSION_PRE_XP)
    return;

  // StatusIconWin uses NIIF_LARGE_ICON if the version is >= vista.  According
  // to http://msdn.microsoft.com/en-us/library/windows/desktop/bb773352.aspx:
  // This corresponds to the icon with dimensions SM_CXICON x SM_CYICON. If
  // this flag is not set, the icon with dimensions SM_CXSMICON x SM_CYSMICON
  // is used.
  int icon_size = GetSystemMetrics(SM_CXICON);
  if (win_version < base::win::VERSION_VISTA)
    icon_size = GetSystemMetrics(SM_CXSMICON);

  scoped_ptr<SkBitmap> sized_app_icon_bitmap = GetAppIconForSize(icon_size);
  gfx::ImageSkia sized_app_icon_skia =
      gfx::ImageSkia::CreateFrom1xBitmap(*sized_app_icon_bitmap);
  status_icon_->DisplayBalloon(
      sized_app_icon_skia,
      l10n_util::GetStringUTF16(IDS_MESSAGE_CENTER_BALLOON_TITLE),
      l10n_util::GetStringUTF16(IDS_MESSAGE_CENTER_BALLOON_TEXT));
}

void WebNotificationTray::EnforceStatusIconVisible() {
  DCHECK(status_icon_);
  status_icon_->ForceVisible();
}

}  // namespace message_center
