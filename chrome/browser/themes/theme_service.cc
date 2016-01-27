// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_service.h"

#include <stddef.h>

#include <algorithm>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ref_counted_memory.h"
#include "base/prefs/pref_service.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/browser_theme_pack.h"
#include "chrome/browser/themes/custom_theme_supplier.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/themes/theme_syncable_service.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/user_metrics.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "grit/components_scaled_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/layout.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/native_theme/common_theme.h"
#include "ui/native_theme/native_theme.h"

#if defined(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_registry_observer.h"
#endif

#if defined(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_theme.h"
#endif

#if defined(OS_WIN)
#include "ui/base/win/shell.h"
#endif

using base::UserMetricsAction;
using content::BrowserThread;
using extensions::Extension;
using extensions::UnloadedExtensionInfo;
using ui::ResourceBundle;

// The default theme if we haven't installed a theme yet or if we've clicked
// the "Use Classic" button.
const char ThemeService::kDefaultThemeID[] = "";

namespace {

// The default theme if we've gone to the theme gallery and installed the
// "Default" theme. We have to detect this case specifically. (By the time we
// realize we've installed the default theme, we already have an extension
// unpacked on the filesystem.)
const char kDefaultThemeGalleryID[] = "hkacjpbfdknhflllbcmjibkdeoafencn";

// Wait this many seconds after startup to garbage collect unused themes.
// Removing unused themes is done after a delay because there is no
// reason to do it at startup.
// ExtensionService::GarbageCollectExtensions() does something similar.
const int kRemoveUnusedThemesStartupDelay = 30;

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

// Heuristic to determine if color is grayscale. This is used to decide whether
// to use the colorful or white logo, if a theme fails to specify which.
bool IsColorGrayscale(SkColor color) {
  const int kChannelTolerance = 9;
  int r = SkColorGetR(color);
  int g = SkColorGetG(color);
  int b = SkColorGetB(color);
  int range = std::max(r, std::max(g, b)) - std::min(r, std::min(g, b));
  return range < kChannelTolerance;
}

}  // namespace

#if defined(ENABLE_EXTENSIONS)
class ThemeService::ThemeObserver
    : public extensions::ExtensionRegistryObserver {
 public:
  explicit ThemeObserver(ThemeService* service) : theme_service_(service) {
    extensions::ExtensionRegistry::Get(theme_service_->profile_)
        ->AddObserver(this);
  }

  ~ThemeObserver() override {
    extensions::ExtensionRegistry::Get(theme_service_->profile_)
        ->RemoveObserver(this);
  }

 private:
  void OnExtensionWillBeInstalled(content::BrowserContext* browser_context,
                                  const extensions::Extension* extension,
                                  bool is_update,
                                  const std::string& old_name) override {
    if (extension->is_theme()) {
      // The theme may be initially disabled. Wait till it is loaded (if ever).
      theme_service_->installed_pending_load_id_ = extension->id();
    }
  }

  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override {
    if (extension->is_theme() &&
        theme_service_->installed_pending_load_id_ != kDefaultThemeID &&
        theme_service_->installed_pending_load_id_ == extension->id()) {
      theme_service_->SetTheme(extension);
    }
    theme_service_->installed_pending_load_id_ = kDefaultThemeID;
  }

  void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension,
      extensions::UnloadedExtensionInfo::Reason reason) override {
    if (reason != extensions::UnloadedExtensionInfo::REASON_UPDATE &&
        reason != extensions::UnloadedExtensionInfo::REASON_LOCK_ALL &&
        extension->is_theme() &&
        extension->id() == theme_service_->GetThemeID()) {
      theme_service_->UseDefaultTheme();
    }
  }

  ThemeService* theme_service_;
};
#endif  // defined(ENABLE_EXTENSIONS)

ThemeService::ThemeService()
    : ready_(false),
      rb_(ResourceBundle::GetSharedInstance()),
      profile_(nullptr),
      installed_pending_load_id_(kDefaultThemeID),
      number_of_infobars_(0),
      original_theme_provider_(*this, false),
      otr_theme_provider_(*this, true),
      weak_ptr_factory_(this) {}

