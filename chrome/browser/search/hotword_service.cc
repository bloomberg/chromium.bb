// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/hotword_service.h"

#include <string>

#include "base/command_line.h"
#include "base/i18n/case_conversion.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/hotword_private/hotword_private_api.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/pending_extension_manager.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search/hotword_audio_history_handler.h"
#include "chrome/browser/search/hotword_service_factory.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/language_usage_metrics/language_usage_metrics.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/common/webplugininfo.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/one_shot_event.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

using extensions::BrowserContextKeyedAPIFactory;
using extensions::HotwordPrivateEventService;

namespace {

// Allowed locales for hotwording. Note that Chrome does not support all of
// these locales, condensing them to their 2-letter equivalent, but the full
// list is here for completeness and testing.
static const char* kSupportedLocales[] = {
  "en",
  "en_au",
  "en_ca",
  "en_gb",
  "en_nz",
  "en_us",
  "en_za",
  "de",
  "de_at",
  "de_de",
  "es",
  "es_419",
  "es_es",
  "fr",
  "fr_fr",
  "it",
  "it_it",
  "ja",
  "ja_jp",
  "ko",
  "ko_kr",
  "pt_br",
  "ru",
  "ru_ru"
};

// Maximum number of retries for installing the hotword shared module from the
// web store.
static const int kMaxInstallRetries = 2;

// Delay between retries for installing the hotword shared module from the web
// store.
static const int kInstallRetryDelaySeconds = 5;

// Enum describing the state of the hotword preference.
// This is used for UMA stats -- do not reorder or delete items; only add to
// the end.
enum HotwordEnabled {
  UNSET = 0,  // No hotword preference has been set.
  ENABLED,    // The (classic) hotword preference is enabled.
  DISABLED,   // All hotwording is disabled.
  ALWAYS_ON_ENABLED,  // Always-on hotwording is enabled.
  NUM_HOTWORD_ENABLED_METRICS
};

// Enum describing the availability state of the hotword extension.
// This is used for UMA stats -- do not reorder or delete items; only add to
// the end.
enum HotwordExtensionAvailability {
  UNAVAILABLE = 0,
  AVAILABLE,
  PENDING_DOWNLOAD,
  DISABLED_EXTENSION,
  NUM_HOTWORD_EXTENSION_AVAILABILITY_METRICS
};

// Enum describing the types of errors that can arise when determining
// if hotwording can be used. NO_ERROR is used so it can be seen how often
// errors arise relative to when they do not.
// This is used for UMA stats -- do not reorder or delete items; only add to
// the end.
enum HotwordError {
  NO_HOTWORD_ERROR = 0,
  GENERIC_HOTWORD_ERROR,
  NACL_HOTWORD_ERROR,
  MICROPHONE_HOTWORD_ERROR,
  NUM_HOTWORD_ERROR_METRICS
};

void RecordExtensionAvailabilityMetrics(
    ExtensionService* service,
    const extensions::Extension* extension) {
  HotwordExtensionAvailability availability_state = UNAVAILABLE;
  if (extension) {
    availability_state = AVAILABLE;
  } else if (service->pending_extension_manager() &&
             service->pending_extension_manager()->IsIdPending(
                 extension_misc::kHotwordExtensionId)) {
    availability_state = PENDING_DOWNLOAD;
  } else if (!service->IsExtensionEnabled(
      extension_misc::kHotwordExtensionId)) {
    availability_state = DISABLED_EXTENSION;
  }
  UMA_HISTOGRAM_ENUMERATION("Hotword.HotwordExtensionAvailability",
                            availability_state,
                            NUM_HOTWORD_EXTENSION_AVAILABILITY_METRICS);
}

void RecordLoggingMetrics(Profile* profile) {
  // If the user is not opted in to hotword voice search, the audio logging
  // metric is not valid so it is not recorded.
  if (!profile->GetPrefs()->GetBoolean(prefs::kHotwordSearchEnabled))
    return;

  UMA_HISTOGRAM_BOOLEAN(
      "Hotword.HotwordAudioLogging",
      profile->GetPrefs()->GetBoolean(prefs::kHotwordAudioLoggingEnabled));
}

void RecordErrorMetrics(int error_message) {
  HotwordError error = NO_HOTWORD_ERROR;
  switch (error_message) {
    case IDS_HOTWORD_GENERIC_ERROR_MESSAGE:
      error = GENERIC_HOTWORD_ERROR;
      break;
    case IDS_HOTWORD_NACL_DISABLED_ERROR_MESSAGE:
      error = NACL_HOTWORD_ERROR;
      break;
    case IDS_HOTWORD_MICROPHONE_ERROR_MESSAGE:
      error = MICROPHONE_HOTWORD_ERROR;
      break;
    default:
      error = NO_HOTWORD_ERROR;
  }

  UMA_HISTOGRAM_ENUMERATION("Hotword.HotwordError",
                            error,
                            NUM_HOTWORD_ERROR_METRICS);
}

void RecordHotwordEnabledMetric(HotwordService *service, Profile* profile) {
  HotwordEnabled enabled_state = DISABLED;
  auto prefs = profile->GetPrefs();
  if (!prefs->HasPrefPath(prefs::kHotwordSearchEnabled) &&
      !prefs->HasPrefPath(prefs::kHotwordAlwaysOnSearchEnabled)) {
    enabled_state = UNSET;
  } else if (service->IsExperimentalHotwordingEnabled() &&
             service->IsAlwaysOnEnabled()) {
    enabled_state = ALWAYS_ON_ENABLED;
  } else if (prefs->GetBoolean(prefs::kHotwordSearchEnabled)) {
    enabled_state = ENABLED;
  }
  UMA_HISTOGRAM_ENUMERATION("Hotword.Enabled", enabled_state,
                            NUM_HOTWORD_ENABLED_METRICS);
}

ExtensionService* GetExtensionService(Profile* profile) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  extensions::ExtensionSystem* extension_system =
      extensions::ExtensionSystem::Get(profile);
  return extension_system ?  extension_system->extension_service() : NULL;
}

