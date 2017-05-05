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
bool StructTraits<ash::mojom::ShelfIDDataView, ash::ShelfID>::Read(
    ash::mojom::ShelfIDDataView data,
    ash::ShelfID* out) {
  if (!data.ReadAppId(&out->app_id) ||!data.ReadLaunchId(&out->launch_id))
    return false;
  // A non-empty launch id requires a non-empty app id.
  return out->launch_id.empty() || !out->app_id.empty();
}

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
  if (!data.ReadType(&out->type) || !data.ReadImage(&image) ||
      !data.ReadStatus(&out->status) || !data.ReadShelfId(&out->id) ||
      !data.ReadTitle(&out->title)) {
    return false;
  }
  // TODO(crbug.com/655874): Support high-dpi images via gfx::Image[Skia] mojom.
  out->image = gfx::ImageSkia::CreateFrom1xBitmap(image);
  out->shows_tooltip = data.shows_tooltip();
  out->pinned_by_policy = data.pinned_by_policy();
  return true;
}

}  // namespace mojo
