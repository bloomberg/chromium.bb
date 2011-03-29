// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THEMES_BROWSER_THEME_PACK_H_
#define CHROME_BROWSER_THEMES_BROWSER_THEME_PACK_H_
#pragma once

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/common/extensions/extension.h"
#include "content/browser/browser_thread.h"
#include "ui/gfx/color_utils.h"

class DictionaryValue;
class FilePath;
class RefCountedMemory;
namespace ui {
class DataPack;
}

// An optimized representation of a theme, backed by a mmapped DataPack.
//
// The idea is to pre-process all images (tinting, compositing, etc) at theme
// install time, save all the PNG-ified data into an mmappable file so we don't
// suffer multiple file system access times, therefore solving two of the
// problems with the previous implementation.
//
// A note on const-ness. All public, non-static methods are const.  We do this
// because once we've constructed a BrowserThemePack through the
// BuildFromExtension() interface, we WriteToDisk() on a thread other than the
// UI thread that consumes a BrowserThemePack. There is no locking; thread
// safety between the writing thread and the UI thread is ensured by having the
// data be immutable.
//
// BrowserThemePacks are always deleted on the file thread because in the
// common case, they are backed by mmapped data and the unmmapping operation
// will trip our IO on the UI thread detector.
class BrowserThemePack : public base::RefCountedThreadSafe<
    BrowserThemePack, BrowserThread::DeleteOnFileThread> {
 public:
  // Builds the theme pack from all data from |extension|. This is often done
  // on a separate thread as it takes so long. This can fail and return NULL in
  // the case where the theme has invalid data.
  static BrowserThemePack* BuildFromExtension(const Extension* extension);

  // Builds the theme pack from a previously performed WriteToDisk(). This
  // operation should be relatively fast, as it should be an mmap() and some
  // pointer swizzling. Returns NULL on any error attempting to read |path|.
  static scoped_refptr<BrowserThemePack> BuildFromDataPack(
      FilePath path, const std::string& expected_id);

  // Builds a data pack on disk at |path| for future quick loading by
  // BuildFromDataPack(). Often (but not always) called from the file thread;
  // implementation should be threadsafe because neither thread will write to
  // |image_memory_| and the worker thread will keep a reference to prevent
  // destruction.
  bool WriteToDisk(FilePath path) const;

  // If this theme specifies data for the corresponding |id|, return true and
  // write the corresponding value to the output parameter. These functions
  // don't return the default data. These methods should only be called from
  // the UI thread. (But this isn't enforced because of unit tests).
  bool GetTint(int id, color_utils::HSL* hsl) const;
  bool GetColor(int id, SkColor* color) const;
  bool GetDisplayProperty(int id, int* result) const;

  // Returns a bitmap if we have a custom image for |id|, otherwise NULL. Note
  // that this is separate from HasCustomImage() which returns whether a custom
  // image |id| was included in the unprocessed theme and is used as a proxy
  // for making layout decisions in the interface.
  SkBitmap* GetBitmapNamed(int id) const;

  // Returns the raw PNG encoded data for IDR_THEME_NTP_*. This method is only
  // supposed to work for the NTP attribution and background resources.
  RefCountedMemory* GetRawData(int id) const;

  // Whether this theme provides an image for |id|.
  bool HasCustomImage(int id) const;

 private:
  friend struct BrowserThread::DeleteOnThread<BrowserThread::FILE>;
  friend class DeleteTask<BrowserThemePack>;
  friend class BrowserThemePackTest;

  // Cached images. We cache all retrieved and generated bitmaps and keep
  // track of the pointers. We own these and will delete them when we're done
  // using them.
  typedef std::map<int, SkBitmap*> ImageCache;

  // The raw PNG memory associated with a certain id.
  typedef std::map<int, scoped_refptr<RefCountedMemory> > RawImages;

  // The type passed to base::DataPack::WritePack.
  typedef std::map<uint32, base::StringPiece> RawDataForWriting;

  // An association between an id and the FilePath that has the image data.
  typedef std::map<int, FilePath> FilePathMap;

  // Default. Everything is empty.
  BrowserThemePack();

  virtual ~BrowserThemePack();

  // Builds a header ready to write to disk.
  void BuildHeader(const Extension* extension);

  // Transforms the JSON tint values into their final versions in the |tints_|
  // array.
  void BuildTintsFromJSON(DictionaryValue* tints_value);

  // Transforms the JSON color values into their final versions in the
  // |colors_| array and also fills in unspecified colors based on tint values.
  void BuildColorsFromJSON(DictionaryValue* color_value);

  // Implementation details of BuildColorsFromJSON().
  void ReadColorsFromJSON(DictionaryValue* colors_value,
                          std::map<int, SkColor>* temp_colors);
  void GenerateMissingColors(std::map<int, SkColor>* temp_colors);

  // Transforms the JSON display properties into |display_properties_|.
  void BuildDisplayPropertiesFromJSON(DictionaryValue* display_value);

  // Parses the image names out of an extension.
  void ParseImageNamesFromJSON(DictionaryValue* images_value,
                               const FilePath& images_path,
                               FilePathMap* file_paths) const;

  // Creates the data for |source_images_| from |file_paths|.
  void BuildSourceImagesArray(const FilePathMap& file_paths);

  // Loads the unmodified bitmaps packed in the extension to SkBitmaps. Returns
  // true if all images loaded.
  bool LoadRawBitmapsTo(const FilePathMap& file_paths,
                        ImageCache* raw_bitmaps);

  // Creates tinted and composited frame images. Source and destination is
  // |bitmaps|.
  void GenerateFrameImages(ImageCache* bitmaps) const;

  // Generates button images tinted with |button_tint| and places them in
  // processed_bitmaps.
  void GenerateTintedButtons(const color_utils::HSL& button_tint,
                             ImageCache* processed_bitmaps) const;

  // Generates the semi-transparent tab background images, putting the results
  // in |bitmaps|. Must be called after GenerateFrameImages().
  void GenerateTabBackgroundImages(ImageCache* bitmaps) const;

  // Takes all the SkBitmaps in |images|, encodes them as PNGs and places
  // them in |reencoded_images|.
  void RepackImages(const ImageCache& images,
                    RawImages* reencoded_images) const;

  // Takes all images in |source| and puts them in |destination|, freeing any
  // image already in |destination| that |source| would overwrite.
  void MergeImageCaches(const ImageCache& source,
                        ImageCache* destination) const;

  // Changes the RefCountedMemory based |images| into StringPiece data in |out|.
  void AddRawImagesTo(const RawImages& images, RawDataForWriting* out) const;

  // Retrieves the tint OR the default tint. Unlike the public interface, we
  // always need to return a reasonable tint here, instead of partially
  // querying if the tint exists.
  color_utils::HSL GetTintInternal(int id) const;

  // Data pack, if we have one.
  scoped_ptr<ui::DataPack> data_pack_;

  // All structs written to disk need to be packed; no alignment tricks here,
  // please.
#pragma pack(push,1)
  // Header that is written to disk.
  struct BrowserThemePackHeader {
    // Numeric version to make sure we're compatible in the future.
    int32 version;

    // 1 if little_endian. 0 if big_endian. On mismatch, abort load.
    int32 little_endian;

    // theme_id without NULL terminator.
    uint8 theme_id[16];
  } *header_;

  // The remaining structs represent individual entries in an array. For the
  // following three structs, BrowserThemePack will either allocate an array or
  // will point directly to mmapped data.
  struct TintEntry {
    int32 id;
    double h;
    double s;
    double l;
  } *tints_;

  struct ColorPair {
    int32 id;
    SkColor color;
  } *colors_;

  struct DisplayPropertyPair {
    int32 id;
    int32 property;
  } *display_properties_;

  // A list of included source images. A pointer to a -1 terminated array of
  // our persistent IDs.
  int* source_images_;
#pragma pack(pop)

  // References to raw PNG data. This map isn't touched when |data_pack_| is
  // non-NULL; |image_memory_| is only filled during BuildFromExtension(). Any
  // image data that needs to be written to the DataPack during WriteToDisk()
  // needs to be in |image_memory_|.
  RawImages image_memory_;

  // An immutable cache of images generated in BuildFromExtension(). When this
  // BrowserThemePack is generated from BuildFromDataPack(), this cache is
  // empty. We separate the images from the images loaded from disk so that
  // WriteToDisk()'s implementation doesn't need locks. There should be no IDs
  // in |image_memory_| that are in |prepared_images_| or vice versa.
  ImageCache prepared_images_;

  // Loaded images. These are loaded from |image_memory_| or the |data_pack_|.
  mutable ImageCache loaded_images_;

  DISALLOW_COPY_AND_ASSIGN(BrowserThemePack);
};

#endif  // CHROME_BROWSER_THEMES_BROWSER_THEME_PACK_H_