std::string GetCurrentLocale(Profile* profile) {
#if defined(OS_CHROMEOS)
  std::string profile_locale =
      profile->GetPrefs()->GetString(prefs::kApplicationLocale);
  if (!profile_locale.empty()) {
    // On ChromeOS locale is per-profile, but only if set.
    return profile_locale;
  }
#endif
  return g_browser_process->GetApplicationLocale();
}

}  // namespace

namespace hotword_internal {
// Constants for the hotword field trial.
const char kHotwordFieldTrialName[] = "VoiceTrigger";
const char kHotwordFieldTrialDisabledGroupName[] = "Disabled";
const char kHotwordFieldTrialExperimentalGroupName[] = "Experimental";
// Old preference constant.
const char kHotwordUnusablePrefName[] = "hotword.search_enabled";
// String passed to indicate the training state has changed.
const char kHotwordTrainingEnabled[] = "hotword_training_enabled";
// Id of the hotword notification.
const char kHotwordNotificationId[] = "hotword";
// Notifier id for the hotword notification.
const char kHotwordNotifierId[] = "hotword.notification";
}  // namespace hotword_internal

// Delegate for the hotword notification.
class HotwordNotificationDelegate : public NotificationDelegate {
 public:
  explicit HotwordNotificationDelegate(Profile* profile)
      : profile_(profile) {
  }

  // Overridden from NotificationDelegate:
  void ButtonClick(int button_index) override {
    DCHECK_EQ(0, button_index);
    Click();
  }

