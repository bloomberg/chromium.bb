// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/drag_download_item.h"

#include <string>

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/download_item.h"
#include "net/base/mime_util.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drag_utils.h"
#include "ui/base/dragdrop/file_info.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/public/drag_drop_client.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/drive/download_handler.h"
#endif

void DragDownloadItem(const content::DownloadItem* download,
                      gfx::Image* icon,
                      gfx::NativeView view) {
  DCHECK(download);
  DCHECK_EQ(content::DownloadItem::COMPLETE, download->GetState());

  // Set up our OLE machinery
  ui::OSExchangeData data;

  drag_utils::CreateDragImageForFile(
      download->GetFileNameToReportUser(),
      icon ? icon->AsImageSkia() : gfx::ImageSkia(),
      &data);

  base::FilePath full_path = download->GetTargetFilePath();
#if defined(OS_CHROMEOS)
  // Overwrite |full_path| with drive cache file path when appropriate.
  Profile* profile = Profile::FromBrowserContext(download->GetBrowserContext());
  drive::DownloadHandler* drive_download_handler =
      drive::DownloadHandler::GetForProfile(profile);
  if (drive_download_handler &&
      drive_download_handler->IsDriveDownload(download))
    full_path = drive_download_handler->GetCacheFilePath(download);
#endif
  std::vector<ui::FileInfo> file_infos;
  file_infos.push_back(
      ui::FileInfo(full_path, download->GetFileNameToReportUser()));
  data.SetFilenames(file_infos);

  aura::Window* root_window = view->GetRootWindow();
  if (!root_window || !aura::client::GetDragDropClient(root_window))
    return;

  gfx::Point location = display::Screen::GetScreen()->GetCursorScreenPoint();
  // TODO(varunjain): Properly determine and send DRAG_EVENT_SOURCE below.
  aura::client::GetDragDropClient(root_window)->StartDragAndDrop(
      data,
      root_window,
      view,
      location,
      ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_LINK,
      ui::DragDropTypes::DRAG_EVENT_SOURCE_MOUSE);
}
