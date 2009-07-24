// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_THEME_PROVIDER_H_
#define CHROME_BROWSER_BROWSER_THEME_PROVIDER_H_

#include <map>
#include <string>
#include <vector>

#include "app/resource_bundle.h"
#include "app/theme_provider.h"
#include "base/basictypes.h"
#include "base/non_thread_safe.h"
#include "base/ref_counted.h"
#include "skia/ext/skia_utils.h"

class Extension;
class Profile;
class DictionaryValue;

class BrowserThemeProvider : public base::RefCounted<BrowserThemeProvider>,
                             public NonThreadSafe,
                             public ThemeProvider {
 public:
  // Public constants used in BrowserThemeProvider and its subclasses:

  // Strings used by themes to identify colors for different parts of our UI.
  static const char* kColorFrame;
  static const char* kColorFrameInactive;
  static const char* kColorFrameIncognito;
  static const char* kColorFrameIncognitoInactive;
  static const char* kColorToolbar;
  static const char* kColorTabText;
  static const char* kColorBackgroundTabText;
  static const char* kColorBookmarkText;
  static const char* kColorNTPBackground;
  static const char* kColorNTPText;
  static const char* kColorNTPLink;
  static const char* kColorNTPSection;
  static const char* kColorNTPSectionText;
  static const char* kColorNTPSectionLink;
  static const char* kColorControlBackground;
  static const char* kColorButtonBackground;

  // Strings used by themes to identify tints to apply to different parts of
  // our UI. The frame tints apply to the frame color and produce the
  // COLOR_FRAME* colors.
  static const char* kTintButtons;
  static const char* kTintFrame;
  static const char* kTintFrameInactive;
  static const char* kTintFrameIncognito;
  static const char* kTintFrameIncognitoInactive;
  static const char* kTintBackgroundTab;

  // Strings used by themes to identify miscellaneous numerical properties.
  static const char* kDisplayPropertyNTPAlignment;
  static const char* kDisplayPropertyNTPTiling;
  static const char* kDisplayPropertyNTPInverseLogo;

  // Strings used in alignment properties.
  static const char* kAlignmentTop;
  static const char* kAlignmentBottom;
  static const char* kAlignmentLeft;
  static const char* kAlignmentRight;

  // Strings used in tiling properties.
  static const char* kTilingNoRepeat;
  static const char* kTilingRepeatX;
  static const char* kTilingRepeatY;
  static const char* kTilingRepeat;

  // Default colors.
  static const SkColor kDefaultColorFrame;
  static const SkColor kDefaultColorFrameInactive;
  static const SkColor kDefaultColorFrameIncognito;
  static const SkColor kDefaultColorFrameIncognitoInactive;
  static const SkColor kDefaultColorToolbar;
  static const SkColor kDefaultColorTabText;
  static const SkColor kDefaultColorBackgroundTabText;
  static const SkColor kDefaultColorBookmarkText;
  static const SkColor kDefaultColorNTPBackground;
  static const SkColor kDefaultColorNTPText;
  static const SkColor kDefaultColorNTPLink;
  static const SkColor kDefaultColorNTPSection;
  static const SkColor kDefaultColorNTPSectionText;
  static const SkColor kDefaultColorNTPSectionLink;
  static const SkColor kDefaultColorControlBackground;
  static const SkColor kDefaultColorButtonBackground;

  static const skia::HSL kDefaultTintButtons;
  static const skia::HSL kDefaultTintFrame;
  static const skia::HSL kDefaultTintFrameInactive;
  static const skia::HSL kDefaultTintFrameIncognito;
  static const skia::HSL kDefaultTintFrameIncognitoInactive;
  static const skia::HSL kDefaultTintBackgroundTab;

 public:
  BrowserThemeProvider();
  virtual ~BrowserThemeProvider();

  enum {
    COLOR_FRAME,
    COLOR_FRAME_INACTIVE,
    COLOR_FRAME_INCOGNITO,
    COLOR_FRAME_INCOGNITO_INACTIVE,
    COLOR_TOOLBAR,
    COLOR_TAB_TEXT,
    COLOR_BACKGROUND_TAB_TEXT,
    COLOR_BOOKMARK_TEXT,
    COLOR_NTP_BACKGROUND,
    COLOR_NTP_TEXT,
    COLOR_NTP_LINK,
    COLOR_NTP_SECTION,
    COLOR_NTP_SECTION_TEXT,
    COLOR_NTP_SECTION_LINK,
    COLOR_CONTROL_BACKGROUND,
    COLOR_BUTTON_BACKGROUND,
    TINT_BUTTONS,
    TINT_FRAME,
    TINT_FRAME_INACTIVE,
    TINT_FRAME_INCOGNITO,
    TINT_FRAME_INCOGNITO_INACTIVE,
    TINT_BACKGROUND_TAB,
    NTP_BACKGROUND_ALIGNMENT,
    NTP_BACKGROUND_TILING,
    NTP_LOGO_ALTERNATE
  };

  // A bitfield mask for alignments.
  typedef enum {
    ALIGN_CENTER = 0x0,
    ALIGN_LEFT = 0x1,
    ALIGN_TOP = 0x2,
    ALIGN_RIGHT = 0x4,
    ALIGN_BOTTOM = 0x8,
  } AlignmentMasks;

  // Background tiling choices.
  enum {
    NO_REPEAT = 0,
    REPEAT_X = 1,
    REPEAT_Y = 2,
    REPEAT = 3
  } Tiling;

  // ThemeProvider implementation.
  virtual void Init(Profile* profile);
  virtual SkBitmap* GetBitmapNamed(int id);
  virtual SkColor GetColor(int id);
  virtual bool GetDisplayProperty(int id, int* result);
  virtual bool ShouldUseNativeFrame();
  virtual bool HasCustomImage(int id);
#if defined(OS_LINUX) && !defined(TOOLKIT_VIEWS)
  virtual GdkPixbuf* GetPixbufNamed(int id);
  virtual GdkPixbuf* GetRTLEnabledPixbufNamed(int id);
#elif defined(OS_MACOSX)
  virtual NSImage* GetNSImageNamed(int id);
  virtual NSColor* GetNSColorTint(int id);
#endif

  // Set the current theme to the theme defined in |extension|.
  virtual void SetTheme(Extension* extension);

  // Reset the theme to default.
  virtual void UseDefaultTheme();

  // Set the current theme to the native theme. On some platforms, the native
  // theme is the default theme.
  virtual void SetNativeTheme() { UseDefaultTheme(); }

  // Convert a bitfield alignment into a string like "top left". Public so that
  // it can be used to generate CSS values. Takes a bitfield of AlignmentMasks.
  static std::string AlignmentToString(int alignment);

  // Parse alignments from something like "top left" into a bitfield of
  // AlignmentMasks
  static int StringToAlignment(const std::string &alignment);

  // Convert a tiling value into a string like "no-repeat". Public
  // so that it can be used to generate CSS values. Takes a Tiling.
  static std::string TilingToString(int tiling);

  // Parse tiling values from something like "no-repeat" into a Tiling value.
  static int StringToTiling(const std::string &tiling);

 protected:
  // Sets an individual color value.
  void SetColor(const char* id, const SkColor& color);

  // Sets an individual tint value.
  void SetTint(const char* id, const skia::HSL& tint);

  // Generate any frame colors that weren't specified.
  void GenerateFrameColors();

  // Generate any frame images that weren't specified. The resulting images
  // will be stored in our cache.
  void GenerateFrameImages();

  // Generate any tab images that weren't specified. The resulting images
  // will be stored in our cache.
  void GenerateTabImages();

  // Clears all the override fields and saves the dictionary.
  void ClearAllThemeData();

  // Load theme data from preferences.
  virtual void LoadThemePrefs();

  // Let all the browser views know that themes have changed.
  virtual void NotifyThemeChanged();

  // Loads a bitmap from the theme, which may be tinted or
  // otherwise modified, or an application default.
  virtual SkBitmap* LoadThemeBitmap(int id);

  Profile* profile() { return profile_; }

 private:
  typedef std::map<const int, std::string> ImageMap;
  typedef std::map<const std::string, SkColor> ColorMap;
  typedef std::map<const std::string, skia::HSL> TintMap;
  typedef std::map<const std::string, int> DisplayPropertyMap;

  // Reads the image data from the theme file into the specified vector. Returns
  // true on success.
  bool ReadThemeFileData(int id, std::vector<unsigned char>* raw_data);

  // Returns the string key for the given tint |id| TINT_* enum value.
  const std::string GetTintKey(int id);

  // Returns the default tint for the given tint |id| TINT_* enum value.
  skia::HSL GetDefaultTint(int id);

  // Get the specified tint - |id| is one of the TINT_* enum values.
  skia::HSL GetTint(int id);

  // Tint |bitmap| with the tint specified by |hsl_id|
  SkBitmap TintBitmap(const SkBitmap& bitmap, int hsl_id);

  // The following load data from specified dictionaries (either from
  // preferences or from an extension manifest) and update our theme
  // data appropriately.
  // Allow any ResourceBundle image to be overridden. |images| should
  // contain keys defined in ThemeResourceMap, and values as paths to
  // the images on-disk.
  void SetImageData(DictionaryValue* images,
                    FilePath images_path);
  // Set our theme colors. The keys of |colors| are any of the kColor*
  // constants, and the values are a three-item list containing 8-bit
  // RGB values.
  void SetColorData(DictionaryValue* colors);

  // Set tint data for our images and colors. The keys of |tints| are
  // any of the kTint* contstants, and the values are a three-item list
  // containing real numbers in the range 0-1 (and -1 for 'null').
  void SetTintData(DictionaryValue* tints);

  // Set miscellaneous display properties. While these can be defined as
  // strings, they are currently stored as integers.
  void SetDisplayPropertyData(DictionaryValue* display_properties);

  // Create any images that aren't pregenerated (e.g. background tab images).
  SkBitmap* GenerateBitmap(int id);

  // Save our data - when saving images we need the original dictionary
  // from the extension because it contains the text ids that we want to save.
  void SaveImageData(DictionaryValue* images);
  void SaveColorData();
  void SaveTintData();
  void SaveDisplayPropertyData();

  SkColor FindColor(const char* id, SkColor default_color);

  // Frees generated images and clears the image cache.
  void ClearCaches();

  // Clears the platform-specific caches. Do not call directly; it's called
  // from ClearCaches().
  void FreePlatformCaches();

#if defined(OS_LINUX) && !defined(TOOLKIT_VIEWS)
  // Loads an image and flips it horizontally if |rtl_enabled| is true.
  GdkPixbuf* GetPixbufImpl(int id, bool rtl_enabled);
#endif

  // Cached images. We cache all retrieved and generated bitmaps and keep
  // track of the pointers. We own these and will delete them when we're done
  // using them.
  typedef std::map<int, SkBitmap*> ImageCache;
  ImageCache image_cache_;
#if defined(OS_LINUX) && !defined(TOOLKIT_VIEWS)
  typedef std::map<int, GdkPixbuf*> GdkPixbufMap;
  GdkPixbufMap gdk_pixbufs_;
#elif defined(OS_MACOSX)
  typedef std::map<int, NSImage*> NSImageMap;
  NSImageMap nsimage_cache_;
  typedef std::map<int, NSColor*> NSColorMap;
  NSColorMap nscolor_cache_;
#endif

  ResourceBundle& rb_;
  Profile* profile_;

  ImageMap images_;
  ColorMap colors_;
  TintMap tints_;
  DisplayPropertyMap display_properties_;

  DISALLOW_COPY_AND_ASSIGN(BrowserThemeProvider);
};

#endif  // CHROME_BROWSER_BROWSER_THEME_PROVIDER_H_
