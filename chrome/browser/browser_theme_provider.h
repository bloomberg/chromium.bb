// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_THEME_PROVIDER_H_
#define CHROME_BROWSER_BROWSER_THEME_PROVIDER_H_

#include <map>
#include <string>
#include <vector>

#include "app/gfx/color_utils.h"
#include "app/resource_bundle.h"
#include "app/theme_provider.h"
#include "base/basictypes.h"
#include "base/lock.h"
#include "base/non_thread_safe.h"
#include "base/ref_counted.h"

class Extension;
class Profile;
class DictionaryValue;
class PrefService;
class BrowserThemeProviderTest;
class BrowserThemePack;

class BrowserThemeProvider : public NonThreadSafe,
                             public ThemeProvider {
 public:
  // Public constants used in BrowserThemeProvider and its subclasses:

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

  static const char* kDefaultThemeID;

  // Returns true if the image is themeable.  Safe to call on any thread.
  static bool IsThemeableImage(int resource_id);

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
    COLOR_NTP_LINK_UNDERLINE,
    COLOR_NTP_HEADER,
    COLOR_NTP_SECTION,
    COLOR_NTP_SECTION_TEXT,
    COLOR_NTP_SECTION_LINK,
    COLOR_NTP_SECTION_LINK_UNDERLINE,
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
  typedef enum {
    NO_REPEAT = 0,
    REPEAT_X = 1,
    REPEAT_Y = 2,
    REPEAT = 3
  } Tiling;

  // ThemeProvider implementation.
  virtual void Init(Profile* profile);
  virtual SkBitmap* GetBitmapNamed(int id) const;
  virtual SkColor GetColor(int id) const;
  virtual bool GetDisplayProperty(int id, int* result) const;
  virtual bool ShouldUseNativeFrame() const;
  virtual bool HasCustomImage(int id) const;
  virtual RefCountedMemory* GetRawData(int id) const;
#if defined(OS_LINUX)
  virtual GdkPixbuf* GetPixbufNamed(int id) const;
  virtual GdkPixbuf* GetRTLEnabledPixbufNamed(int id) const;
#elif defined(OS_MACOSX)
  virtual NSImage* GetNSImageNamed(int id) const;
  virtual NSColor* GetNSColor(int id) const;
  virtual NSColor* GetNSColorTint(int id) const;
#endif

  // Set the current theme to the theme defined in |extension|.
  virtual void SetTheme(Extension* extension);

  // Reset the theme to default.
  virtual void UseDefaultTheme();

  // Set the current theme to the native theme. On some platforms, the native
  // theme is the default theme.
  virtual void SetNativeTheme() { UseDefaultTheme(); }

  // Gets the id of the last installed theme. (The theme may have been further
  // locally customized.)
  std::string GetThemeID() const;

  // Convert a bitfield alignment into a string like "top left". Public so that
  // it can be used to generate CSS values. Takes a bitfield of AlignmentMasks.
  static std::string AlignmentToString(int alignment);

  // Parse alignments from something like "top left" into a bitfield of
  // AlignmentMasks
  static int StringToAlignment(const std::string& alignment);

  // Convert a tiling value into a string like "no-repeat". Public
  // so that it can be used to generate CSS values. Takes a Tiling.
  static std::string TilingToString(int tiling);

  // Parse tiling values from something like "no-repeat" into a Tiling value.
  static int StringToTiling(const std::string& tiling);

  // Returns the default tint for the given tint |id| TINT_* enum value.
  static color_utils::HSL GetDefaultTint(int id);

  // Returns the default color for the given color |id| COLOR_* enum value.
  static SkColor GetDefaultColor(int id);

  // Save the images to be written to disk, mapping file path to id.
  typedef std::map<FilePath, int> ImagesDiskCache;

 protected:
  // Get the specified tint - |id| is one of the TINT_* enum values.
  color_utils::HSL GetTint(int id) const;

  // Clears all the override fields and saves the dictionary.
  virtual void ClearAllThemeData();

  // Load theme data from preferences.
  virtual void LoadThemePrefs();

  // Let all the browser views know that themes have changed.
  virtual void NotifyThemeChanged();

  // Clears the platform-specific caches. Do not call directly; it's called
  // from ClearCaches().
  virtual void FreePlatformCaches();

  Profile* profile() { return profile_; }

 private:
  friend class BrowserThemeProviderTest;

  // Saves the filename of the cached theme pack.
  void SavePackName(const FilePath& pack_path);

  // Save the id of the last theme installed.
  void SaveThemeID(const std::string& id);

  // Implementation of SetTheme() (and the fallback from LoadThemePrefs() in
  // case we don't have a theme pack).
  void BuildFromExtension(Extension* extension);

  // Remove preference values for themes that are no longer in use.
  void RemoveUnusedThemes();

#if defined(OS_LINUX)
  // Loads an image and flips it horizontally if |rtl_enabled| is true.
  GdkPixbuf* GetPixbufImpl(int id, bool rtl_enabled) const;
#endif

#if defined(OS_LINUX)
  typedef std::map<int, GdkPixbuf*> GdkPixbufMap;
  mutable GdkPixbufMap gdk_pixbufs_;
#elif defined(OS_MACOSX)
  typedef std::map<int, NSImage*> NSImageMap;
  mutable NSImageMap nsimage_cache_;
  typedef std::map<int, NSColor*> NSColorMap;
  mutable NSColorMap nscolor_cache_;
#endif

  ResourceBundle& rb_;
  Profile* profile_;

  scoped_refptr<BrowserThemePack> theme_pack_;

  DISALLOW_COPY_AND_ASSIGN(BrowserThemeProvider);
};

#endif  // CHROME_BROWSER_BROWSER_THEME_PROVIDER_H_
