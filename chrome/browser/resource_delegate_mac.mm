// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_delegate_mac.h"

#import <Cocoa/Cocoa.h>

#include "ui/resources/grit/ui_resources.h"
#include "ui/views/resources/grit/views_resources.h"

namespace {

const int kFolderIconReportedSizeDIP = 16;

// Returns the icon for the given |hfs_type| defined in Cocoa's IconsCore.h, as
// a gfx::Image of dimensions |width| X |height|.
gfx::Image ImageForHFSType(int hfs_type, int width, int height) {
  NSImage* icon = [[NSWorkspace sharedWorkspace]
      iconForFileType:NSFileTypeForHFSTypeCode(hfs_type)];
  DCHECK(icon);
  [icon setSize:NSMakeSize(width, height)];
  return gfx::Image([icon retain]);
}

}  // namespace

MacResourceDelegate::MacResourceDelegate() {}

MacResourceDelegate::~MacResourceDelegate() {}

base::FilePath MacResourceDelegate::GetPathForResourcePack(
    const base::FilePath& pack_path,
    ui::ScaleFactor scale_factor) {
  return pack_path;
}

base::FilePath MacResourceDelegate::GetPathForLocalePack(
    const base::FilePath& pack_path,
    const std::string& locale) {
  return pack_path;
}

gfx::Image MacResourceDelegate::GetImageNamed(int resource_id) {
  return GetNativeImageNamed(resource_id);
}

gfx::Image MacResourceDelegate::GetNativeImageNamed(int resource_id) {
  switch (resource_id) {
    // On Mac, return the same assets for the _RTL resources. Consistent with,
    // e.g., Safari.
    case IDR_FOLDER_OPEN:
    case IDR_FOLDER_OPEN_RTL:
      return ImageForHFSType(kOpenFolderIcon, kFolderIconReportedSizeDIP,
                             kFolderIconReportedSizeDIP);
    case IDR_FOLDER_CLOSED:
    case IDR_FOLDER_CLOSED_RTL:
      return ImageForHFSType(kGenericFolderIcon, kFolderIconReportedSizeDIP,
                             kFolderIconReportedSizeDIP);
  }
  return gfx::Image();
}

base::RefCountedStaticMemory* MacResourceDelegate::LoadDataResourceBytes(
    int resource_id,
    ui::ScaleFactor scale_factor) {
  return nullptr;
}

bool MacResourceDelegate::GetRawDataResource(int resource_id,
                                             ui::ScaleFactor scale_factor,
                                             base::StringPiece* value) {
  return false;
}

bool MacResourceDelegate::GetLocalizedString(int message_id,
                                             base::string16* value) {
  return false;
}
