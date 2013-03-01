// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/taskbar_decorator.h"

#include <shobjidl.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/threading/worker_pool.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_comptr.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/windows_version.h"
#include "chrome/browser/profiles/profile_info_util.h"
#include "chrome/browser/ui/host_desktop.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/gfx/icon_util.h"
#include "ui/gfx/image/image.h"
#include "ui/views/win/hwnd_util.h"

namespace chrome {

namespace {

// Responsible for invoking TaskbarList::SetOverlayIcon(). The call to
// TaskbarList::SetOverlayIcon() runs a nested message loop that proves
// problematic when called on the UI thread. Additionally it seems the call may
// take a while to complete. For this reason we call it on a worker thread.
//
// Docs for TaskbarList::SetOverlayIcon() say it does nothing if the HWND is not
// valid.
void SetOverlayIcon(HWND hwnd, scoped_ptr<SkBitmap> bitmap) {
  base::win::ScopedCOMInitializer com_initializer;
  base::win::ScopedComPtr<ITaskbarList3> taskbar;
  HRESULT result = taskbar.CreateInstance(CLSID_TaskbarList, NULL,
                                          CLSCTX_INPROC_SERVER);
  if (FAILED(result) || FAILED(taskbar->HrInit()))
    return;

  base::win::ScopedGDIObject<HICON> icon;
  if (bitmap.get()) {
    const SkBitmap* source_bitmap = NULL;
    SkBitmap squarer_bitmap;
    if ((bitmap->width() == profiles::kAvatarIconWidth) &&
        (bitmap->height() == profiles::kAvatarIconHeight)) {
      // Shave a couple of columns so the bitmap is more square. So when
      // resized to a square aspect ratio it looks pretty.
      int x = 2;
      bitmap->extractSubset(&squarer_bitmap, SkIRect::MakeXYWH(x, 0,
          profiles::kAvatarIconWidth - x * 2, profiles::kAvatarIconHeight));
      source_bitmap = &squarer_bitmap;
    } else {
      // The image's size has changed. Resize what we have.
      source_bitmap = bitmap.get();
    }
    // Since the target size is so small, we use our best resizer. Never pass
    // windows a different size because it will badly hammer it to 16x16.
    SkBitmap sk_icon = skia::ImageOperations::Resize(
        *source_bitmap,
        skia::ImageOperations::RESIZE_LANCZOS3,
        16, 16);
    icon.Set(IconUtil::CreateHICONFromSkBitmap(sk_icon));
    if (!icon.Get())
      return;
  }
  taskbar->SetOverlayIcon(hwnd, icon, L"");
}

}  // namespace

void DrawTaskbarDecoration(gfx::NativeWindow window, const gfx::Image* image) {
  // HOST_DESKTOP_TYPE_ASH doesn't use the taskbar.
  if (base::win::GetVersion() < base::win::VERSION_WIN7 ||
      chrome::GetHostDesktopTypeForNativeWindow(window) !=
      chrome::HOST_DESKTOP_TYPE_NATIVE)
    return;

  HWND hwnd = views::HWNDForNativeWindow(window);

  // SetOverlayIcon() does nothing if the window is not visible so testing here
  // avoids all the wasted effort of the image resizing.
  if (!::IsWindowVisible(hwnd))
    return;

  // Copy the image since we're going to use it on a separate thread and
  // gfx::Image isn't thread safe.
  scoped_ptr<SkBitmap> bitmap(
      image ? new SkBitmap(*image->ToSkBitmap()) : NULL);
  // TaskbarList::SetOverlayIcon() may take a while, so we use slow here.
  base::WorkerPool::PostTask(
      FROM_HERE, base::Bind(&SetOverlayIcon, hwnd, Passed(&bitmap)), true);
}

}  // namespace chrome
