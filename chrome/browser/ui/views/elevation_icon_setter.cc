// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/elevation_icon_setter.h"

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#include <shellapi.h>
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "ui/gfx/icon_util.h"
#include "ui/views/controls/button/label_button.h"
#endif

void AddElevationIconToButton(views::LabelButton* button) {
#if defined(OS_WIN)
  if ((base::win::GetVersion() < base::win::VERSION_VISTA) ||
      !base::win::UserAccountControlIsEnabled())
    return;

  SHSTOCKICONINFO icon_info = { sizeof(SHSTOCKICONINFO) };
  typedef HRESULT (STDAPICALLTYPE *GetStockIconInfo)(SHSTOCKICONID,
                                                     UINT,
                                                     SHSTOCKICONINFO*);
  // Even with the runtime guard above, we have to use GetProcAddress()
  // here, because otherwise the loader will try to resolve the function
  // address on startup, which will break on XP.
  GetStockIconInfo func = reinterpret_cast<GetStockIconInfo>(
      GetProcAddress(GetModuleHandle(L"shell32.dll"), "SHGetStockIconInfo"));
  // TODO(pkasting): Run on a background thread since this call spins a nested
  // message loop.
  if (FAILED((*func)(SIID_SHIELD, SHGSI_ICON | SHGSI_SMALLICON, &icon_info)))
    return;

  scoped_ptr<SkBitmap> icon(IconUtil::CreateSkBitmapFromHICON(
      icon_info.hIcon, gfx::Size(GetSystemMetrics(SM_CXSMICON),
                                 GetSystemMetrics(SM_CYSMICON))));
  DestroyIcon(icon_info.hIcon);
  if (icon) {
    button->SetImage(views::Button::STATE_NORMAL,
                     gfx::ImageSkia::CreateFrom1xBitmap(*icon));
  }
#endif
}