  void Click() override {
    // Launch the hotword audio verification app in the right mode.
    HotwordService::LaunchMode launch_mode =
        HotwordService::HOTWORD_AND_AUDIO_HISTORY;
    if (profile_->GetPrefs()->GetBoolean(
            prefs::kHotwordAudioLoggingEnabled)) {
      launch_mode = HotwordService::HOTWORD_ONLY;
    }

    HotwordService* hotword_service =
        HotwordServiceFactory::GetForProfile(profile_);

    if (!hotword_service)
      return;

    hotword_service->LaunchHotwordAudioVerificationApp(launch_mode);

    // Close the notification after it's been clicked on to remove it
    // from the notification tray.
    g_browser_process->notification_ui_manager()->CancelById(
        id(), NotificationUIManager::GetProfileID(profile_));
  }

  // Overridden from NotificationDelegate:
  std::string id() const override {
    return hotword_internal::kHotwordNotificationId;
  }

 private:
  ~HotwordNotificationDelegate() override {}

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(HotwordNotificationDelegate);
};

// static
bool HotwordService::DoesHotwordSupportLanguage(Profile* profile) {
  std::string normalized_locale =
      l10n_util::NormalizeLocale(GetCurrentLocale(profile));
  base::StringToLowerASCII(&normalized_locale);

  for (size_t i = 0; i < arraysize(kSupportedLocales); i++) {
    if (normalized_locale == kSupportedLocales[i])
      return true;
  }
  return false;
}

// static
bool HotwordService::IsExperimentalHotwordingEnabled() {
  std::string group = base::FieldTrialList::FindFullName(
      hotword_internal::kHotwordFieldTrialName);
  if (!group.empty() &&
      group == hotword_internal::kHotwordFieldTrialExperimentalGroupName) {
    return true;
  }

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return !command_line->HasSwitch(switches::kDisableExperimentalHotwording);
}

#if defined(OS_CHROMEOS)
class HotwordService::HotwordUserSessionStateObserver
    : public user_manager::UserManager::UserSessionStateObserver {
 public:
  explicit HotwordUserSessionStateObserver(HotwordService* service)
      : service_(service) {}

  // Overridden from UserSessionStateObserver:
  void ActiveUserChanged(const user_manager::User* active_user) override {
    service_->ActiveUserChanged();
  }

 private:
  HotwordService* service_;  // Not owned
};
#else
// Dummy class to please the linker.
class HotwordService::HotwordUserSessionStateObserver {
};
#endif

void HotwordService::HotwordWebstoreInstaller::Shutdown() {
  AbortInstall();
}

HotwordService::HotwordService(Profile* profile)
    : profile_(profile),
      extension_registry_observer_(this),
      client_(NULL),
      error_message_(0),
      reinstall_pending_(false),
      training_(false),
      weak_factory_(this) {
  extension_registry_observer_.Add(extensions::ExtensionRegistry::Get(profile));
  if (IsExperimentalHotwordingEnabled()) {
    // Disable the old extension so it doesn't interfere with the new stuff.
    DisableHotwordExtension(GetExtensionService(profile_));
  } else {
    if (!profile_->GetPrefs()->HasPrefPath(prefs::kHotwordSearchEnabled) &&
        IsHotwordAllowed()) {
      // If the preference has not been set the hotword extension should
      // not be running. However, this should only be done if auto-install
      // is enabled which is gated through the IsHotwordAllowed check.
      DisableHotwordExtension(GetExtensionService(profile_));
    }
  }
  // This will be called during profile initialization which is a good time
  // to check the user's hotword state.
  RecordHotwordEnabledMetric(this, profile_);

  pref_registrar_.Init(profile_->GetPrefs());
  pref_registrar_.Add(
      prefs::kHotwordSearchEnabled,
      base::Bind(&HotwordService::OnHotwordSearchEnabledChanged,
      base::Unretained(this)));
  pref_registrar_.Add(
      prefs::kHotwordAlwaysOnSearchEnabled,
      base::Bind(&HotwordService::OnHotwordAlwaysOnSearchEnabledChanged,
      base::Unretained(this)));

  extensions::ExtensionSystem::Get(profile_)->ready().Post(
      FROM_HERE,
      base::Bind(base::IgnoreResult(
          &HotwordService::MaybeReinstallHotwordExtension),
                 weak_factory_.GetWeakPtr()));

  // Clear the old user pref because it became unusable.
  // TODO(rlp): Remove this code per crbug.com/358789.
  if (profile_->GetPrefs()->HasPrefPath(
          hotword_internal::kHotwordUnusablePrefName)) {
    profile_->GetPrefs()->ClearPref(hotword_internal::kHotwordUnusablePrefName);
  }

  SetAudioHistoryHandler(new HotwordAudioHistoryHandler(
      profile_, base::MessageLoop::current()->task_runner()));

  if (HotwordServiceFactory::IsAlwaysOnAvailable() &&
      IsHotwordAllowed() &&
      IsExperimentalHotwordingEnabled()) {
    // Show the hotword notification in 5 seconds if the experimental flag is
    // on, or in 30 minutes if not. We need to wait at least a few seconds
    // for the hotword extension to be installed.
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(switches::kEnableExperimentalHotwordHardware)) {
      base::MessageLoop::current()->PostDelayedTask(
          FROM_HERE,
          base::Bind(&HotwordService::ShowHotwordNotification,
                     weak_factory_.GetWeakPtr()),
          base::TimeDelta::FromSeconds(5));
    } else if (!profile_->GetPrefs()->GetBoolean(
                   prefs::kHotwordAlwaysOnNotificationSeen)) {
      base::MessageLoop::current()->PostDelayedTask(
          FROM_HERE,
          base::Bind(&HotwordService::ShowHotwordNotification,
                     weak_factory_.GetWeakPtr()),
          base::TimeDelta::FromMinutes(30));
    }
  }

