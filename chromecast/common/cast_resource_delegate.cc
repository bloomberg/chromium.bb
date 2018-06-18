// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/common/cast_resource_delegate.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "ui/gfx/image/image.h"

namespace chromecast {

namespace {

CastResourceDelegate* g_instance = NULL;

}  // namespace

// static
CastResourceDelegate* CastResourceDelegate::GetInstance() {
  DCHECK(g_instance);
  return g_instance;
}

CastResourceDelegate::CastResourceDelegate() {
  DCHECK(!g_instance) << "Cannot initialize resource bundle delegate twice.";
  g_instance = this;
}

CastResourceDelegate::~CastResourceDelegate() {
  DCHECK_EQ(g_instance, this);
  g_instance = NULL;
}

base::FilePath CastResourceDelegate::GetPathForResourcePack(
    const base::FilePath& pack_path,
    ui::ScaleFactor scale_factor) {
  return pack_path;
}

base::FilePath CastResourceDelegate::GetPathForLocalePack(
    const base::FilePath& pack_path,
    const std::string& locale) {
  base::FilePath product_dir;
  if (!base::PathService::Get(base::DIR_ASSETS, &product_dir)) {
    NOTREACHED();
  }
  return product_dir.
      Append(FILE_PATH_LITERAL("chromecast_locales")).
      Append(FILE_PATH_LITERAL(locale)).
      AddExtension(FILE_PATH_LITERAL("pak"));
}

gfx::Image CastResourceDelegate::GetImageNamed(int resource_id) {
  return gfx::Image();
}

gfx::Image CastResourceDelegate::GetNativeImageNamed(int resource_id) {
  return gfx::Image();
}

base::RefCountedStaticMemory* CastResourceDelegate::LoadDataResourceBytes(
    int resource_id,
    ui::ScaleFactor scale_factor) {
  return NULL;
}

bool CastResourceDelegate::GetRawDataResource(int resource_id,
                                              ui::ScaleFactor scale_factor,
                                              base::StringPiece* value) {
  return false;
}

bool CastResourceDelegate::GetLocalizedString(int message_id,
                                              base::string16* value) {
  ExtraLocaledStringMap::const_iterator it =
      extra_localized_strings_.find(message_id);
  if (it != extra_localized_strings_.end()) {
    *value = it->second;
    return true;
  }
  return false;
}

void CastResourceDelegate::AddExtraLocalizedString(
    int resource_id,
    const base::string16& localized) {
  RemoveExtraLocalizedString(resource_id);
  extra_localized_strings_.insert(std::make_pair(resource_id, localized));
}

void CastResourceDelegate::RemoveExtraLocalizedString(int resource_id) {
  extra_localized_strings_.erase(resource_id);
}

void CastResourceDelegate::ClearAllExtraLocalizedStrings() {
  extra_localized_strings_.clear();
}

}  // namespace chromecast