ThemeService::~ThemeService() {
  FreePlatformCaches();
}

void ThemeService::Init(Profile* profile) {
  DCHECK(CalledOnValidThread());
  profile_ = profile;

  LoadThemePrefs();

  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSIONS_READY_DEPRECATED,
                 content::Source<Profile>(profile_));

  theme_syncable_service_.reset(new ThemeSyncableService(profile_, this));
}

bool ThemeService::IsSystemThemeDistinctFromDefaultTheme() const {
  return false;
}

void ThemeService::Shutdown() {
#if defined(ENABLE_EXTENSIONS)
  theme_observer_.reset();
#endif
}

void ThemeService::Observe(int type,
                           const content::NotificationSource& source,
                           const content::NotificationDetails& details) {
  using content::Details;
  switch (type) {
    case extensions::NOTIFICATION_EXTENSIONS_READY_DEPRECATED:
      registrar_.Remove(this,
                        extensions::NOTIFICATION_EXTENSIONS_READY_DEPRECATED,
                        content::Source<Profile>(profile_));
      OnExtensionServiceReady();
      break;
    case extensions::NOTIFICATION_EXTENSION_ENABLED: {
      const Extension* extension = Details<const Extension>(details).ptr();
      if (extension->is_theme())
        SetTheme(extension);
      break;
    }
    default:
      NOTREACHED();
  }
}

void ThemeService::SetTheme(const Extension* extension) {
  DCHECK(extension->is_theme());
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  if (!service->IsExtensionEnabled(extension->id())) {
    // |extension| is disabled when reverting to the previous theme via an
    // infobar.
    service->EnableExtension(extension->id());
    // Enabling the extension will call back to SetTheme().
    return;
  }

  std::string previous_theme_id = GetThemeID();

  // Clear our image cache.
  FreePlatformCaches();

  BuildFromExtension(extension);
  SaveThemeID(extension->id());

  NotifyThemeChanged();
  content::RecordAction(UserMetricsAction("Themes_Installed"));

  if (previous_theme_id != kDefaultThemeID &&
      previous_theme_id != extension->id() &&
      service->GetInstalledExtension(previous_theme_id)) {
    // Do not disable the previous theme if it is already uninstalled. Sending
    // NOTIFICATION_BROWSER_THEME_CHANGED causes the previous theme to be
    // uninstalled when the notification causes the remaining infobar to close
    // and does not open any new infobars. See crbug.com/468280.

    // Disable the old theme.
    service->DisableExtension(previous_theme_id,
                              extensions::Extension::DISABLE_USER_ACTION);
  }
}

void ThemeService::SetCustomDefaultTheme(
    scoped_refptr<CustomThemeSupplier> theme_supplier) {
  ClearAllThemeData();
  SwapThemeSupplier(theme_supplier);
  NotifyThemeChanged();
}

bool ThemeService::ShouldInitWithSystemTheme() const {
  return false;
}

void ThemeService::RemoveUnusedThemes(bool ignore_infobars) {
  // We do not want to garbage collect themes on startup (|ready_| is false).
  // Themes will get garbage collected after |kRemoveUnusedThemesStartupDelay|.
  if (!profile_ || !ready_)
    return;
  if (!ignore_infobars && number_of_infobars_ != 0)
    return;

  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  if (!service)
    return;

  std::string current_theme = GetThemeID();
  std::vector<std::string> remove_list;
  scoped_ptr<const extensions::ExtensionSet> extensions(
      extensions::ExtensionRegistry::Get(profile_)
          ->GenerateInstalledExtensionsSet());
  extensions::ExtensionPrefs* prefs = extensions::ExtensionPrefs::Get(profile_);
  for (extensions::ExtensionSet::const_iterator it = extensions->begin();
       it != extensions->end(); ++it) {
    const extensions::Extension* extension = it->get();
    if (extension->is_theme() &&
        extension->id() != current_theme) {
      // Only uninstall themes which are not disabled or are disabled with
      // reason DISABLE_USER_ACTION. We cannot blanket uninstall all disabled
      // themes because externally installed themes are initially disabled.
      int disable_reason = prefs->GetDisableReasons(extension->id());
      if (!prefs->IsExtensionDisabled(extension->id()) ||
          disable_reason == Extension::DISABLE_USER_ACTION) {
        remove_list.push_back((*it)->id());
      }
    }
  }
  // TODO: Garbage collect all unused themes. This method misses themes which
  // are installed but not loaded because they are blacklisted by a management
  // policy provider.

  for (size_t i = 0; i < remove_list.size(); ++i) {
    service->UninstallExtension(remove_list[i],
                                extensions::UNINSTALL_REASON_ORPHANED_THEME,
                                base::Bind(&base::DoNothing), nullptr);
  }
}

