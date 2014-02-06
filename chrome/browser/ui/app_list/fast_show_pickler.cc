// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/fast_show_pickler.h"

#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/app_list/app_list_item.h"
#include "ui/gfx/image/image_skia_rep.h"

namespace {

using app_list::AppListItem;
using app_list::AppListModel;

// These have the same meaning as SkBitmap::Config. Reproduced here to insure
// against their value changing in Skia. If the order of these changes kVersion
// should be incremented.
enum ImageFormat {
  NONE,
  A8,
  INDEX_8,
  RGB_565,
  ARGB_4444,
  ARGB_8888,
};

bool FormatToConfig(ImageFormat format, SkBitmap::Config* out) {
  switch (format) {
    case NONE:
      *out = SkBitmap::kNo_Config;
      break;
    case A8:
      *out = SkBitmap::kA8_Config;
      break;
    case INDEX_8:
      *out = SkBitmap::kIndex8_Config;
      break;
    case RGB_565:
      *out = SkBitmap::kRGB_565_Config;
      break;
    case ARGB_4444:
      *out = SkBitmap::kARGB_4444_Config;
      break;
    case ARGB_8888:
      *out = SkBitmap::kARGB_8888_Config;
      break;
    default: return false;
  }
  return true;
}

bool ConfigToFormat(SkBitmap::Config config, ImageFormat* out) {
  switch (config) {
    case SkBitmap::kNo_Config:
      *out = NONE;
      break;
    case SkBitmap::kA8_Config:
      *out = A8;
      break;
    case SkBitmap::kIndex8_Config:
      *out = INDEX_8;
      break;
    case SkBitmap::kRGB_565_Config:
      *out = RGB_565;
      break;
    case SkBitmap::kARGB_4444_Config:
      *out = ARGB_4444;
      break;
    case SkBitmap::kARGB_8888_Config:
      *out = ARGB_8888;
      break;
    default: return false;
  }
  return true;
}

bool PickleImage(Pickle* pickle, const gfx::ImageSkia& image) {
  std::vector<gfx::ImageSkiaRep> reps(image.image_reps());
  pickle->WriteInt(static_cast<int>(reps.size()));
  for (std::vector<gfx::ImageSkiaRep>::const_iterator it = reps.begin();
       it != reps.end(); ++it) {
    pickle->WriteFloat(it->scale());
    pickle->WriteInt(it->pixel_width());
    pickle->WriteInt(it->pixel_height());
    ImageFormat format = NONE;
    if (!ConfigToFormat(it->sk_bitmap().getConfig(), &format))
      return false;
    pickle->WriteInt(static_cast<int>(format));
    int size = static_cast<int>(it->sk_bitmap().getSafeSize());
    pickle->WriteInt(size);
    SkBitmap bitmap = it->sk_bitmap();
    SkAutoLockPixels lock(bitmap);
    pickle->WriteBytes(bitmap.getPixels(), size);
  }
  return true;
}

bool UnpickleImage(PickleIterator* it, gfx::ImageSkia* out) {
  int rep_count = 0;
  if (!it->ReadInt(&rep_count))
    return false;

  gfx::ImageSkia result;
  for (int i = 0; i < rep_count; ++i) {
    float scale = 0.0f;
    if (!it->ReadFloat(&scale))
      return false;

    int width = 0;
    if (!it->ReadInt(&width))
      return false;

    int height = 0;
    if (!it->ReadInt(&height))
      return false;

    int format_int = 0;
    if (!it->ReadInt(&format_int))
      return false;
    ImageFormat format = static_cast<ImageFormat>(format_int);
    SkBitmap::Config config = SkBitmap::kNo_Config;
    if (!FormatToConfig(format, &config))
      return false;

    int size = 0;
    if (!it->ReadInt(&size))
      return false;

    const char* pixels = NULL;
    if (!it->ReadBytes(&pixels, size))
      return false;

    SkBitmap bitmap;
    bitmap.setConfig(static_cast<SkBitmap::Config>(config), width, height);
    if (!bitmap.allocPixels())
      return false;
    {
      SkAutoLockPixels lock(bitmap);
      memcpy(bitmap.getPixels(), pixels, bitmap.getSize());
    }
    result.AddRepresentation(gfx::ImageSkiaRep(bitmap, scale));
  }

  *out = result;
  return true;
}

scoped_ptr<AppListItem> UnpickleAppListItem(PickleIterator* it) {
  std::string id;
  if (!it->ReadString(&id))
    return scoped_ptr<AppListItem>();
  scoped_ptr<AppListItem> result(new AppListItem(id));
  std::string title;
  if (!it->ReadString(&title))
    return scoped_ptr<AppListItem>();
  std::string full_name;
  if (!it->ReadString(&full_name))
    return scoped_ptr<AppListItem>();
  result->SetTitleAndFullName(title, full_name);
  bool has_shadow = false;
  if (!it->ReadBool(&has_shadow))
    return scoped_ptr<AppListItem>();
  gfx::ImageSkia icon;
  if (!UnpickleImage(it, &icon))
    return scoped_ptr<AppListItem>();
  result->SetIcon(icon, has_shadow);
  return result.Pass();
}

bool PickleAppListItem(Pickle* pickle, AppListItem* item) {
  if (!pickle->WriteString(item->id()))
    return false;
  if (!pickle->WriteString(item->title()))
    return false;
  if (!pickle->WriteString(item->full_name()))
    return false;
  if (!pickle->WriteBool(item->has_shadow()))
    return false;
  if (!PickleImage(pickle, item->icon()))
    return false;
  return true;
}

void CopyOverItem(AppListItem* src_item, AppListItem* dest_item) {
  dest_item->SetTitleAndFullName(src_item->title(), src_item->full_name());
  dest_item->SetIcon(src_item->icon(), src_item->has_shadow());
  // Do not set folder_id, pass that to AppListModel::AddItemToFolder() instead.
}

}  // namespace

