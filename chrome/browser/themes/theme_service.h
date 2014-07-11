// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THEMES_THEME_SERVICE_H_
#define CHROME_BROWSER_THEMES_THEME_SERVICE_H_

#include <map>
#include <set>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/base/theme_provider.h"

class CustomThemeSupplier;
class BrowserThemePack;
class ThemeSyncableService;
class Profile;

namespace base {
class FilePath;
}

namespace color_utils {
struct HSL;
}

namespace extensions {
class Extension;
}

namespace gfx {
class Image;
}

namespace theme_service_internal {
class ThemeServiceTest;
}

namespace ui {
class ResourceBundle;
}

#ifdef __OBJC__
@class NSString;
// Sent whenever the browser theme changes.  Object => NSValue wrapping the
// ThemeService that changed.
extern "C" NSString* const kBrowserThemeDidChangeNotification;
#endif  // __OBJC__

class ThemeService : public base::NonThreadSafe,
                     public content::NotificationObserver,
                     public KeyedService,
                     public ui::ThemeProvider {
 public:
  // Public constants used in ThemeService and its subclasses:
  static const char* kDefaultThemeID;

  ThemeService();
  virtual ~ThemeService();

  virtual void Init(Profile* profile);

  // Returns a cross platform image for an id.
  //
  // TODO(erg): Make this part of the ui::ThemeProvider and the main way to get
  // theme properties out of the theme provider since it's cross platform.
  virtual gfx::Image GetImageNamed(int id) const;

  // Overridden from ui::ThemeProvider:
  virtual bool UsingSystemTheme() const OVERRIDE;
  virtual gfx::ImageSkia* GetImageSkiaNamed(int id) const OVERRIDE;
  virtual SkColor GetColor(int id) const OVERRIDE;
  virtual int GetDisplayProperty(int id) const OVERRIDE;
  virtual bool ShouldUseNativeFrame() const OVERRIDE;
  virtual bool HasCustomImage(int id) const OVERRIDE;
  virtual base::RefCountedMemory* GetRawData(
      int id,
      ui::ScaleFactor scale_factor) const OVERRIDE;
#if defined(OS_MACOSX)
  virtual NSImage* GetNSImageNamed(int id) const OVERRIDE;
  virtual NSColor* GetNSImageColorNamed(int id) const OVERRIDE;
  virtual NSColor* GetNSColor(int id) const OVERRIDE;
  virtual NSColor* GetNSColorTint(int id) const OVERRIDE;
  virtual NSGradient* GetNSGradient(int id) const OVERRIDE;
#endif

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Set the current theme to the theme defined in |extension|.
  // |extension| must already be added to this profile's
  // ExtensionService.
  virtual void SetTheme(const extensions::Extension* extension);

  // Reset the theme to default.
  virtual void UseDefaultTheme();

  // Set the current theme to the system theme. On some platforms, the system
  // theme is the default theme.
  virtual void UseSystemTheme();

  // Returns true if the default theme and system theme are not the same on
  // this platform.
  virtual bool IsSystemThemeDistinctFromDefaultTheme() const;

  // Whether we're using the chrome default theme. Virtual so linux can check
  // if we're using the GTK theme.
  virtual bool UsingDefaultTheme() const;

  // Gets the id of the last installed theme. (The theme may have been further
  // locally customized.)
  virtual std::string GetThemeID() const;

  // This class needs to keep track of the number of theme infobars so that we
  // clean up unused themes.
  void OnInfobarDisplayed();

  // Decrements the number of theme infobars. If the last infobar has been
  // destroyed, uninstalls all themes that aren't the currently selected.
  void OnInfobarDestroyed();

  // Uninstall theme extensions which are no longer in use. |ignore_infobars| is
  // whether unused themes should be removed despite a theme infobar being
  // visible.
  void RemoveUnusedThemes(bool ignore_infobars);

  // Returns the syncable service for syncing theme. The returned service is
  // owned by |this| object.
  virtual ThemeSyncableService* GetThemeSyncableService() const;

  // Save the images to be written to disk, mapping file path to id.
  typedef std::map<base::FilePath, int> ImagesDiskCache;

 protected:
  // Set a custom default theme instead of the normal default theme.
  virtual void SetCustomDefaultTheme(
      scoped_refptr<CustomThemeSupplier> theme_supplier);

  // Returns true if the ThemeService should use the system theme on startup.
  virtual bool ShouldInitWithSystemTheme() const;

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
  // from ClearAllThemeData().
  virtual void FreePlatformCaches();

  Profile* profile() const { return profile_; }

  void set_ready() { ready_ = true; }

  const CustomThemeSupplier* get_theme_supplier() const {
    return theme_supplier_.get();
  }

  // True if the theme service is ready to be used.
  // TODO(pkotwicz): Add DCHECKS to the theme service's getters once
  // ThemeSource no longer uses the ThemeService when it is not ready.
  bool ready_;

 private:
  friend class theme_service_internal::ThemeServiceTest;

  // Called when the extension service is ready.
  void OnExtensionServiceReady();

  // Migrate the theme to the new theme pack schema by recreating the data pack
  // from the extension.
  void MigrateTheme();

  // Replaces the current theme supplier with a new one and calls
  // StopUsingTheme() or StartUsingTheme() as appropriate.
  void SwapThemeSupplier(scoped_refptr<CustomThemeSupplier> theme_supplier);

  // Saves the filename of the cached theme pack.
  void SavePackName(const base::FilePath& pack_path);

  // Save the id of the last theme installed.
  void SaveThemeID(const std::string& id);

  // Implementation of SetTheme() (and the fallback from LoadThemePrefs() in
  // case we don't have a theme pack).
  void BuildFromExtension(const extensions::Extension* extension);

  // Returns true if the profile belongs to a supervised user.
  bool IsSupervisedUser() const;

  // Sets the current theme to the supervised user theme. Should only be used
  // for supervised user profiles.
  void SetSupervisedUserTheme();

#if defined(OS_MACOSX)
  // |nsimage_cache_| retains the images it has cached.
  typedef std::map<int, NSImage*> NSImageMap;
  mutable NSImageMap nsimage_cache_;

  // |nscolor_cache_| retains the colors it has cached.
  typedef std::map<int, NSColor*> NSColorMap;
  mutable NSColorMap nscolor_cache_;

  typedef std::map<int, NSGradient*> NSGradientMap;
  mutable NSGradientMap nsgradient_cache_;
#endif

  ui::ResourceBundle& rb_;
  Profile* profile_;

  scoped_refptr<CustomThemeSupplier> theme_supplier_;

  // The id of the theme extension which has just been installed but has not
  // been loaded yet. The theme extension with |installed_pending_load_id_| may
  // never be loaded if the install is due to updating a disabled theme.
  // |pending_install_id_| should be set to |kDefaultThemeID| if there are no
  // recently installed theme extensions
  std::string installed_pending_load_id_;

  // The number of infobars currently displayed.
  int number_of_infobars_;

  content::NotificationRegistrar registrar_;

  scoped_ptr<ThemeSyncableService> theme_syncable_service_;

  base::WeakPtrFactory<ThemeService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ThemeService);
};

#endif  // CHROME_BROWSER_THEMES_THEME_SERVICE_H_
