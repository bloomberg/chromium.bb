// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_service.h"

#include "base/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/prefs/pref_service.h"
#include "base/sequenced_task_runner.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/browser_theme_pack.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_syncable_service.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/user_metrics.h"
#include "grit/theme_resources.h"
#include "grit/ui_resources.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

#if defined(OS_WIN)
#include "ui/base/win/shell.h"
#endif

#if defined(USE_AURA) && !defined(USE_ASH) && defined(OS_LINUX)
#include "ui/linux_ui/linux_ui.h"
#endif

using content::BrowserThread;
using content::UserMetricsAction;
using extensions::Extension;
using ui::ResourceBundle;

typedef ThemeProperties Properties;

// The default theme if we haven't installed a theme yet or if we've clicked
// the "Use Classic" button.
const char* ThemeService::kDefaultThemeID = "";

namespace {

// The default theme if we've gone to the theme gallery and installed the
// "Default" theme. We have to detect this case specifically. (By the time we
// realize we've installed the default theme, we already have an extension
// unpacked on the filesystem.)
const char* kDefaultThemeGalleryID = "hkacjpbfdknhflllbcmjibkdeoafencn";

SkColor TintForUnderline(SkColor input) {
  return SkColorSetA(input, SkColorGetA(input) / 3);
}

SkColor IncreaseLightness(SkColor color, double percent) {
  color_utils::HSL result;
  color_utils::SkColorToHSL(color, &result);
  result.l += (1 - result.l) * percent;
  return color_utils::HSLToSkColor(result, SkColorGetA(color));
}

// Writes the theme pack to disk on a separate thread.
void WritePackToDiskCallback(BrowserThemePack* pack,
                             const base::FilePath& path) {
  if (!pack->WriteToDisk(path))
    NOTREACHED() << "Could not write theme pack to disk";
}

}  // namespace

ThemeService::ThemeService()
    : rb_(ResourceBundle::GetSharedInstance()),
      profile_(NULL),
      number_of_infobars_(0) {
}

ThemeService::~ThemeService() {
  FreePlatformCaches();
}

void ThemeService::Init(Profile* profile) {
  DCHECK(CalledOnValidThread());
  profile_ = profile;

  LoadThemePrefs();

  theme_syncable_service_.reset(new ThemeSyncableService(profile_, this));
}

gfx::Image ThemeService::GetImageNamed(int id) const {
  DCHECK(CalledOnValidThread());

  const gfx::Image* image = NULL;

  if (theme_pack_.get())
    image = theme_pack_->GetImageNamed(id);

#if defined(USE_AURA) && !defined(USE_ASH) && defined(OS_LINUX)
  const ui::LinuxUI* linux_ui = ui::LinuxUI::instance();
  if (!image && linux_ui)
    image = linux_ui->GetThemeImageNamed(id);
#endif

  if (!image)
    image = &rb_.GetNativeImageNamed(id);

  return image ? *image : gfx::Image();
}

gfx::ImageSkia* ThemeService::GetImageSkiaNamed(int id) const {
  gfx::Image image = GetImageNamed(id);
  if (image.IsEmpty())
    return NULL;
  // TODO(pkotwicz): Remove this const cast.  The gfx::Image interface returns
  // its images const. GetImageSkiaNamed() also should but has many callsites.
  return const_cast<gfx::ImageSkia*>(image.ToImageSkia());
}

SkColor ThemeService::GetColor(int id) const {
  DCHECK(CalledOnValidThread());

  SkColor color;
  if (theme_pack_.get() && theme_pack_->GetColor(id, &color))
    return color;

#if defined(USE_AURA) && !defined(USE_ASH) && defined(OS_LINUX)
  const ui::LinuxUI* linux_ui = ui::LinuxUI::instance();
  if (linux_ui && linux_ui->GetColor(id, &color))
    return color;
#endif

  // For backward compat with older themes, some newer colors are generated from
  // older ones if they are missing.
  switch (id) {
    case Properties::COLOR_NTP_SECTION_HEADER_TEXT:
      return IncreaseLightness(GetColor(Properties::COLOR_NTP_TEXT), 0.30);
    case Properties::COLOR_NTP_SECTION_HEADER_TEXT_HOVER:
      return GetColor(Properties::COLOR_NTP_TEXT);
    case Properties::COLOR_NTP_SECTION_HEADER_RULE:
      return IncreaseLightness(GetColor(Properties::COLOR_NTP_TEXT), 0.70);
    case Properties::COLOR_NTP_SECTION_HEADER_RULE_LIGHT:
      return IncreaseLightness(GetColor(Properties::COLOR_NTP_TEXT), 0.86);
    case Properties::COLOR_NTP_TEXT_LIGHT:
      return IncreaseLightness(GetColor(Properties::COLOR_NTP_TEXT), 0.40);
  }

  return Properties::GetDefaultColor(id);
}

