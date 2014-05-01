// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/elevation_icon_setter.h"

#include "base/task_runner_util.h"
#include "content/public/browser/browser_thread.h"
#include "ui/views/controls/button/label_button.h"

#if defined(OS_WIN)
#include <windows.h>
#include <shellapi.h>
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "ui/gfx/icon_util.h"
#endif


// Helpers --------------------------------------------------------------------

namespace {

scoped_ptr<SkBitmap> GetElevationIcon() {
  scoped_ptr<SkBitmap> icon;
#if defined(OS_WIN)
  if ((base::win::GetVersion() < base::win::VERSION_VISTA) ||
      !base::win::UserAccountControlIsEnabled())
    return icon.Pass();

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
    return icon.Pass();

  icon.reset(IconUtil::CreateSkBitmapFromHICON(
      icon_info.hIcon,
      gfx::Size(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON))));
  DestroyIcon(icon_info.hIcon);
#endif
  return icon.Pass();
}

}  // namespace


// ElevationIconSetter --------------------------------------------------------

ElevationIconSetter::ElevationIconSetter(views::LabelButton* button)
    : button_(button),
      weak_factory_(this) {
  base::PostTaskAndReplyWithResult(
      content::BrowserThread::GetBlockingPool(),
      FROM_HERE,
      base::Bind(&GetElevationIcon),
      base::Bind(&ElevationIconSetter::SetButtonIcon,
                 weak_factory_.GetWeakPtr()));
}

ElevationIconSetter::~ElevationIconSetter() {
}

void ElevationIconSetter::SetButtonIcon(scoped_ptr<SkBitmap> icon) {
  if (icon) {
    button_->SetImage(views::Button::STATE_NORMAL,
                      gfx::ImageSkia::CreateFrom1xBitmap(*icon));
    button_->SizeToPreferredSize();
    if (button_->parent())
      button_->parent()->Layout();
  }
}
