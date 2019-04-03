// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/send_tab_to_self/send_tab_to_self_desktop_util.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/send_tab_to_self/desktop_notification_handler.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/native_theme/native_theme.h"
#include "ui/resources/grit/ui_resources.h"
#include "url/gurl.h"

namespace send_tab_to_self {

void CreateNewEntry(content::WebContents* tab, Profile* profile) {
  content::NavigationEntry* navigation_entry =
      tab->GetController().GetLastCommittedEntry();

  GURL url = navigation_entry->GetURL();
  std::string title = base::UTF16ToUTF8(navigation_entry->GetTitle());
  base::Time navigation_time = navigation_entry->GetTimestamp();

  // TODO(crbug/946804) Add target device.
  std::string target_device;
  SendTabToSelfModel* model =
      SendTabToSelfSyncServiceFactory::GetForProfile(profile)
          ->GetSendTabToSelfModel();

  UMA_HISTOGRAM_BOOLEAN("SendTabToSelf.Sync.ModelLoadedInTime",
                        model->IsReady());
  if (!model->IsReady()) {
    DesktopNotificationHandler(profile).DisplayFailureMessage(url);
    return;
  }

  const SendTabToSelfEntry* entry =
      model->AddEntry(url, title, navigation_time, target_device);

  if (entry) {
    DesktopNotificationHandler(profile).DisplaySendingConfirmation(*entry);
  } else {
    DesktopNotificationHandler(profile).DisplayFailureMessage(url);
  }
}

gfx::ImageSkia* GetImageSkia() {
  const ui::NativeTheme* native_theme =
      ui::NativeTheme::GetInstanceForNativeUi();
  bool is_dark = native_theme && native_theme->SystemDarkModeEnabled();
  int resource_id = is_dark ? IDR_SEND_TAB_TO_SELF_ICON_DARK
                            : IDR_SEND_TAB_TO_SELF_ICON_LIGHT;
  return ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(resource_id);
}

void RecordSendTabToSelfClickResult(std::string context_menu,
                                    SendTabToSelfClickResult state) {
  base::UmaHistogramEnumeration(
      "SendTabToSelf." + context_menu + ".ClickResult", state);
}

}  // namespace send_tab_to_self
