// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/resource_bundle.h"

#include "base/data_pack.h"
#include "base/lock.h"
#include "base/logging.h"
#include "base/string_piece.h"
#include "build/build_config.h"
#include "gfx/codec/png_codec.h"
#include "gfx/font.h"
#include "third_party/skia/include/core/SkBitmap.h"

ResourceBundle* ResourceBundle::g_shared_instance_ = NULL;

/* static */
// TODO(glen): Finish moving these into theme provider (dialogs still
//    depend on these colors).
const SkColor ResourceBundle::frame_color =
     SkColorSetRGB(66, 116, 201);
const SkColor ResourceBundle::frame_color_inactive =
     SkColorSetRGB(161, 182, 228);
const SkColor ResourceBundle::frame_color_app_panel =
     SK_ColorWHITE;
const SkColor ResourceBundle::frame_color_app_panel_inactive =
     SK_ColorWHITE;
const SkColor ResourceBundle::frame_color_incognito =
     SkColorSetRGB(83, 106, 139);
const SkColor ResourceBundle::frame_color_incognito_inactive =
     SkColorSetRGB(126, 139, 156);
const SkColor ResourceBundle::toolbar_color =
     SkColorSetRGB(210, 225, 246);
const SkColor ResourceBundle::toolbar_separator_color =
     SkColorSetRGB(182, 186, 192);

/* static */
std::string ResourceBundle::InitSharedInstance(
    const std::string& pref_locale) {
  DCHECK(g_shared_instance_ == NULL) << "ResourceBundle initialized twice";
  g_shared_instance_ = new ResourceBundle();

  g_shared_instance_->LoadCommonResources();
  return g_shared_instance_->LoadLocaleResources(pref_locale);
}

/* static */
std::string ResourceBundle::ReloadSharedInstance(
    const std::string& pref_locale) {
  DCHECK(g_shared_instance_ != NULL) << "ResourceBundle not initialized";

  g_shared_instance_->UnloadLocaleResources();
  return g_shared_instance_->LoadLocaleResources(pref_locale);
}

/* static */
void ResourceBundle::AddDataPackToSharedInstance(const FilePath& path) {
  DCHECK(g_shared_instance_ != NULL) << "ResourceBundle not initialized";
  g_shared_instance_->data_packs_.push_back(new LoadedDataPack(path));
}

/* static */
void ResourceBundle::CleanupSharedInstance() {
  if (g_shared_instance_) {
    delete g_shared_instance_;
    g_shared_instance_ = NULL;
  }
}

/* static */
ResourceBundle& ResourceBundle::GetSharedInstance() {
  // Must call InitSharedInstance before this function.
  CHECK(g_shared_instance_ != NULL);
  return *g_shared_instance_;
}

ResourceBundle::ResourceBundle()
    : lock_(new Lock),
      resources_data_(NULL),
      locale_resources_data_(NULL) {
}

void ResourceBundle::FreeImages() {
  for (SkImageMap::iterator i = skia_images_.begin();
       i != skia_images_.end(); i++) {
    delete i->second;
  }
  skia_images_.clear();
}

/* static */
SkBitmap* ResourceBundle::LoadBitmap(DataHandle data_handle, int resource_id) {
  scoped_refptr<RefCountedMemory> memory(
      LoadResourceBytes(data_handle, resource_id));
  if (!memory)
    return NULL;

  SkBitmap bitmap;
  if (!gfx::PNGCodec::Decode(memory->front(), memory->size(), &bitmap)) {
    NOTREACHED() << "Unable to decode theme image resource " << resource_id;
    return NULL;
  }

  return new SkBitmap(bitmap);
}

RefCountedStaticMemory* ResourceBundle::LoadDataResourceBytes(
    int resource_id) const {
  RefCountedStaticMemory* bytes =
      LoadResourceBytes(resources_data_, resource_id);

  // Check all our additional data packs for the resources if it wasn't loaded
  // from our main source.
  for (std::vector<LoadedDataPack*>::const_iterator it = data_packs_.begin();
       !bytes && it != data_packs_.end(); ++it) {
    bytes = (*it)->GetStaticMemory(resource_id);
  }

  return bytes;
}

