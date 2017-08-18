// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_app_icon.h"

#include <algorithm>

#include "base/memory/ptr_util.h"
#include "chrome/browser/extensions/chrome_app_icon_delegate.h"
#include "chrome/browser/extensions/extension_util.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia_operations.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/extensions/gfx_utils.h"
#endif

namespace extensions {

namespace {

// Rounds the corners of a given image.
// TODO(khmel): avoid sub-classing CanvasImageSource.
class RoundedCornersImageSource : public gfx::CanvasImageSource {
 public:
  explicit RoundedCornersImageSource(const gfx::ImageSkia& icon)
      : gfx::CanvasImageSource(icon.size(), false), icon_(icon) {}
  ~RoundedCornersImageSource() override {}

 private:
  // gfx::CanvasImageSource overrides:
  void Draw(gfx::Canvas* canvas) override {
    // The radius used to round the app icon, based on 2 pixel per 48 pixels
    // icon size.
    const int rounding_radius =
        std::max<int>(std::round(2.0 * icon_.width() / 48.0), 1);

    canvas->DrawImageInt(icon_, 0, 0);

    cc::PaintFlags masking_flags;
    masking_flags.setBlendMode(SkBlendMode::kDstIn);
    canvas->SaveLayerWithFlags(masking_flags);

    cc::PaintFlags mask_flags;
    mask_flags.setAntiAlias(true);
    mask_flags.setColor(SK_ColorWHITE);
    canvas->DrawRoundRect(gfx::Rect(icon_.width(), icon_.height()),
                          rounding_radius, mask_flags);

    canvas->Restore();
  }

  gfx::ImageSkia icon_;

  DISALLOW_COPY_AND_ASSIGN(RoundedCornersImageSource);
};

}  // namespace

ChromeAppIcon::ChromeAppIcon(ChromeAppIconDelegate* delegate,
                             content::BrowserContext* browser_context,
                             DestroyedCallback destroyed_callback,
                             const std::string& app_id,
                             int resource_size_in_dip)
    : delegate_(delegate),
      browser_context_(browser_context),
      destroyed_callback_(std::move(destroyed_callback)),
      app_id_(app_id),
      resource_size_in_dip_(resource_size_in_dip) {
  DCHECK(delegate_);
  DCHECK(browser_context_);
  DCHECK(!destroyed_callback_.is_null());
  DCHECK_GE(resource_size_in_dip, 0);
  Reload();
}

ChromeAppIcon::~ChromeAppIcon() {
  std::move(destroyed_callback_).Run(this);
}

const Extension* ChromeAppIcon::GetExtension() {
  return ExtensionRegistry::Get(browser_context_)
      ->GetInstalledExtension(app_id_);
}

void ChromeAppIcon::Reload() {
  const Extension* extension = GetExtension();
  const gfx::ImageSkia default_icon = extension && extension->is_app()
                                          ? util::GetDefaultAppIcon()
                                          : util::GetDefaultExtensionIcon();
  icon_ = base::MakeUnique<IconImage>(
      browser_context_, extension,
      extension ? IconsInfo::GetIcons(extension) : ExtensionIconSet(),
      resource_size_in_dip_, default_icon, this);
  UpdateIcon();
}

bool ChromeAppIcon::IsValid() const {
  DCHECK(icon_);
  return icon_->is_valid();
}

void ChromeAppIcon::UpdateIcon() {
  DCHECK(icon_);

  image_skia_ = icon_->image_skia();
#if defined(OS_CHROMEOS)
  util::MaybeApplyChromeBadge(browser_context_, app_id_, &image_skia_);
#endif

  if (!util::IsAppLaunchable(app_id_, browser_context_)) {
    const color_utils::HSL shift = {-1, 0, 0.6};
    image_skia_ =
        gfx::ImageSkiaOperations::CreateHSLShiftedImage(image_skia_, shift);
  }

  const Extension* extension = GetExtension();
  if (extension && extension->from_bookmark()) {
    image_skia_ =
        gfx::ImageSkia(base::MakeUnique<RoundedCornersImageSource>(image_skia_),
                       image_skia_.size());
  }

  delegate_->OnIconUpdated(this);
}

void ChromeAppIcon::OnExtensionIconImageChanged(IconImage* icon) {
  DCHECK_EQ(icon_.get(), icon);
  UpdateIcon();
}

}  // namespace extensions