bool ThemeService::GetDisplayProperty(int id, int* result) const {
  if (theme_pack_.get())
    return theme_pack_->GetDisplayProperty(id, result);

  return Properties::GetDefaultDisplayProperty(id, result);
}

bool ThemeService::ShouldUseNativeFrame() const {
  if (HasCustomImage(IDR_THEME_FRAME))
    return false;
#if defined(OS_WIN)
  return ui::win::IsAeroGlassEnabled();
#else
  return false;
#endif
}

bool ThemeService::HasCustomImage(int id) const {
  if (!Properties::IsThemeableImage(id))
    return false;

  if (theme_pack_)
    return theme_pack_->HasCustomImage(id);

  return false;
}

base::RefCountedMemory* ThemeService::GetRawData(
    int id,
    ui::ScaleFactor scale_factor) const {
  // Check to see whether we should substitute some images.
  int ntp_alternate;
  GetDisplayProperty(Properties::NTP_LOGO_ALTERNATE, &ntp_alternate);
  if (id == IDR_PRODUCT_LOGO && ntp_alternate != 0)
    id = IDR_PRODUCT_LOGO_WHITE;

  base::RefCountedMemory* data = NULL;
  if (theme_pack_.get())
    data = theme_pack_->GetRawData(id, scale_factor);
  if (!data)
    data = rb_.LoadDataResourceBytesForScale(id, ui::SCALE_FACTOR_100P);

  return data;
}

void ThemeService::SetTheme(const Extension* extension) {
  // Clear our image cache.
  FreePlatformCaches();

  DCHECK(extension);
  DCHECK(extension->is_theme());
  if (DCHECK_IS_ON()) {
    ExtensionService* service =
        extensions::ExtensionSystem::Get(profile_)->extension_service();
    DCHECK(service);
    DCHECK(service->GetExtensionById(extension->id(), false));
  }

  BuildFromExtension(extension);
  SaveThemeID(extension->id());

  NotifyThemeChanged();
  content::RecordAction(UserMetricsAction("Themes_Installed"));
}

void ThemeService::RemoveUnusedThemes() {
  if (!profile_)
    return;
  ExtensionService* service = profile_->GetExtensionService();
  if (!service)
    return;
  std::string current_theme = GetThemeID();
  std::vector<std::string> remove_list;
  const ExtensionSet* extensions = service->extensions();
  for (ExtensionSet::const_iterator it = extensions->begin();
       it != extensions->end(); ++it) {
    if ((*it)->is_theme() && (*it)->id() != current_theme) {
      remove_list.push_back((*it)->id());
    }
  }
  for (size_t i = 0; i < remove_list.size(); ++i)
    service->UninstallExtension(remove_list[i], false, NULL);
}

void ThemeService::UseDefaultTheme() {
  ClearAllThemeData();
  NotifyThemeChanged();
  content::RecordAction(UserMetricsAction("Themes_Reset"));
}

void ThemeService::SetNativeTheme() {
  UseDefaultTheme();
}

bool ThemeService::UsingDefaultTheme() const {
  std::string id = GetThemeID();
  return id == ThemeService::kDefaultThemeID ||
      id == kDefaultThemeGalleryID;
}

bool ThemeService::UsingNativeTheme() const {
  return UsingDefaultTheme();
}

std::string ThemeService::GetThemeID() const {
  return profile_->GetPrefs()->GetString(prefs::kCurrentThemeID);
}