SkBitmap* ResourceBundle::GetBitmapNamed(int resource_id) {
  // Check to see if we already have the Skia image in the cache.
  {
    AutoLock lock_scope(*lock_);
    SkImageMap::const_iterator found = skia_images_.find(resource_id);
    if (found != skia_images_.end())
      return found->second;
  }

  scoped_ptr<SkBitmap> bitmap;

  bitmap.reset(LoadBitmap(resources_data_, resource_id));

  if (bitmap.get()) {
    // We loaded successfully.  Cache the Skia version of the bitmap.
    AutoLock lock_scope(*lock_);

    // Another thread raced us, and has already cached the skia image.
    if (skia_images_.count(resource_id))
      return skia_images_[resource_id];

    skia_images_[resource_id] = bitmap.get();
    return bitmap.release();
  }

  // We failed to retrieve the bitmap, show a debugging red square.
  {
    LOG(WARNING) << "Unable to load bitmap with id " << resource_id;
    NOTREACHED();  // Want to assert in debug mode.

    AutoLock lock_scope(*lock_);  // Guard empty_bitmap initialization.

    static SkBitmap* empty_bitmap = NULL;
    if (!empty_bitmap) {
      // The placeholder bitmap is bright red so people notice the problem.
      // This bitmap will be leaked, but this code should never be hit.
      empty_bitmap = new SkBitmap();
      empty_bitmap->setConfig(SkBitmap::kARGB_8888_Config, 32, 32);
      empty_bitmap->allocPixels();
      empty_bitmap->eraseARGB(255, 255, 0, 0);
    }
    return empty_bitmap;
  }
}

void ResourceBundle::LoadFontsIfNecessary() {
  AutoLock lock_scope(*lock_);
  if (!base_font_.get()) {
    base_font_.reset(new gfx::Font());

    bold_font_.reset(new gfx::Font());
    *bold_font_ =
        base_font_->DeriveFont(0, base_font_->GetStyle() | gfx::Font::BOLD);

    small_font_.reset(new gfx::Font());
    *small_font_ = base_font_->DeriveFont(-2);

    medium_font_.reset(new gfx::Font());
    *medium_font_ = base_font_->DeriveFont(3);

    medium_bold_font_.reset(new gfx::Font());
    *medium_bold_font_ =
        base_font_->DeriveFont(3, base_font_->GetStyle() | gfx::Font::BOLD);

    large_font_.reset(new gfx::Font());
    *large_font_ = base_font_->DeriveFont(8);
  }
}

const gfx::Font& ResourceBundle::GetFont(FontStyle style) {
  LoadFontsIfNecessary();
  switch (style) {
    case BoldFont:
      return *bold_font_;
    case SmallFont:
      return *small_font_;
    case MediumFont:
      return *medium_font_;
    case MediumBoldFont:
      return *medium_bold_font_;
    case LargeFont:
      return *large_font_;
    default:
      return *base_font_;
  }
}

// LoadedDataPack implementation
ResourceBundle::LoadedDataPack::LoadedDataPack(const FilePath& path)
    : path_(path) {
  // Always preload the data packs so we can maintain constness.
  Load();
}

ResourceBundle::LoadedDataPack::~LoadedDataPack() {
}

void ResourceBundle::LoadedDataPack::Load() {
  DCHECK(!data_pack_.get());
  data_pack_.reset(new base::DataPack);
  bool success = data_pack_->Load(path_);
  CHECK(success) << "Failed to load " << path_.value();
}

bool ResourceBundle::LoadedDataPack::GetStringPiece(
    int resource_id, base::StringPiece* data) const {
  return data_pack_->GetStringPiece(static_cast<uint32>(resource_id), data);
}

RefCountedStaticMemory* ResourceBundle::LoadedDataPack::GetStaticMemory(
    int resource_id) const {
  return data_pack_->GetStaticMemory(resource_id);
}