#if defined(OS_CHROMEOS)
  if (user_manager::UserManager::IsInitialized()) {
    session_observer_.reset(new HotwordUserSessionStateObserver(this));
    user_manager::UserManager::Get()->AddSessionStateObserver(
        session_observer_.get());
  }
#endif
}

HotwordService::~HotwordService() {
#if defined(OS_CHROMEOS)
  if (user_manager::UserManager::IsInitialized() && session_observer_) {
    user_manager::UserManager::Get()->RemoveSessionStateObserver(
        session_observer_.get());
  }
#endif
}

void HotwordService::Shutdown() {
  if (installer_.get())
    installer_->Shutdown();
}

void HotwordService::ShowHotwordNotification() {
  // Check for enabled here in case always-on was enabled during the delay.
  if (!IsServiceAvailable() || IsAlwaysOnEnabled())
    return;

  message_center::RichNotificationData data;
  const base::string16 label = l10n_util::GetStringUTF16(
        IDS_HOTWORD_NOTIFICATION_BUTTON);
  data.buttons.push_back(message_center::ButtonInfo(label));

  Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      GURL(),
      l10n_util::GetStringUTF16(IDS_HOTWORD_NOTIFICATION_TITLE),
      l10n_util::GetStringUTF16(IDS_HOTWORD_NOTIFICATION_DESCRIPTION),
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_HOTWORD_NOTIFICATION_ICON),
      message_center::NotifierId(
          message_center::NotifierId::SYSTEM_COMPONENT,
          hotword_internal::kHotwordNotifierId),
      base::string16(),
      std::string(),
      data,
      new HotwordNotificationDelegate(profile_));

  g_browser_process->notification_ui_manager()->Add(notification, profile_);
  profile_->GetPrefs()->SetBoolean(
      prefs::kHotwordAlwaysOnNotificationSeen, true);
}

void HotwordService::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UninstallReason reason) {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if ((extension->id() != extension_misc::kHotwordExtensionId &&
       extension->id() != extension_misc::kHotwordSharedModuleId) ||
       profile_ != Profile::FromBrowserContext(browser_context) ||
      !GetExtensionService(profile_))
    return;

  // If the extension wasn't uninstalled due to language change, don't try to
  // reinstall it.
  if (!reinstall_pending_)
    return;

  InstallHotwordExtensionFromWebstore(kMaxInstallRetries);
  SetPreviousLanguagePref();
}