color_utils::HSL ThemeService::GetTint(int id) const {
  DCHECK(CalledOnValidThread());

  color_utils::HSL hsl;
  if (theme_pack_.get() && theme_pack_->GetTint(id, &hsl))
    return hsl;

  return ThemeProperties::GetDefaultTint(id);
}

void ThemeService::ClearAllThemeData() {
  // Clear our image cache.
  FreePlatformCaches();
  theme_pack_ = NULL;

  profile_->GetPrefs()->ClearPref(prefs::kCurrentThemePackFilename);
  SaveThemeID(kDefaultThemeID);
}

void ThemeService::LoadThemePrefs() {
  PrefService* prefs = profile_->GetPrefs();

  std::string current_id = GetThemeID();
  if (current_id != kDefaultThemeID) {
    bool loaded_pack = false;

    // If we don't have a file pack, we're updating from an old version.
    base::FilePath path = prefs->GetFilePath(prefs::kCurrentThemePackFilename);
    if (path != base::FilePath()) {
      theme_pack_ = BrowserThemePack::BuildFromDataPack(path, current_id);
      loaded_pack = theme_pack_.get() != NULL;
    }

    if (loaded_pack) {
      content::RecordAction(UserMetricsAction("Themes.Loaded"));
    } else {
      // TODO(erg): We need to pop up a dialog informing the user that their
      // theme is being migrated.
      ExtensionService* service =
          extensions::ExtensionSystem::Get(profile_)->extension_service();
      if (service) {
        const Extension* extension =
            service->GetExtensionById(current_id, false);
        if (extension) {
          DLOG(ERROR) << "Migrating theme";
          BuildFromExtension(extension);
          content::RecordAction(UserMetricsAction("Themes.Migrated"));
        } else {
          DLOG(ERROR) << "Theme is mysteriously gone.";
          ClearAllThemeData();
          content::RecordAction(UserMetricsAction("Themes.Gone"));
        }
      }
    }
  }
}

void ThemeService::NotifyThemeChanged() {
  DVLOG(1) << "Sending BROWSER_THEME_CHANGED";
  // Redraw!
  content::NotificationService* service =
      content::NotificationService::current();
  service->Notify(chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                  content::Source<ThemeService>(this),
                  content::NotificationService::NoDetails());
#if defined(OS_MACOSX)
  NotifyPlatformThemeChanged();
#endif  // OS_MACOSX

  // Notify sync that theme has changed.
  if (theme_syncable_service_.get()) {
    theme_syncable_service_->OnThemeChange();
  }
}

#if defined(OS_WIN) || defined(USE_AURA)
void ThemeService::FreePlatformCaches() {
  // Views (Skia) has no platform image cache to clear.
}
#endif

void ThemeService::SavePackName(const base::FilePath& pack_path) {
  profile_->GetPrefs()->SetFilePath(
      prefs::kCurrentThemePackFilename, pack_path);
}

void ThemeService::SaveThemeID(const std::string& id) {
  profile_->GetPrefs()->SetString(prefs::kCurrentThemeID, id);
}

void ThemeService::BuildFromExtension(const Extension* extension) {
  scoped_refptr<BrowserThemePack> pack(
      BrowserThemePack::BuildFromExtension(extension));
  if (!pack.get()) {
    // TODO(erg): We've failed to install the theme; perhaps we should tell the
    // user? http://crbug.com/34780
    LOG(ERROR) << "Could not load theme.";
    return;
  }

  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  if (!service)
    return;

  // Write the packed file to disk.
  base::FilePath pack_path =
      extension->path().Append(chrome::kThemePackFilename);
  service->GetFileTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&WritePackToDiskCallback, pack, pack_path));

  SavePackName(pack_path);
  theme_pack_ = pack;
}

void ThemeService::OnInfobarDisplayed() {
  number_of_infobars_++;
}

void ThemeService::OnInfobarDestroyed() {
  number_of_infobars_--;

  if (number_of_infobars_ == 0)
    RemoveUnusedThemes();
}

ThemeSyncableService* ThemeService::GetThemeSyncableService() const {
  return theme_syncable_service_.get();
}
