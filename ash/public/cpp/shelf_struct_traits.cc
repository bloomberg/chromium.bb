// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/shelf_struct_traits.h"

#include "mojo/common/common_custom_types_struct_traits.h"
#include "skia/public/interfaces/bitmap_skbitmap_struct_traits.h"
#include "ui/gfx/image/image_skia.h"

namespace mojo {

using ShelfItemStructTraits =
    StructTraits<ash::mojom::ShelfItemDataView, ash::ShelfItem>;

// static
const SkBitmap& ShelfItemStructTraits::image(const ash::ShelfItem& i) {
  // TODO(crbug.com/655874): Remove this when we have a gfx::Image[Skia] mojom.
  static const SkBitmap kNullSkBitmap;
  return i.image.isNull() ? kNullSkBitmap : *i.image.bitmap();
}

// static
bool ShelfItemStructTraits::Read(ash::mojom::ShelfItemDataView data,
                                 ash::ShelfItem* out) {
  SkBitmap image;
  std::string app_id;
  std::string launch_id;
  if (!data.ReadType(&out->type) || !data.ReadImage(&image) ||
      !data.ReadStatus(&out->status) || !data.ReadAppId(&app_id) ||
      !data.ReadLaunchId(&launch_id) || !data.ReadTitle(&out->title)) {
    return false;
  }
  out->id = data.shelf_id();
  out->app_launch_id = ash::AppLaunchId(app_id, launch_id);
  // TODO(crbug.com/655874): Support high-dpi images via gfx::Image[Skia] mojom.
  out->image = gfx::ImageSkia::CreateFrom1xBitmap(image);
  out->shows_tooltip = data.shows_tooltip();
  out->pinned_by_policy = data.pinned_by_policy();
  return true;
}

}  // namespace mojo