std::string HotwordService::ReinstalledExtensionId() {
  if (IsExperimentalHotwordingEnabled())
    return extension_misc::kHotwordSharedModuleId;

  return extension_misc::kHotwordExtensionId;
}

void HotwordService::InstalledFromWebstoreCallback(
    int num_tries,
    bool success,
    const std::string& error,
    extensions::webstore_install::Result result) {
  if (result != extensions::webstore_install::SUCCESS && num_tries) {
    // Try again on failure.
    content::BrowserThread::PostDelayedTask(
        content::BrowserThread::UI,
        FROM_HERE,
        base::Bind(&HotwordService::InstallHotwordExtensionFromWebstore,
                   weak_factory_.GetWeakPtr(),
                   num_tries),
        base::TimeDelta::FromSeconds(kInstallRetryDelaySeconds));
  }
}

void HotwordService::InstallHotwordExtensionFromWebstore(int num_tries) {
  installer_ = new HotwordWebstoreInstaller(
      ReinstalledExtensionId(),
      profile_,
      base::Bind(&HotwordService::InstalledFromWebstoreCallback,
                 weak_factory_.GetWeakPtr(),
                 num_tries - 1));
  installer_->BeginInstall();
}

void HotwordService::OnExtensionInstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    bool is_update) {

  if ((extension->id() != extension_misc::kHotwordExtensionId &&
       extension->id() != extension_misc::kHotwordSharedModuleId) ||
       profile_ != Profile::FromBrowserContext(browser_context))
    return;

  // If the previous locale pref has never been set, set it now since
  // the extension has been installed.
  if (!profile_->GetPrefs()->HasPrefPath(prefs::kHotwordPreviousLanguage))
    SetPreviousLanguagePref();

  // If MaybeReinstallHotwordExtension already triggered an uninstall, we
  // don't want to loop and trigger another uninstall-install cycle.
  // However, if we arrived here via an uninstall-triggered-install (and in
  // that case |reinstall_pending_| will be true) then we know install
  // has completed and we can reset |reinstall_pending_|.
  if (!reinstall_pending_)
    MaybeReinstallHotwordExtension();
  else
    reinstall_pending_ = false;

  // Now that the extension is installed, if the user has not selected
  // the preference on, make sure it is turned off.
  //
  // Disabling the extension automatically on install should only occur
  // if the user is in the field trial for auto-install which is gated
  // by the IsHotwordAllowed check. The check for IsHotwordAllowed() here
  // can be removed once it's known that few people have manually
  // installed extension.
  if (IsHotwordAllowed() && !IsSometimesOnEnabled())
    DisableHotwordExtension(GetExtensionService(profile_));
}

bool HotwordService::MaybeReinstallHotwordExtension() {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  ExtensionService* extension_service = GetExtensionService(profile_);
  if (!extension_service)
    return false;

  const extensions::Extension* extension = extension_service->GetExtensionById(
      ReinstalledExtensionId(), true);
  if (!extension)
    return false;

  // If the extension is currently pending, return and we'll check again
  // after the install is finished.
  extensions::PendingExtensionManager* pending_manager =
      extension_service->pending_extension_manager();
  if (pending_manager->IsIdPending(extension->id()))
    return false;

  // If there is already a pending request from HotwordService, don't try
  // to uninstall either.
  if (reinstall_pending_)
    return false;

  // Check if the current locale matches the previous. If they don't match,
  // uninstall the extension.
  if (!ShouldReinstallHotwordExtension())
    return false;

  // Ensure the call to OnExtensionUninstalled was triggered by a language
  // change so it's okay to reinstall.
  reinstall_pending_ = true;

  // Disable always-on on a language change. We do this because the speaker-id
  // model needs to be re-trained.
  if (IsAlwaysOnEnabled()) {
    profile_->GetPrefs()->SetBoolean(prefs::kHotwordAlwaysOnSearchEnabled,
                                     false);
  }

  // Record re-installs due to language change.
  UMA_HISTOGRAM_SPARSE_SLOWLY(
      "Hotword.SharedModuleReinstallLanguage",
      language_usage_metrics::LanguageUsageMetrics::ToLanguageCode(
          GetCurrentLocale(profile_)));
  return UninstallHotwordExtension(extension_service);
}

