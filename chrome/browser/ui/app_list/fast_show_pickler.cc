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

bool FormatToColorType(ImageFormat format, SkColorType* out) {
  switch (format) {
    case NONE:
      *out = kUnknown_SkColorType;
      break;
    case A8:
      *out = kAlpha_8_SkColorType;
      break;
    case INDEX_8:
      *out = kIndex_8_SkColorType;
      break;
    case RGB_565:
      *out = kRGB_565_SkColorType;
      break;
    case ARGB_4444:
      *out = kARGB_4444_SkColorType;
      break;
    case ARGB_8888:
      *out = kN32_SkColorType;
      break;
    default: return false;
  }
  return true;
}

bool ColorTypeToFormat(SkColorType colorType, ImageFormat* out) {
  switch (colorType) {
    case kUnknown_SkColorType:
      *out = NONE;
      break;
    case kAlpha_8_SkColorType:
      *out = A8;
      break;
    case kIndex_8_SkColorType:
      *out = INDEX_8;
      break;
    case kRGB_565_SkColorType:
      *out = RGB_565;
      break;
    case kARGB_4444_SkColorType:
      *out = ARGB_4444;
      break;
    case kN32_SkColorType:
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
    if (!ColorTypeToFormat(it->sk_bitmap().colorType(), &format))
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
    SkColorType color_type = kUnknown_SkColorType;
    if (!FormatToColorType(format, &color_type))
      return false;

    int size = 0;
    if (!it->ReadInt(&size))
      return false;

    const char* pixels = NULL;
    if (!it->ReadBytes(&pixels, size))
      return false;

    SkBitmap bitmap;
    if (!bitmap.tryAllocPixels(SkImageInfo::Make(
        width, height, color_type, kPremul_SkAlphaType)))
      return false;

    memcpy(bitmap.getPixels(), pixels, bitmap.getSize());
    result.AddRepresentation(gfx::ImageSkiaRep(bitmap, scale));
  }

  *out = result;
  return true;
}

}  // namespace

scoped_ptr<AppListItem> FastShowPickler::UnpickleAppListItem(
    PickleIterator* it) {
  std::string id;
  if (!it->ReadString(&id))
    return scoped_ptr<AppListItem>();
  scoped_ptr<AppListItem> result(new AppListItem(id));
  std::string name;
  if (!it->ReadString(&name))
    return scoped_ptr<AppListItem>();
  std::string short_name;
  if (!it->ReadString(&short_name))
    return scoped_ptr<AppListItem>();
  result->SetNameAndShortName(name, short_name);
  bool has_shadow = false;
  if (!it->ReadBool(&has_shadow))
    return scoped_ptr<AppListItem>();
  gfx::ImageSkia icon;
  if (!UnpickleImage(it, &icon))
    return scoped_ptr<AppListItem>();
  result->SetIcon(icon, has_shadow);
  return result.Pass();
}

bool FastShowPickler::PickleAppListItem(Pickle* pickle, AppListItem* item) {
  if (!pickle->WriteString(item->id()))
    return false;
  if (!pickle->WriteString(item->name()))
    return false;
  if (!pickle->WriteString(item->short_name()))
    return false;
  if (!pickle->WriteBool(item->has_shadow()))
    return false;
  if (!PickleImage(pickle, item->icon()))
    return false;
  return true;
}

void FastShowPickler::CopyOverItem(AppListItem* src_item,
                                   AppListItem* dest_item) {
  dest_item->SetNameAndShortName(src_item->name(), src_item->short_name());
  dest_item->SetIcon(src_item->icon(), src_item->has_shadow());
  // Do not set folder_id, pass that to AppListModel::AddItemToFolder() instead.
}

// The version of the pickle format defined here. This needs to be incremented
// whenever this format is changed so new clients can invalidate old versions.
const int FastShowPickler::kVersion = 3;

scoped_ptr<Pickle> FastShowPickler::PickleAppListModelForFastShow(
    AppListModel* model) {
  scoped_ptr<Pickle> result(new Pickle);
  if (!result->WriteInt(kVersion))
    return scoped_ptr<Pickle>();
  if (!result->WriteInt((int)model->top_level_item_list()->item_count()))
    return scoped_ptr<Pickle>();
  for (size_t i = 0; i < model->top_level_item_list()->item_count(); ++i) {
    if (!PickleAppListItem(result.get(),
                           model->top_level_item_list()->item_at(i))) {
      return scoped_ptr<Pickle>();
    }
  }
  return result.Pass();
}

void FastShowPickler::CopyOver(AppListModel* src, AppListModel* dest) {
  DCHECK_EQ(0u, dest->top_level_item_list()->item_count());
  for (size_t i = 0; i < src->top_level_item_list()->item_count(); i++) {
    AppListItem* src_item = src->top_level_item_list()->item_at(i);
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