void ThemeService::UseDefaultTheme() {
  if (ready_)
    content::RecordAction(UserMetricsAction("Themes_Reset"));
#if defined(ENABLE_SUPERVISED_USERS)
  if (IsSupervisedUser()) {
    SetSupervisedUserTheme();
    return;
  }
#endif
  ClearAllThemeData();
  NotifyThemeChanged();
}

void ThemeService::UseSystemTheme() {
  UseDefaultTheme();
}

bool ThemeService::UsingDefaultTheme() const {
  std::string id = GetThemeID();
  return id == ThemeService::kDefaultThemeID ||
      id == kDefaultThemeGalleryID;
}

std::string ThemeService::GetThemeID() const {
  return profile_->GetPrefs()->GetString(prefs::kCurrentThemeID);
}

color_utils::HSL ThemeService::GetTint(int id, bool otr) const {
  DCHECK(CalledOnValidThread());

  color_utils::HSL hsl;
  if (theme_supplier_ && theme_supplier_->GetTint(id, &hsl))
    return hsl;

  return ThemeProperties::GetDefaultTint(id, otr);
}

void ThemeService::ClearAllThemeData() {
  if (!ready_)
    return;

  SwapThemeSupplier(nullptr);

  // Clear our image cache.
  FreePlatformCaches();

  profile_->GetPrefs()->ClearPref(prefs::kCurrentThemePackFilename);
  SaveThemeID(kDefaultThemeID);

  // There should be no more infobars. This may not be the case because of
  // http://crbug.com/62154
  // RemoveUnusedThemes is called on a task because ClearAllThemeData() may
  // be called as a result of NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&ThemeService::RemoveUnusedThemes,
                            weak_ptr_factory_.GetWeakPtr(), true));
}

void ThemeService::LoadThemePrefs() {
  PrefService* prefs = profile_->GetPrefs();

  std::string current_id = GetThemeID();
  if (current_id == kDefaultThemeID) {
#if defined(ENABLE_SUPERVISED_USERS)
    // Supervised users have a different default theme.
    if (IsSupervisedUser()) {
      SetSupervisedUserTheme();
      set_ready();
      return;
    }
#endif
    if (ShouldInitWithSystemTheme())
      UseSystemTheme();
    else
      UseDefaultTheme();
    set_ready();
    return;
  }

  bool loaded_pack = false;

  // If we don't have a file pack, we're updating from an old version, or the
  // pack was created for an alternative MaterialDesignController::Mode.
  base::FilePath path = prefs->GetFilePath(prefs::kCurrentThemePackFilename);
  if (path != base::FilePath()) {
    path = path.Append(ui::MaterialDesignController::IsModeMaterial()
                           ? chrome::kThemePackMaterialDesignFilename
                           : chrome::kThemePackFilename);
    SwapThemeSupplier(BrowserThemePack::BuildFromDataPack(path, current_id));
    loaded_pack = theme_supplier_ != nullptr;
  }

  if (loaded_pack) {
    content::RecordAction(UserMetricsAction("Themes.Loaded"));
    set_ready();
  }
  // Else: wait for the extension service to be ready so that the theme pack
  // can be recreated from the extension.
}