bool HotwordService::UninstallHotwordExtension(
    ExtensionService* extension_service) {
  base::string16 error;
  std::string extension_id = ReinstalledExtensionId();
  if (!extension_service->UninstallExtension(
          extension_id,
          extensions::UNINSTALL_REASON_INTERNAL_MANAGEMENT,
          base::Bind(&base::DoNothing),
          &error)) {
    LOG(WARNING) << "Cannot uninstall extension with id "
                 << extension_id
                 << ": " << error;
    reinstall_pending_ = false;
    return false;
  }
  return true;
}

bool HotwordService::IsServiceAvailable() {
  error_message_ = 0;

  // Determine if the extension is available.
  extensions::ExtensionSystem* system =
      extensions::ExtensionSystem::Get(profile_);
  ExtensionService* service = system->extension_service();
  // Include disabled extensions (true parameter) since it may not be enabled
  // if the user opted out.
  const extensions::Extension* extension =
      service->GetExtensionById(ReinstalledExtensionId(), true);
  if (!extension)
    error_message_ = IDS_HOTWORD_GENERIC_ERROR_MESSAGE;

  RecordExtensionAvailabilityMetrics(service, extension);
  RecordLoggingMetrics(profile_);

  // Determine if NaCl is available.
  bool nacl_enabled = false;
  base::FilePath path;
  if (PathService::Get(chrome::FILE_NACL_PLUGIN, &path)) {
    content::WebPluginInfo info;
    PluginPrefs* plugin_prefs = PluginPrefs::GetForProfile(profile_).get();
    if (content::PluginService::GetInstance()->GetPluginInfoByPath(path, &info))
      nacl_enabled = plugin_prefs->IsPluginEnabled(info);
  }
  if (!nacl_enabled)
    error_message_ = IDS_HOTWORD_NACL_DISABLED_ERROR_MESSAGE;

  RecordErrorMetrics(error_message_);

  // Determine if the proper audio capabilities exist.
  // The first time this is called, it probably won't return in time, but that's
  // why it won't be included in the error calculation (i.e., the call to
  // IsAudioDeviceStateUpdated()). However, this use case is rare and typically
  // the devices will be initialized by the time a user goes to settings.
  bool audio_device_state_updated =
      HotwordServiceFactory::IsAudioDeviceStateUpdated();
  HotwordServiceFactory::GetInstance()->UpdateMicrophoneState();
  if (audio_device_state_updated) {
    bool audio_capture_allowed =
        profile_->GetPrefs()->GetBoolean(prefs::kAudioCaptureAllowed);
    if (!audio_capture_allowed ||
        !HotwordServiceFactory::IsMicrophoneAvailable())
      error_message_ = IDS_HOTWORD_MICROPHONE_ERROR_MESSAGE;
  }

  return (error_message_ == 0) && IsHotwordAllowed();
}

bool HotwordService::IsHotwordAllowed() {
  std::string group = base::FieldTrialList::FindFullName(
      hotword_internal::kHotwordFieldTrialName);
  return !group.empty() &&
      group != hotword_internal::kHotwordFieldTrialDisabledGroupName &&
      DoesHotwordSupportLanguage(profile_);
}

bool HotwordService::IsOptedIntoAudioLogging() {
  // Do not opt the user in if the preference has not been set.
  return
      profile_->GetPrefs()->HasPrefPath(prefs::kHotwordAudioLoggingEnabled) &&
      profile_->GetPrefs()->GetBoolean(prefs::kHotwordAudioLoggingEnabled);
}

bool HotwordService::IsAlwaysOnEnabled() {
  return
      profile_->GetPrefs()->HasPrefPath(prefs::kHotwordAlwaysOnSearchEnabled) &&
      profile_->GetPrefs()->GetBoolean(prefs::kHotwordAlwaysOnSearchEnabled) &&
      HotwordServiceFactory::IsAlwaysOnAvailable();
}

