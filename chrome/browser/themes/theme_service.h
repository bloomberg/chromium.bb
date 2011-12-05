// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THEMES_THEME_SERVICE_H_
#define CHROME_BROWSER_THEMES_THEME_SERVICE_H_
#pragma once

#include <map>
#include <set>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/base/theme_provider.h"

class BrowserThemePack;
class ThemeServiceTest;
class Extension;
class FilePath;
class Profile;

namespace color_utils {
struct HSL;
}

namespace gfx {
class Image;
}

namespace ui {
class ResourceBundle;
}
using ui::ResourceBundle;

#ifdef __OBJC__
@class NSString;
// Sent whenever the browser theme changes.  Object => NSValue wrapping the
// ThemeService that changed.
extern "C" NSString* const kBrowserThemeDidChangeNotification;
#endif  // __OBJC__

class ThemeService : public base::NonThreadSafe,
                     public content::NotificationObserver,
                     public ProfileKeyedService,
                     public ui::ThemeProvider {
 public:
  // Public constants used in ThemeService and its subclasses:

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

  ThemeService();
  virtual ~ThemeService();

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

    // These colors don't have constant default values. They are derived from
    // the runtime value of other colors.
    COLOR_NTP_SECTION_HEADER_TEXT,
    COLOR_NTP_SECTION_HEADER_TEXT_HOVER,
    COLOR_NTP_SECTION_HEADER_RULE,
    COLOR_NTP_SECTION_HEADER_RULE_LIGHT,
    COLOR_NTP_TEXT_LIGHT,

    TINT_BUTTONS,
    TINT_FRAME,
    TINT_FRAME_INACTIVE,
    TINT_FRAME_INCOGNITO,
    TINT_FRAME_INCOGNITO_INACTIVE,
    TINT_BACKGROUND_TAB,
    NTP_BACKGROUND_ALIGNMENT,
    NTP_BACKGROUND_TILING,
    NTP_LOGO_ALTERNATE
#if defined(OS_MACOSX)
    ,
    COLOR_TOOLBAR_BEZEL = 1000,
    COLOR_TOOLBAR_STROKE,
    COLOR_TOOLBAR_STROKE_INACTIVE,
    COLOR_TOOLBAR_BUTTON_STROKE,
    COLOR_TOOLBAR_BUTTON_STROKE_INACTIVE,
    GRADIENT_FRAME_INCOGNITO,
    GRADIENT_FRAME_INCOGNITO_INACTIVE,
    GRADIENT_TOOLBAR,
    GRADIENT_TOOLBAR_INACTIVE,
    GRADIENT_TOOLBAR_BUTTON,
    GRADIENT_TOOLBAR_BUTTON_INACTIVE,
    GRADIENT_TOOLBAR_BUTTON_PRESSED,
    GRADIENT_TOOLBAR_BUTTON_PRESSED_INACTIVE
#endif  // OS_MACOSX
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

  // Returns a cross platform image for an id.
  //
  // TODO(erg): Make this a virtual, exposed through ui::ThemeProvider and the
  // main way to get theme properties out of the theme provider since it's
  // cross platform.
  const gfx::Image* GetImageNamed(int id) const;

  // ui::ThemeProvider implementation.
  virtual void Init(Profile* profile) OVERRIDE;
  virtual SkBitmap* GetBitmapNamed(int id) const OVERRIDE;
  virtual SkColor GetColor(int id) const OVERRIDE;
  virtual bool GetDisplayProperty(int id, int* result) const OVERRIDE;
  virtual bool ShouldUseNativeFrame() const OVERRIDE;
  virtual bool HasCustomImage(int id) const OVERRIDE;
  virtual RefCountedMemory* GetRawData(int id) const OVERRIDE;
#if defined(OS_MACOSX)
  virtual NSImage* GetNSImageNamed(int id, bool allow_default) const OVERRIDE;
  virtual NSColor* GetNSImageColorNamed(int id,
                                        bool allow_default) const OVERRIDE;
  virtual NSColor* GetNSColor(int id, bool allow_default) const OVERRIDE;
  virtual NSColor* GetNSColorTint(int id, bool allow_default) const OVERRIDE;
  virtual NSGradient* GetNSGradient(int id) const OVERRIDE;
#elif defined(OS_POSIX) && !defined(TOOLKIT_VIEWS) && !defined(OS_ANDROID)
  // This mismatch between what this class defines and whether or not it
  // overrides ui::ThemeProvider is http://crbug.com/105040 .
  // GdkPixbufs returned by GetPixbufNamed and GetRTLEnabledPixbufNamed are
  // shared instances owned by the theme provider and should not be freed.
  virtual GdkPixbuf* GetPixbufNamed(int id) const OVERRIDE;
  virtual GdkPixbuf* GetRTLEnabledPixbufNamed(int id) const OVERRIDE;
#elif defined(TOOLKIT_USES_GTK)
  // GdkPixbufs returned by GetPixbufNamed and GetRTLEnabledPixbufNamed are
  // shared instances owned by the theme provider and should not be freed.
  virtual GdkPixbuf* GetPixbufNamed(int id) const;
  virtual GdkPixbuf* GetRTLEnabledPixbufNamed(int id) const;
#endif

  // Set the current theme to the theme defined in |extension|.
  virtual void SetTheme(const Extension* extension);

  // Reset the theme to default.
  virtual void UseDefaultTheme();

  // Set the current theme to the native theme. On some platforms, the native
  // theme is the default theme.
  virtual void SetNativeTheme();

  // Whether we're using the chrome default theme. Virtual so linux can check
  // if we're using the GTK theme.
  virtual bool UsingDefaultTheme() const;

  // Whether we're using the native theme (which may or may not be the
  // same as the default theme).
  virtual bool UsingNativeTheme() const;

  // Gets the id of the last installed theme. (The theme may have been further
  // locally customized.)
  virtual std::string GetThemeID() const;

  // This class needs to keep track of the number of theme infobars so that we
  // clean up unused themes.
  void OnInfobarDisplayed();

  // Decrements the number of theme infobars. If the last infobar has been
  // destroyed, uninstalls all themes that aren't the currently selected.
  void OnInfobarDestroyed();

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

  // Returns true and sets |result| to the requested default property, if |id|
  // is valid.
  static bool GetDefaultDisplayProperty(int id, int* result);

  // Returns the set of IDR_* resources that should be tinted.
  static const std::set<int>& GetTintableToolbarButtons();

  // Remove preference values for themes that are no longer in use.
  void RemoveUnusedThemes();

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

#if defined(OS_MACOSX)
  // Let all the browser views know that themes have changed in a platform way.
  virtual void NotifyPlatformThemeChanged();
#endif  // OS_MACOSX

  // Clears the platform-specific caches. Do not call directly; it's called
  // from ClearCaches().
  virtual void FreePlatformCaches();

  Profile* profile() { return profile_; }

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  friend class ThemeServiceTest;

  // Saves the filename of the cached theme pack.
  void SavePackName(const FilePath& pack_path);

  // Save the id of the last theme installed.
  void SaveThemeID(const std::string& id);

  // Implementation of SetTheme() (and the fallback from LoadThemePrefs() in
  // case we don't have a theme pack).
  void BuildFromExtension(const Extension* extension);

#if defined(TOOLKIT_USES_GTK)
  // Loads an image and flips it horizontally if |rtl_enabled| is true.
  GdkPixbuf* GetPixbufImpl(int id, bool rtl_enabled) const;
#endif

#if defined(TOOLKIT_USES_GTK)
  typedef std::map<int, GdkPixbuf*> GdkPixbufMap;
  mutable GdkPixbufMap gdk_pixbufs_;
#elif defined(OS_MACOSX)
  typedef std::map<int, NSImage*> NSImageMap;
  mutable NSImageMap nsimage_cache_;

  // The bool member of the pair is whether the color is a default color.
  typedef std::map<int, std::pair<NSColor*, bool> > NSColorMap;
  mutable NSColorMap nscolor_cache_;

  typedef std::map<int, NSGradient*> NSGradientMap;
  mutable NSGradientMap nsgradient_cache_;
#endif

  ResourceBundle& rb_;
  Profile* profile_;

  scoped_refptr<BrowserThemePack> theme_pack_;

  // The number of infobars currently displayed.
  int number_of_infobars_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ThemeService);
};

#endif  // CHROME_BROWSER_THEMES_THEME_SERVICE_H_