void ThemeService::NotifyThemeChanged() {
  if (!ready_)
    return;

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

#if defined(USE_AURA)
void ThemeService::FreePlatformCaches() {
  // Views (Skia) has no platform image cache to clear.
}
#endif

bool ThemeService::UsingSystemTheme() const {
  return UsingDefaultTheme();
}

gfx::ImageSkia* ThemeService::GetImageSkiaNamed(int id) const {
  gfx::Image image = GetImageNamed(id);
  if (image.IsEmpty())
    return nullptr;
  // TODO(pkotwicz): Remove this const cast.  The gfx::Image interface returns
  // its images const. GetImageSkiaNamed() also should but has many callsites.
  return const_cast<gfx::ImageSkia*>(image.ToImageSkia());
}

SkColor ThemeService::GetColor(int id, bool otr) const {
  DCHECK(CalledOnValidThread());

  // For legacy reasons, |theme_supplier_| requires the incognito variants
  // of color IDs.
  int theme_supplier_id = id;
  if (otr) {
    if (id == ThemeProperties::COLOR_FRAME)
      theme_supplier_id = ThemeProperties::COLOR_FRAME_INCOGNITO;
    else if (id == ThemeProperties::COLOR_FRAME_INACTIVE)
      theme_supplier_id = ThemeProperties::COLOR_FRAME_INCOGNITO_INACTIVE;
  }

  SkColor color;
  if (theme_supplier_ && theme_supplier_->GetColor(theme_supplier_id, &color))
    return color;

  // For backward compat with older themes, some newer colors are generated from
  // older ones if they are missing.
  const int kNtpText = ThemeProperties::COLOR_NTP_TEXT;
  const int kLabelBackground =
      ThemeProperties::COLOR_SUPERVISED_USER_LABEL_BACKGROUND;
  switch (id) {
    case ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON:
      return color_utils::HSLShift(gfx::kChromeIconGrey,
                                   GetTint(ThemeProperties::TINT_BUTTONS, otr));
    case ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON_INACTIVE:
      // The active color is overridden in Gtk2UI.
      return SkColorSetA(
          GetColor(ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON, otr), 0x33);
    case ThemeProperties::COLOR_BACKGROUND_TAB:
      return color_utils::HSLShift(
          GetColor(ThemeProperties::COLOR_TOOLBAR, otr),
          GetTint(ThemeProperties::TINT_BACKGROUND_TAB, otr));
    case ThemeProperties::COLOR_DETACHED_BOOKMARK_BAR_BACKGROUND:
      if (UsingDefaultTheme())
        break;
      return GetColor(ThemeProperties::COLOR_TOOLBAR, otr);
    case ThemeProperties::COLOR_DETACHED_BOOKMARK_BAR_SEPARATOR:
      if (UsingDefaultTheme())
        break;
      // Use 50% of bookmark text color as separator color.
      return SkColorSetA(GetColor(ThemeProperties::COLOR_BOOKMARK_TEXT, otr),
                         128);
    case ThemeProperties::COLOR_NTP_SECTION_HEADER_TEXT:
      return IncreaseLightness(GetColor(kNtpText, otr), 0.30);
    case ThemeProperties::COLOR_NTP_SECTION_HEADER_TEXT_HOVER:
      return GetColor(kNtpText, otr);
    case ThemeProperties::COLOR_NTP_SECTION_HEADER_RULE:
      return IncreaseLightness(GetColor(kNtpText, otr), 0.70);
    case ThemeProperties::COLOR_NTP_SECTION_HEADER_RULE_LIGHT:
      return IncreaseLightness(GetColor(kNtpText, otr), 0.86);
    case ThemeProperties::COLOR_NTP_TEXT_LIGHT:
      return IncreaseLightness(GetColor(kNtpText, otr), 0.40);
    case ThemeProperties::COLOR_TAB_THROBBER_SPINNING:
    case ThemeProperties::COLOR_TAB_THROBBER_WAITING: {
      SkColor base_color =
          ui::GetAuraColor(id == ThemeProperties::COLOR_TAB_THROBBER_SPINNING
                               ? ui::NativeTheme::kColorId_ThrobberSpinningColor
                               : ui::NativeTheme::kColorId_ThrobberWaitingColor,
                           nullptr);
      color_utils::HSL hsl = GetTint(ThemeProperties::TINT_BUTTONS, otr);
      return color_utils::HSLShift(base_color, hsl);
    }
#if defined(ENABLE_SUPERVISED_USERS)
    case ThemeProperties::COLOR_SUPERVISED_USER_LABEL:
      return color_utils::GetReadableColor(SK_ColorWHITE,
                                           GetColor(kLabelBackground, otr));
    case ThemeProperties::COLOR_SUPERVISED_USER_LABEL_BACKGROUND:
      return color_utils::BlendTowardOppositeLuminance(
          GetColor(ThemeProperties::COLOR_FRAME, otr), 0x80);
    case ThemeProperties::COLOR_SUPERVISED_USER_LABEL_BORDER:
      return color_utils::AlphaBlend(GetColor(kLabelBackground, otr),
                                     SK_ColorBLACK, 230);
#endif
    case ThemeProperties::COLOR_STATUS_BAR_TEXT: {
      // A long time ago, we blended the toolbar and the tab text together to
      // get the status bar text because, at the time, our text rendering in
      // views couldn't do alpha blending. Even though this is no longer the
      // case, this blending decision is built into the majority of themes that
      // exist, and we must keep doing it.
      SkColor toolbar_color = GetColor(ThemeProperties::COLOR_TOOLBAR, otr);
      SkColor text_color = GetColor(ThemeProperties::COLOR_TAB_TEXT, otr);
      return SkColorSetARGB(
          SkColorGetA(text_color),
          (SkColorGetR(text_color) + SkColorGetR(toolbar_color)) / 2,
          (SkColorGetG(text_color) + SkColorGetR(toolbar_color)) / 2,
          (SkColorGetB(text_color) + SkColorGetR(toolbar_color)) / 2);
    }
  }

  return ThemeProperties::GetDefaultColor(id, otr);
}

int ThemeService::GetDisplayProperty(int id) const {
  int result = 0;
  if (theme_supplier_ && theme_supplier_->GetDisplayProperty(id, &result)) {
    return result;
  }

  switch (id) {
    case ThemeProperties::NTP_BACKGROUND_ALIGNMENT:
      return ThemeProperties::ALIGN_CENTER;

    case ThemeProperties::NTP_BACKGROUND_TILING:
      return ThemeProperties::NO_REPEAT;

    case ThemeProperties::NTP_LOGO_ALTERNATE: {
      if (UsingDefaultTheme() || UsingSystemTheme())
        return 0;
      if (HasCustomImage(IDR_THEME_NTP_BACKGROUND))
        return 1;
      return IsColorGrayscale(
          GetColor(ThemeProperties::COLOR_NTP_BACKGROUND, false)) ? 0 : 1;
    }

    default:
      return -1;
  }
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
  return BrowserThemePack::IsPersistentImageID(id) && theme_supplier_ &&
         theme_supplier_->HasCustomImage(id);
}

base::RefCountedMemory* ThemeService::GetRawData(
    int id,
    ui::ScaleFactor scale_factor) const {
  // Check to see whether we should substitute some images.
  int ntp_alternate = GetDisplayProperty(ThemeProperties::NTP_LOGO_ALTERNATE);
  if (id == IDR_PRODUCT_LOGO && ntp_alternate != 0)
    id = IDR_PRODUCT_LOGO_WHITE;

  base::RefCountedMemory* data = nullptr;
  if (theme_supplier_)
    data = theme_supplier_->GetRawData(id, scale_factor);
  if (!data)
    data = rb_.LoadDataResourceBytesForScale(id, ui::SCALE_FACTOR_100P);

  return data;
}

gfx::Image ThemeService::GetImageNamed(int id) const {
  DCHECK(CalledOnValidThread());

  gfx::Image image;
  if (theme_supplier_)
    image = theme_supplier_->GetImageNamed(id);

  if (image.IsEmpty())
    image = rb_.GetNativeImageNamed(id);

  return image;
}

void ThemeService::OnExtensionServiceReady() {
  if (!ready_) {
    // If the ThemeService is not ready yet, the custom theme data pack needs to
    // be recreated from the extension.
    MigrateTheme();
    set_ready();

    // Send notification in case anyone requested data and cached it when the
    // theme service was not ready yet.
    NotifyThemeChanged();
  }

#if defined(ENABLE_EXTENSIONS)
  theme_observer_.reset(new ThemeObserver(this));
#endif

  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_ENABLED,
                 content::Source<Profile>(profile_));

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&ThemeService::RemoveUnusedThemes,
                            weak_ptr_factory_.GetWeakPtr(), false),
      base::TimeDelta::FromSeconds(kRemoveUnusedThemesStartupDelay));
}