bool HotwordService::IsSometimesOnEnabled() {
  return profile_->GetPrefs()->HasPrefPath(prefs::kHotwordSearchEnabled) &&
      profile_->GetPrefs()->GetBoolean(prefs::kHotwordSearchEnabled) &&
      !HotwordServiceFactory::IsAlwaysOnAvailable();
}

void HotwordService::EnableHotwordExtension(
    ExtensionService* extension_service) {
  if (extension_service && !IsExperimentalHotwordingEnabled())
    extension_service->EnableExtension(extension_misc::kHotwordExtensionId);
}

void HotwordService::DisableHotwordExtension(
    ExtensionService* extension_service) {
  if (extension_service) {
    extension_service->DisableExtension(
        extension_misc::kHotwordExtensionId,
        extensions::Extension::DISABLE_USER_ACTION);
  }
}

void HotwordService::LaunchHotwordAudioVerificationApp(
    const LaunchMode& launch_mode) {
  hotword_audio_verification_launch_mode_ = launch_mode;

  ExtensionService* extension_service = GetExtensionService(profile_);
  if (!extension_service)
    return;
  const extensions::Extension* extension = extension_service->GetExtensionById(
      extension_misc::kHotwordAudioVerificationAppId, true);
  if (!extension)
    return;

  OpenApplication(
      AppLaunchParams(profile_, extension, extensions::LAUNCH_CONTAINER_WINDOW,
                      NEW_WINDOW, extensions::SOURCE_CHROME_INTERNAL));
}

HotwordService::LaunchMode
HotwordService::GetHotwordAudioVerificationLaunchMode() {
  return hotword_audio_verification_launch_mode_;
}

void HotwordService::StartTraining() {
  training_ = true;

  if (!IsServiceAvailable())
    return;

  HotwordPrivateEventService* event_service =
      BrowserContextKeyedAPIFactory<HotwordPrivateEventService>::Get(profile_);
  if (event_service)
    event_service->OnEnabledChanged(hotword_internal::kHotwordTrainingEnabled);
}

void HotwordService::FinalizeSpeakerModel() {
  if (!IsServiceAvailable())
    return;

  HotwordPrivateEventService* event_service =
      BrowserContextKeyedAPIFactory<HotwordPrivateEventService>::Get(profile_);
  if (event_service)
    event_service->OnFinalizeSpeakerModel();
}

void HotwordService::StopTraining() {
  training_ = false;

  if (!IsServiceAvailable())
    return;

  HotwordPrivateEventService* event_service =
      BrowserContextKeyedAPIFactory<HotwordPrivateEventService>::Get(profile_);
  if (event_service)
    event_service->OnEnabledChanged(hotword_internal::kHotwordTrainingEnabled);
}

void HotwordService::NotifyHotwordTriggered() {
  if (!IsServiceAvailable())
    return;

  HotwordPrivateEventService* event_service =
      BrowserContextKeyedAPIFactory<HotwordPrivateEventService>::Get(profile_);
  if (event_service)
    event_service->OnHotwordTriggered();
}

bool HotwordService::IsTraining() {
  return training_;
}

HotwordAudioHistoryHandler* HotwordService::GetAudioHistoryHandler() {
  return audio_history_handler_.get();
}

void HotwordService::SetAudioHistoryHandler(
    HotwordAudioHistoryHandler* handler) {
  audio_history_handler_.reset(handler);
  audio_history_handler_->UpdateAudioHistoryState();
}

void HotwordService::DisableHotwordPreferences() {
  if (IsSometimesOnEnabled()) {
    if (profile_->GetPrefs()->HasPrefPath(prefs::kHotwordSearchEnabled))
      profile_->GetPrefs()->SetBoolean(prefs::kHotwordSearchEnabled, false);
  } else if (IsAlwaysOnEnabled()) {
    if (profile_->GetPrefs()->HasPrefPath(
            prefs::kHotwordAlwaysOnSearchEnabled)) {
      profile_->GetPrefs()->SetBoolean(prefs::kHotwordAlwaysOnSearchEnabled,
                                       false);
    }
  }
}