// The version of the pickle format defined here. This needs to be incremented
// whenever this format is changed so new clients can invalidate old versions.
const int FastShowPickler::kVersion = 3;

scoped_ptr<Pickle> FastShowPickler::PickleAppListModelForFastShow(
    AppListModel* model) {
  scoped_ptr<Pickle> result(new Pickle);
  if (!result->WriteInt(kVersion))
    return scoped_ptr<Pickle>();
  if (!result->WriteInt((int) model->item_list()->item_count()))
    return scoped_ptr<Pickle>();
  for (size_t i = 0; i < model->item_list()->item_count(); ++i) {
    if (!PickleAppListItem(result.get(), model->item_list()->item_at(i)))
      return scoped_ptr<Pickle>();
  }
  return result.Pass();
}

void FastShowPickler::CopyOver(AppListModel* src, AppListModel* dest) {
  DCHECK_EQ(0u, dest->item_list()->item_count());
  for (size_t i = 0; i < src->item_list()->item_count(); i++) {
    AppListItem* src_item = src->item_list()->item_at(i);
    scoped_ptr<AppListItem> dest_item(new AppListItem(src_item->id()));
    CopyOverItem(src_item, dest_item.get());
    dest->AddItemToFolder(dest_item.Pass(), src_item->folder_id());
  }
}

scoped_ptr<AppListModel>
FastShowPickler::UnpickleAppListModelForFastShow(Pickle* pickle) {
  PickleIterator it(*pickle);
  int read_version = 0;
  if (!it.ReadInt(&read_version))
    return scoped_ptr<AppListModel>();
  if (read_version != kVersion)
    return scoped_ptr<AppListModel>();
  int app_count = 0;
  if (!it.ReadInt(&app_count))
    return scoped_ptr<AppListModel>();

  scoped_ptr<AppListModel> model(new AppListModel);
  for (int i = 0; i < app_count; ++i) {
    scoped_ptr<AppListItem> item(UnpickleAppListItem(&it).Pass());
    if (!item)
      return scoped_ptr<AppListModel>();
    std::string folder_id = item->folder_id();
    model->AddItemToFolder(item.Pass(), folder_id);
  }

  return model.Pass();
}