void ThemeService::MigrateTheme() {
  // TODO(erg): We need to pop up a dialog informing the user that their
  // theme is being migrated.
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  const Extension* extension =
      service ? service->GetExtensionById(GetThemeID(), false) : nullptr;
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

void ThemeService::SwapThemeSupplier(
    scoped_refptr<CustomThemeSupplier> theme_supplier) {
  if (theme_supplier_)
    theme_supplier_->StopUsingTheme();
  theme_supplier_ = theme_supplier;
  if (theme_supplier_)
    theme_supplier_->StartUsingTheme();
}

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
      extension->path().Append(ui::MaterialDesignController::IsModeMaterial()
                                   ? chrome::kThemePackMaterialDesignFilename
                                   : chrome::kThemePackFilename);
  service->GetFileTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&WritePackToDiskCallback, pack, pack_path));

  // Save only the extension path. The packed file which matches the
  // MaterialDesignController::Mode will be loaded via LoadThemePrefs().
  SavePackName(extension->path());
  SwapThemeSupplier(pack);
}

#if defined(ENABLE_SUPERVISED_USERS)
bool ThemeService::IsSupervisedUser() const {
  return profile_->IsSupervised();
}

void ThemeService::SetSupervisedUserTheme() {
  SetCustomDefaultTheme(new SupervisedUserTheme);
}
#endif

