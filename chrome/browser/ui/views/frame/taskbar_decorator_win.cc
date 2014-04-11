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
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/ui/host_desktop.h"
#include "skia/ext/image_operations.h"
#include "skia/ext/platform_canvas.h"
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
    const size_t kOverlayIconSize = 16;
    const SkBitmap* source_bitmap = bitmap.get();

    // Maintain aspect ratio on resize. Image is assumed to be square.
    // Since the target size is so small, we use our best resizer.
    SkBitmap sk_icon = skia::ImageOperations::Resize(
        *source_bitmap,
        skia::ImageOperations::RESIZE_LANCZOS3,
        kOverlayIconSize, kOverlayIconSize);

    // Paint the resized icon onto a 16x16 canvas otherwise Windows will badly
    // hammer it to 16x16.
    SkBitmap offscreen_bitmap;
    offscreen_bitmap.allocN32Pixels(kOverlayIconSize, kOverlayIconSize);
    SkCanvas offscreen_canvas(offscreen_bitmap);
    offscreen_canvas.clear(SK_ColorTRANSPARENT);
    offscreen_canvas.drawBitmap(sk_icon, 0, 0);

    icon.Set(IconUtil::CreateHICONFromSkBitmap(offscreen_bitmap));
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