void HotwordService::OnHotwordSearchEnabledChanged(
    const std::string& pref_name) {
  DCHECK_EQ(pref_name, std::string(prefs::kHotwordSearchEnabled));

  ExtensionService* extension_service = GetExtensionService(profile_);
  if (profile_->GetPrefs()->GetBoolean(prefs::kHotwordSearchEnabled))
    EnableHotwordExtension(extension_service);
  else
    DisableHotwordExtension(extension_service);
}

void HotwordService::OnHotwordAlwaysOnSearchEnabledChanged(
    const std::string& pref_name) {
  // If the pref for always on has been changed in some way, that means that
  // the user is aware of always on (either from settings or another source)
  // so they don't need to be shown the notification.
  profile_->GetPrefs()->SetBoolean(prefs::kHotwordAlwaysOnNotificationSeen,
                                   true);
  pref_registrar_.Remove(prefs::kHotwordAlwaysOnSearchEnabled);
}

void HotwordService::RequestHotwordSession(HotwordClient* client) {
  if (!IsServiceAvailable() || (client_ && client_ != client))
    return;

  client_ = client;

  HotwordPrivateEventService* event_service =
      BrowserContextKeyedAPIFactory<HotwordPrivateEventService>::Get(profile_);
  if (event_service)
    event_service->OnHotwordSessionRequested();
}

void HotwordService::StopHotwordSession(HotwordClient* client) {
  if (!IsServiceAvailable())
    return;

  // Do nothing if there's no client.
  if (!client_)
    return;
  DCHECK(client_ == client);

  client_ = NULL;
  HotwordPrivateEventService* event_service =
      BrowserContextKeyedAPIFactory<HotwordPrivateEventService>::Get(profile_);
  if (event_service)
    event_service->OnHotwordSessionStopped();
}

void HotwordService::SetPreviousLanguagePref() {
  profile_->GetPrefs()->SetString(prefs::kHotwordPreviousLanguage,
                                  GetCurrentLocale(profile_));
}

bool HotwordService::ShouldReinstallHotwordExtension() {
  // If there is no previous locale pref, then this is the first install
  // so no need to uninstall first.
  if (!profile_->GetPrefs()->HasPrefPath(prefs::kHotwordPreviousLanguage))
    return false;

  std::string previous_locale =
      profile_->GetPrefs()->GetString(prefs::kHotwordPreviousLanguage);
  std::string locale = GetCurrentLocale(profile_);

  // If it's a new locale, then the old extension should be uninstalled.
  return locale != previous_locale &&
      HotwordService::DoesHotwordSupportLanguage(profile_);
}

void HotwordService::ActiveUserChanged() {
  // Do nothing for old hotwording.
  if (!IsExperimentalHotwordingEnabled())
    return;

  // Don't bother notifying the extension if hotwording is completely off.
  if (!IsSometimesOnEnabled() && !IsAlwaysOnEnabled() && !IsTraining())
    return;

  HotwordPrivateEventService* event_service =
      BrowserContextKeyedAPIFactory<HotwordPrivateEventService>::Get(profile_);
  // "enabled" isn't being changed, but piggy-back off the notification anyway.
  if (event_service)
    event_service->OnEnabledChanged(prefs::kHotwordSearchEnabled);
}

bool HotwordService::UserIsActive() {
#if defined(OS_CHROMEOS)
  // Only support multiple profiles and profile switching in ChromeOS.
  if (user_manager::UserManager::IsInitialized()) {
    user_manager::User* user =
        user_manager::UserManager::Get()->GetActiveUser();
    if (user && user->is_profile_created())
      return profile_ == ProfileManager::GetActiveUserProfile();
  }
#endif
  return true;
}
