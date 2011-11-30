// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/avatar_menu_button.h"

#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_info_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/avatar_menu_bubble_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/window.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/views/widget/widget.h"


#if defined(OS_WIN)
#include <shobjidl.h>
#include "base/win/scoped_comptr.h"
#include "base/win/windows_version.h"
#include "skia/ext/image_operations.h"
#include "ui/gfx/icon_util.h"
#endif

static inline int Round(double x) {
  return static_cast<int>(x + 0.5);
}

// The Windows 7 taskbar supports dynamic overlays and effects, we use this
// to ovelay the avatar icon there. The overlay only applies if the taskbar
// is in "default large icon mode". This function is a best effort deal so
// we bail out silently at any error condition.
// See http://msdn.microsoft.com/en-us/library/dd391696(VS.85).aspx for
// more information.
void DrawTaskBarDecoration(const Browser* browser, const gfx::Image* image) {
#if defined(OS_WIN) && !defined(USE_AURA)
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return;
  BrowserWindow* bw = browser->window();
  if (!bw)
    return;
  gfx::NativeWindow window = bw->GetNativeHandle();
  if (!window)
    return;

  base::win::ScopedComPtr<ITaskbarList3> taskbar;
  HRESULT result = taskbar.CreateInstance(CLSID_TaskbarList, NULL,
                                          CLSCTX_INPROC_SERVER);
  if (FAILED(result) || FAILED(taskbar->HrInit()))
    return;
  HICON icon = NULL;
  if (image) {
    const SkBitmap* bitmap = image->ToSkBitmap();
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
      // The bitmaps size has changed. Resize what we have.
      source_bitmap = bitmap;
    }
    // Since the target size is so small, we use our best resizer. Never pass
    // windows a different size because it will badly hammer it to 16x16.
    SkBitmap sk_icon = skia::ImageOperations::Resize(
        *source_bitmap,
        skia::ImageOperations::RESIZE_LANCZOS3,
        16, 16);
    icon = IconUtil::CreateHICONFromSkBitmap(sk_icon);
    if (!icon)
      return;
  }
  taskbar->SetOverlayIcon(window, icon, L"");
  if (icon)
    DestroyIcon(icon);
#endif
}

AvatarMenuButton::AvatarMenuButton(Browser* browser, bool has_menu)
    : MenuButton(NULL, string16(), this, false),
      browser_(browser),
      has_menu_(has_menu),
      set_taskbar_decoration_(false),
      is_gaia_picture_(false),
      old_height_(0) {
  // In RTL mode, the avatar icon should be looking the opposite direction.
  EnableCanvasFlippingForRTLUI(true);
}

AvatarMenuButton::~AvatarMenuButton() {
  // During destruction of the browser frame, we might not have a window
  // so the taskbar button will be removed by windows anyway.
  if (browser_->IsAttemptingToCloseBrowser())
    return;
  DrawTaskBarDecoration(browser_, NULL);
}

void AvatarMenuButton::OnPaint(gfx::Canvas* canvas) {
  if (!icon_.get())
    return;

  if (old_height_ != height() || button_icon_.isNull()) {
    old_height_ = height();
    button_icon_ = profiles::GetAvatarIconForTitleBar(
        *icon_, is_gaia_picture_, width(), height());
  }

  // Scale the image to fit the width of the button.
  int dst_width = std::min(button_icon_.width(), width());
  // Truncate rather than rounding, so that for odd widths we put the extra
  // pixel on the left.
  int dst_x = (width() - dst_width) / 2;

  // Scale the height and maintain aspect ratio. This means that the
  // icon may not fit in the view. That's ok, we just vertically center it.
  float scale =
      static_cast<float>(dst_width) / static_cast<float>(button_icon_.width());
  // Round here so that we minimize the aspect ratio drift.
  int dst_height = Round(button_icon_.height() * scale);
  // Round rather than truncating, so that for odd heights we select an extra
  // pixel below the image center rather than above.  This is because the
  // incognito image has shadows at the top that make the apparent center below
  // the real center.
  int dst_y = Round((height() - dst_height) / 2.0);
  canvas->DrawBitmapInt(button_icon_, 0, 0, button_icon_.width(),
      button_icon_.height(), dst_x, dst_y, dst_width, dst_height, false);

  if (set_taskbar_decoration_) {
    // Drawing the taskbar decoration uses lanczos resizing so we really
    // want to do it only once.
    DrawTaskBarDecoration(browser_, icon_.get());
    set_taskbar_decoration_ = false;
  }
}

bool AvatarMenuButton::HitTest(const gfx::Point& point) const {
  if (!has_menu_)
    return false;
  return views::MenuButton::HitTest(point);
}

void AvatarMenuButton::SetIcon(const gfx::Image& icon, bool is_gaia_picture) {
  icon_.reset(new gfx::Image(icon));
  button_icon_ = SkBitmap();
  is_gaia_picture_ = is_gaia_picture;
  // If the icon changes, we need to set the taskbar decoration again.
  set_taskbar_decoration_ = true;
}

// views::ViewMenuDelegate implementation
void AvatarMenuButton::RunMenu(views::View* source, const gfx::Point& pt) {
  ShowAvatarBubble();
}

void AvatarMenuButton::ShowAvatarBubble() {
  if (!has_menu_)
    return;

  gfx::Point origin;
  views::View::ConvertPointToScreen(this, &origin);
  gfx::Rect bounds(origin, size());

  AvatarMenuBubbleView* bubble = new AvatarMenuBubbleView(this,
      views::BubbleBorder::TOP_LEFT, bounds, browser_);
  browser::CreateViewsBubble(bubble);
  bubble->Show();

  ProfileMetrics::LogProfileOpenMethod(ProfileMetrics::ICON_AVATAR_BUBBLE);
}
