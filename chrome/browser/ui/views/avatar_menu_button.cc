// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/avatar_menu_button.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/managed_mode/managed_mode.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/avatar_menu_model.h"
#include "chrome/browser/profiles/profile_info_util.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/avatar_menu_bubble_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"
#include "ui/gfx/canvas.h"
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
void DrawTaskBarDecoration(gfx::NativeWindow window, const gfx::Image* image) {
#if defined(OS_WIN) && !defined(USE_AURA)
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return;

  // SetOverlayIcon does nothing if the window is not visible so testing
  // here avoids all the wasted effort of the image resizing.
  if (!::IsWindowVisible(window))
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
      // The image's size has changed. Resize what we have.
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

AvatarMenuButton::AvatarMenuButton(Browser* browser, bool incognito)
    : MenuButton(NULL, string16(), this, false),
      browser_(browser),
      incognito_(incognito),
      is_gaia_picture_(false),
      old_height_(0) {
  // In RTL mode, the avatar icon should be looking the opposite direction.
  EnableCanvasFlippingForRTLUI(true);
}

AvatarMenuButton::~AvatarMenuButton() {
}

void AvatarMenuButton::OnPaint(gfx::Canvas* canvas) {
  if (!icon_.get())
    return;

  if (old_height_ != height() || button_icon_.isNull()) {
    old_height_ = height();
    button_icon_ = *profiles::GetAvatarIconForTitleBar(
        *icon_, is_gaia_picture_, width(), height()).ToImageSkia();
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
  canvas->DrawImageInt(button_icon_, 0, 0, button_icon_.width(),
      button_icon_.height(), dst_x, dst_y, dst_width, dst_height, false);
}

bool AvatarMenuButton::HitTestRect(const gfx::Rect& rect) const {
  if (incognito_)
    return false;
  return views::MenuButton::HitTestRect(rect);
}

void AvatarMenuButton::SetAvatarIcon(const gfx::Image& icon,
                                     bool is_gaia_picture) {
  icon_.reset(new gfx::Image(icon));
  button_icon_ = gfx::ImageSkia();
  is_gaia_picture_ = is_gaia_picture;
  SchedulePaint();
}

// views::MenuButtonListener implementation
void AvatarMenuButton::OnMenuButtonClicked(views::View* source,
                                           const gfx::Point& point) {
  if (incognito_)
    return;

  if (ManagedMode::IsInManagedMode())
    ManagedMode::LeaveManagedMode();
  else
    ShowAvatarBubble();
}

void AvatarMenuButton::ShowAvatarBubble() {
  gfx::Point origin;
  views::View::ConvertPointToScreen(this, &origin);
  gfx::Rect bounds(origin, size());
  AvatarMenuBubbleView::ShowBubble(this, views::BubbleBorder::TOP_LEFT,
      views::BubbleBorder::ALIGN_ARROW_TO_MID_ANCHOR, bounds, browser_);

  ProfileMetrics::LogProfileOpenMethod(ProfileMetrics::ICON_AVATAR_BUBBLE);
}
