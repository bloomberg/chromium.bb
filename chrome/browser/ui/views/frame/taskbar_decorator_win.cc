// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/taskbar_decorator_win.h"

#include <objbase.h>
#include <shobjidl.h>
#include <wrl/client.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/task/post_task.h"
#include "base/win/scoped_gdi_object.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "skia/ext/image_operations.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkFont.h"
#include "third_party/skia/include/core/SkImageEncoder.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "third_party/skia/include/core/SkStream.h"
#include "ui/gfx/icon_util.h"
#include "ui/gfx/image/image.h"
#include "ui/views/win/hwnd_util.h"

namespace chrome {

namespace {

constexpr int kOverlayIconSize = 16;
static const SkRRect kOverlayIconClip =
    SkRRect::MakeOval(SkRect::MakeWH(kOverlayIconSize, kOverlayIconSize));

// Responsible for invoking TaskbarList::SetOverlayIcon(). The call to
// TaskbarList::SetOverlayIcon() runs a nested run loop that proves
// problematic when called on the UI thread. Additionally it seems the call may
// take a while to complete. For this reason we call it on a worker thread.
//
// Docs for TaskbarList::SetOverlayIcon() say it does nothing if the HWND is not
// valid.
void SetOverlayIcon(HWND hwnd, std::unique_ptr<SkBitmap> bitmap) {
  Microsoft::WRL::ComPtr<ITaskbarList3> taskbar;
  HRESULT result = ::CoCreateInstance(
      CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&taskbar));
  if (FAILED(result) || FAILED(taskbar->HrInit()))
    return;

  base::win::ScopedGDIObject<HICON> icon;
  if (bitmap) {
    DCHECK_GE(bitmap.get()->width(), bitmap.get()->height());

    // Maintain aspect ratio on resize, but prefer more square.
    // (We used to round down here, but rounding up produces nicer results.)
    const int resized_height =
        std::ceilf(kOverlayIconSize * (float{bitmap.get()->height()} /
                                       float{bitmap.get()->width()}));

    DCHECK_GE(kOverlayIconSize, resized_height);
    // Since the target size is so small, we use our best resizer.
    SkBitmap sk_icon = skia::ImageOperations::Resize(
        *bitmap.get(),
        skia::ImageOperations::RESIZE_LANCZOS3,
        kOverlayIconSize, resized_height);

    // Paint the resized icon onto a 16x16 canvas otherwise Windows will badly
    // hammer it to 16x16. We'll use a circular clip to be consistent with the
    // way profile icons are rendered in the profile switcher.
    SkBitmap offscreen_bitmap;
    offscreen_bitmap.allocN32Pixels(kOverlayIconSize, kOverlayIconSize);
    SkCanvas offscreen_canvas(offscreen_bitmap);
    offscreen_canvas.clear(SK_ColorTRANSPARENT);
    offscreen_canvas.clipRRect(kOverlayIconClip, true);

    // Note: the original code used kOverlayIconSize - resized_height, but in
    // order to center the icon in the circle clip area, we're going to center
    // it in the paintable region instead, rounding up to the closest pixel to
    // avoid smearing.
    const int y_offset = std::ceilf((kOverlayIconSize - resized_height) / 2.0f);
    offscreen_canvas.drawBitmap(sk_icon, 0, y_offset);

    icon = IconUtil::CreateHICONFromSkBitmap(offscreen_bitmap);
    if (!icon.is_valid())
      return;
  }
  taskbar->SetOverlayIcon(hwnd, icon.get(), L"");
}

void PostSetOverlayIcon(HWND hwnd, std::unique_ptr<SkBitmap> bitmap) {
  base::CreateCOMSTATaskRunnerWithTraits(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE})
      ->PostTask(FROM_HERE,
                 base::BindOnce(&SetOverlayIcon, hwnd, base::Passed(&bitmap)));
}

}  // namespace

void DrawTaskbarDecorationString(gfx::NativeWindow window,
                                 const std::string& content) {
  HWND hwnd = views::HWNDForNativeWindow(window);

  // This is the color used by the Windows 10 Badge API, for platform
  // consistency.
  constexpr int kBackgroundColor = SkColorSetRGB(0x26, 0x25, 0x2D);
  constexpr int kForegroundColor = SK_ColorWHITE;
  constexpr int kRadius = kOverlayIconSize / 2;
  // The minimum gap to have between our content and the edge of the badge.
  constexpr int kMinMargin = 3;
  // The amount of space we have to render the icon.
  constexpr int kMaxBounds = kOverlayIconSize - 2 * kMinMargin;
  constexpr int kMaxTextSize = 24;  // Max size for our text.
  constexpr int kMinTextSize = 7;   // Min size for our text.

  auto badge = std::make_unique<SkBitmap>();
  badge->allocN32Pixels(kOverlayIconSize, kOverlayIconSize);

  SkCanvas canvas(*badge.get());

  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setColor(kBackgroundColor);

  canvas.clear(SK_ColorTRANSPARENT);
  canvas.drawCircle(kRadius, kRadius, kRadius, paint);

  paint.reset();
  paint.setColor(kForegroundColor);

  SkFont font;

  SkRect bounds;
  int text_size = kMaxTextSize;
  // Find the largest |text_size| larger than |kMinTextSize| in which
  // |content| fits into our 16x16px icon, with margins.
  do {
    font.setSize(text_size--);
    font.measureText(content.c_str(), content.size(), kUTF8_SkTextEncoding,
                     &bounds);
  } while (text_size >= kMinTextSize &&
           (bounds.width() > kMaxBounds || bounds.height() > kMaxBounds));

  canvas.drawSimpleText(content.c_str(), content.size(), kUTF8_SkTextEncoding,
                        kRadius - bounds.width() / 2 - bounds.x(),
                        kRadius - bounds.height() / 2 - bounds.y(), font,
                        paint);

  PostSetOverlayIcon(hwnd, std::move(badge));
}

void DrawTaskbarDecoration(gfx::NativeWindow window, const gfx::Image* image) {
  HWND hwnd = views::HWNDForNativeWindow(window);

  // SetOverlayIcon() does nothing if the window is not visible so testing here
  // avoids all the wasted effort of the image resizing.
  if (!::IsWindowVisible(hwnd))
    return;

  // Copy the image since we're going to use it on a separate thread and
  // gfx::Image isn't thread safe.
  std::unique_ptr<SkBitmap> bitmap;
  if (image) {
    bitmap.reset(new SkBitmap(
        profiles::GetAvatarIconAsSquare(*image->ToSkBitmap(), 1)));
  }

  PostSetOverlayIcon(hwnd, std::move(bitmap));
}

}  // namespace chrome
