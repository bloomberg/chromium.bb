// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_RESOURCE_BUNDLE_H_
#define APP_RESOURCE_BUNDLE_H_

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/lock.h"
#include "base/ref_counted_memory.h"
#include "base/scoped_ptr.h"
#include "base/string16.h"

#if defined(USE_BASE_DATA_PACK)
namespace base {
class DataPack;
}
#endif
#if defined(USE_X11)
typedef struct _GdkPixbuf GdkPixbuf;
#endif
namespace gfx {
class Font;
}
class SkBitmap;
typedef uint32 SkColor;
namespace base {
class StringPiece;
}

#if defined(OS_MACOSX)
#ifdef __OBJC__
@class NSImage;
#else
class NSImage;
#endif  // __OBJC__
#endif  // defined(OS_MACOSX)

// ResourceBundle is a central facility to load images and other resources,
// such as theme graphics.
// Every resource is loaded only once.
class ResourceBundle {
 public:
  // An enumeration of the various font styles used throughout Chrome.
  // The following holds true for the font sizes:
  // Small <= Base <= Medium <= MediumBold <= Large.
  enum FontStyle {
    SmallFont,
    BaseFont,
    MediumFont,
    // NOTE: depending upon the locale, this may *not* result in a bold font.
    MediumBoldFont,
    LargeFont,
  };

  // Initialize the ResourceBundle for this process.
  // NOTE: Mac ignores this and always loads up resources for the language
  // defined by the Cocoa UI (ie-NSBundle does the langange work).
  static void InitSharedInstance(const std::wstring& pref_locale);

  // Delete the ResourceBundle for this process if it exists.
  static void CleanupSharedInstance();

  // Return the global resource loader instance.
  static ResourceBundle& GetSharedInstance();

  // Gets the bitmap with the specified resource_id from the current module
  // data. Returns a pointer to a shared instance of the SkBitmap. This shared
  // bitmap is owned by the resource bundle and should not be freed.
  //
  // The bitmap is assumed to exist. This function will log in release, and
  // assert in debug mode if it does not. On failure, this will return a
  // pointer to a shared empty placeholder bitmap so it will be visible what
  // is missing.
  SkBitmap* GetBitmapNamed(int resource_id);

  // Loads the raw bytes of a data resource into |bytes|,
  // without doing any processing or interpretation of
  // the resource. Returns whether we successfully read the resource.
  RefCountedStaticMemory* LoadDataResourceBytes(int resource_id) const;

  // Return the contents of a file in a string given the resource id.
  // This will copy the data from the resource and return it as a string.
  // TODO(port): deprecate this and replace with GetRawDataResource to avoid
  // needless copying.
  std::string GetDataResource(int resource_id);

  // Like GetDataResource(), but avoids copying the resource.  Instead, it
  // returns a StringPiece which points into the actual resource in the image.
  base::StringPiece GetRawDataResource(int resource_id);

  // Get a localized string given a message id.  Returns an empty
  // string if the message_id is not found.
  string16 GetLocalizedString(int message_id);

  // Returns the font for the specified style.
  const gfx::Font& GetFont(FontStyle style);

#if defined(OS_WIN)
  // Loads and returns an icon from the app module.
  HICON LoadThemeIcon(int icon_id);

  // Loads and returns a cursor from the app module.
  HCURSOR LoadCursor(int cursor_id);
#elif defined(OS_MACOSX)
  // Wrapper for GetBitmapNamed. Converts the bitmap to an autoreleased NSImage.
  NSImage* GetNSImageNamed(int resource_id);
#elif defined(USE_X11)
  // Gets the GdkPixbuf with the specified resource_id from the main data pak
  // file. Returns a pointer to a shared instance of the GdkPixbuf.  This
  // shared GdkPixbuf is owned by the resource bundle and should not be freed.
  //
  // The bitmap is assumed to exist. This function will log in release, and
  // assert in debug mode if it does not. On failure, this will return a
  // pointer to a shared empty placeholder bitmap so it will be visible what
  // is missing.
  GdkPixbuf* GetPixbufNamed(int resource_id);

  // As above, but flips it in RTL locales.
  GdkPixbuf* GetRTLEnabledPixbufNamed(int resource_id);

 private:
  // Shared implementation for the above two functions.
  GdkPixbuf* GetPixbufImpl(int resource_id, bool rtl_enabled);

 public:
#endif

  // TODO(glen): Move these into theme provider (dialogs still depend on
  //    ResourceBundle).
  static const SkColor frame_color;
  static const SkColor frame_color_inactive;
  static const SkColor frame_color_incognito;
  static const SkColor frame_color_incognito_inactive;
  static const SkColor toolbar_color;
  static const SkColor toolbar_separator_color;

 private:
  // We define a DataHandle typedef to abstract across how data is stored
  // across platforms.
#if defined(OS_WIN)
  // Windows stores resources in DLLs, which are managed by HINSTANCE.
  typedef HINSTANCE DataHandle;
#elif defined(USE_BASE_DATA_PACK)
  // Linux uses base::DataPack.
  typedef base::DataPack* DataHandle;
#endif

  // Ctor/dtor are private, since we're a singleton.
  ResourceBundle();
  ~ResourceBundle();

  // Free skia_images_.
  void FreeImages();

  // Try to load the main resources and the locale specific strings from an
  // external data module.
  void LoadResources(const std::wstring& pref_locale);

  // Initialize all the gfx::Font members if they haven't yet been initialized.
  void LoadFontsIfNecessary();

  // Returns the full pathname of the locale file to load.  May return an empty
  // string if no locale data files are found.
  FilePath GetLocaleFilePath(const std::wstring& pref_locale);

  // Returns a handle to bytes from the resource |module|, without doing any
  // processing or interpretation of the resource. Returns whether we
  // successfully read the resource.  Caller does not own the data returned
  // through this method and must not modify the data pointed to by |bytes|.
  static RefCountedStaticMemory* LoadResourceBytes(DataHandle module,
                                                   int resource_id);

  // Creates and returns a new SkBitmap given the data file to look in and the
  // resource id.  It's up to the caller to free the returned bitmap when
  // done.
  static SkBitmap* LoadBitmap(DataHandle dll_inst, int resource_id);

  // Class level lock.  Used to protect internal data structures that may be
  // accessed from other threads (e.g., skia_images_).
  Lock lock_;

  // Handles for data sources.
  DataHandle resources_data_;
  DataHandle locale_resources_data_;

  // Cached images. The ResourceBundle caches all retrieved bitmaps and keeps
  // ownership of the pointers.
  typedef std::map<int, SkBitmap*> SkImageMap;
  SkImageMap skia_images_;
#if defined(USE_X11)
  typedef std::map<int, GdkPixbuf*> GdkPixbufMap;
  GdkPixbufMap gdk_pixbufs_;
#endif

  // The various fonts used. Cached to avoid repeated GDI creation/destruction.
  scoped_ptr<gfx::Font> base_font_;
  scoped_ptr<gfx::Font> small_font_;
  scoped_ptr<gfx::Font> medium_font_;
  scoped_ptr<gfx::Font> medium_bold_font_;
  scoped_ptr<gfx::Font> large_font_;
  scoped_ptr<gfx::Font> web_font_;

  static ResourceBundle* g_shared_instance_;

  DISALLOW_EVIL_CONSTRUCTORS(ResourceBundle);
};

#endif  // APP_RESOURCE_BUNDLE_H_