void ThemeService::OnInfobarDisplayed() {
  number_of_infobars_++;
}

void ThemeService::OnInfobarDestroyed() {
  number_of_infobars_--;

  if (number_of_infobars_ == 0)
    RemoveUnusedThemes(false);
}

ThemeSyncableService* ThemeService::GetThemeSyncableService() const {
  return theme_syncable_service_.get();
}

// static
const ui::ThemeProvider& ThemeService::GetThemeProviderForProfile(
    Profile* profile) {
  ThemeService* service = ThemeServiceFactory::GetForProfile(profile);
#if defined(OS_MACOSX)
  // TODO(estade): this doesn't work for OSX yet; fall back to normal theming
  // in incognito. Since the OSX version of ThemeService caches colors, and
  // both ThemeProviders use the same ThemeService some code needs to be
  // rearranged.
  bool incognito = false;
#else
  bool incognito = profile->GetProfileType() == Profile::INCOGNITO_PROFILE;
#endif
  return incognito ? service->otr_theme_provider_
                   : service->original_theme_provider_;
}

ThemeService::BrowserThemeProvider::BrowserThemeProvider(
    const ThemeService& theme_service,
    bool incognito)
    : theme_service_(theme_service), incognito_(incognito) {}

ThemeService::BrowserThemeProvider::~BrowserThemeProvider() {}

gfx::ImageSkia* ThemeService::BrowserThemeProvider::GetImageSkiaNamed(
    int id) const {
  return theme_service_.GetImageSkiaNamed(id);
}

SkColor ThemeService::BrowserThemeProvider::GetColor(int id) const {
  return theme_service_.GetColor(id, incognito_);
}

int ThemeService::BrowserThemeProvider::GetDisplayProperty(int id) const {
  return theme_service_.GetDisplayProperty(id);
}

bool ThemeService::BrowserThemeProvider::ShouldUseNativeFrame() const {
  return theme_service_.ShouldUseNativeFrame();
}

bool ThemeService::BrowserThemeProvider::HasCustomImage(int id) const {
  return theme_service_.HasCustomImage(id);
}

base::RefCountedMemory* ThemeService::BrowserThemeProvider::GetRawData(
    int id,
    ui::ScaleFactor scale_factor) const {
  return theme_service_.GetRawData(id, scale_factor);
}
