// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_system_impl.h"

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_tokenizer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/browser/extensions/blacklist.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/error_console/error_console.h"
#include "chrome/browser/extensions/extension_error_reporter.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/extension_warning_badge_service.h"
#include "chrome/browser/extensions/extension_warning_set.h"
#include "chrome/browser/extensions/install_verifier.h"
#include "chrome/browser/extensions/navigation_observer.h"
#include "chrome/browser/extensions/shared_module_service.h"
#include "chrome/browser/extensions/standard_management_policy_provider.h"
#include "chrome/browser/extensions/state_store_notification_observer.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/extensions/user_script_master.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "chrome/common/extensions/features/feature_channel.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/url_data_source.h"
#include "extensions/browser/content_verifier.h"
#include "extensions/browser/content_verifier_delegate.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_pref_store.h"
#include "extensions/browser/extension_pref_value_map.h"
#include "extensions/browser/extension_pref_value_map_factory.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/info_map.h"
#include "extensions/browser/lazy_background_task_queue.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/quota_service.h"
#include "extensions/browser/runtime_data.h"
#include "extensions/browser/state_store.h"
#include "extensions/browser/updater/manifest_fetch_data.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "net/base/escape.h"

#if defined(ENABLE_NOTIFICATIONS)
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "ui/message_center/notifier_settings.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/chromeos/extensions/device_local_account_management_policy_provider.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/extensions/extension_assets_manager_chromeos.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/login/login_state.h"
#include "components/user_manager/user.h"
#endif

using content::BrowserThread;

namespace {

const char kContentVerificationExperimentName[] =
    "ExtensionContentVerification";

}  // namespace

namespace extensions {

//
// ExtensionSystemImpl::Shared
//

ExtensionSystemImpl::Shared::Shared(Profile* profile)
    : profile_(profile) {
}

ExtensionSystemImpl::Shared::~Shared() {
}

void ExtensionSystemImpl::Shared::InitPrefs() {
  lazy_background_task_queue_.reset(new LazyBackgroundTaskQueue(profile_));
  event_router_.reset(new EventRouter(profile_, ExtensionPrefs::Get(profile_)));
  // Two state stores. The latter, which contains declarative rules, must be
  // loaded immediately so that the rules are ready before we issue network
  // requests.
  state_store_.reset(new StateStore(
      profile_,
      profile_->GetPath().AppendASCII(extensions::kStateStoreName),
      true));
  state_store_notification_observer_.reset(
      new StateStoreNotificationObserver(state_store_.get()));

  rules_store_.reset(new StateStore(
      profile_,
      profile_->GetPath().AppendASCII(extensions::kRulesStoreName),
      false));

  blacklist_.reset(new Blacklist(ExtensionPrefs::Get(profile_)));

  standard_management_policy_provider_.reset(
      new StandardManagementPolicyProvider(ExtensionPrefs::Get(profile_)));

#if defined(OS_CHROMEOS)
  const user_manager::User* user =
      chromeos::UserManager::Get()->GetActiveUser();
  policy::DeviceLocalAccount::Type device_local_account_type;
  if (user && policy::IsDeviceLocalAccountUser(user->email(),
                                               &device_local_account_type)) {
    device_local_account_management_policy_provider_.reset(
        new chromeos::DeviceLocalAccountManagementPolicyProvider(
            device_local_account_type));
  }
#endif  // defined(OS_CHROMEOS)
}

void ExtensionSystemImpl::Shared::RegisterManagementPolicyProviders() {
  DCHECK(standard_management_policy_provider_.get());
  management_policy_->RegisterProvider(
      standard_management_policy_provider_.get());

#if defined(OS_CHROMEOS)
  if (device_local_account_management_policy_provider_) {
    management_policy_->RegisterProvider(
        device_local_account_management_policy_provider_.get());
  }
#endif  // defined(OS_CHROMEOS)

  management_policy_->RegisterProvider(install_verifier_.get());
}

namespace {

class ContentVerifierDelegateImpl : public ContentVerifierDelegate {
 public:
  explicit ContentVerifierDelegateImpl(ExtensionService* service)
      : service_(service->AsWeakPtr()), default_mode_(GetDefaultMode()) {}

  virtual ~ContentVerifierDelegateImpl() {}

  virtual Mode ShouldBeVerified(const Extension& extension) OVERRIDE {
#if defined(OS_CHROMEOS)
    if (ExtensionAssetsManagerChromeOS::IsSharedInstall(&extension))
      return ContentVerifierDelegate::ENFORCE_STRICT;
#endif

    if (!extension.is_extension() && !extension.is_legacy_packaged_app())
      return ContentVerifierDelegate::NONE;
    if (!Manifest::IsAutoUpdateableLocation(extension.location()))
      return ContentVerifierDelegate::NONE;

    if (!ManifestURL::UpdatesFromGallery(&extension)) {
      // It's possible that the webstore update url was overridden for testing
      // so also consider extensions with the default (production) update url
      // to be from the store as well.
      GURL default_webstore_url = extension_urls::GetDefaultWebstoreUpdateUrl();
      if (ManifestURL::GetUpdateURL(&extension) != default_webstore_url)
        return ContentVerifierDelegate::NONE;
    }

    return default_mode_;
  }

  virtual const ContentVerifierKey& PublicKey() OVERRIDE {
    static ContentVerifierKey key(
        extension_misc::kWebstoreSignaturesPublicKey,
        extension_misc::kWebstoreSignaturesPublicKeySize);
    return key;
  }

  virtual GURL GetSignatureFetchUrl(const std::string& extension_id,
                                    const base::Version& version) OVERRIDE {
    // TODO(asargent) Factor out common code from the extension updater's
    // ManifestFetchData class that can be shared for use here.
    std::vector<std::string> parts;
    parts.push_back("uc");
    parts.push_back("installsource=signature");
    parts.push_back("id=" + extension_id);
    parts.push_back("v=" + version.GetString());
    std::string x_value =
        net::EscapeQueryParamValue(JoinString(parts, "&"), true);
    std::string query = "response=redirect&x=" + x_value;

    GURL base_url = extension_urls::GetWebstoreUpdateUrl();
    GURL::Replacements replacements;
    replacements.SetQuery(query.c_str(), url::Component(0, query.length()));
    return base_url.ReplaceComponents(replacements);
  }

  virtual std::set<base::FilePath> GetBrowserImagePaths(
      const extensions::Extension* extension) OVERRIDE {
    return extension_file_util::GetBrowserImagePaths(extension);
  }

  virtual void VerifyFailed(const std::string& extension_id) OVERRIDE {
    if (!service_)
      return;
    ExtensionRegistry* registry = ExtensionRegistry::Get(service_->profile());
    const Extension* extension =
        registry->GetExtensionById(extension_id, ExtensionRegistry::ENABLED);
    if (!extension)
      return;
    Mode mode = ShouldBeVerified(*extension);
    if (mode >= ContentVerifierDelegate::ENFORCE) {
      service_->DisableExtension(extension_id, Extension::DISABLE_CORRUPTED);
      ExtensionPrefs::Get(service_->profile())
          ->IncrementCorruptedDisableCount();
      UMA_HISTOGRAM_BOOLEAN("Extensions.CorruptExtensionBecameDisabled", true);
    } else if (!ContainsKey(would_be_disabled_ids_, extension_id)) {
      UMA_HISTOGRAM_BOOLEAN("Extensions.CorruptExtensionWouldBeDisabled", true);
      would_be_disabled_ids_.insert(extension_id);
    }
  }

  static Mode GetDefaultMode() {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

    Mode experiment_value = NONE;
    const std::string group = base::FieldTrialList::FindFullName(
        kContentVerificationExperimentName);
    if (group == "EnforceStrict")
      experiment_value = ContentVerifierDelegate::ENFORCE_STRICT;
    else if (group == "Enforce")
      experiment_value = ContentVerifierDelegate::ENFORCE;
    else if (group == "Bootstrap")
      experiment_value = ContentVerifierDelegate::BOOTSTRAP;

    // The field trial value that normally comes from the server can be
    // overridden on the command line, which we don't want to allow since
    // malware can set chrome command line flags. There isn't currently a way
    // to find out what the server-provided value is in this case, so we
    // conservatively default to the strictest mode if we detect our experiment
    // name being overridden.
    if (command_line->HasSwitch(switches::kForceFieldTrials)) {
      std::string forced_trials =
          command_line->GetSwitchValueASCII(switches::kForceFieldTrials);
      if (forced_trials.find(kContentVerificationExperimentName) !=
              std::string::npos)
        experiment_value = ContentVerifierDelegate::ENFORCE_STRICT;
    }

    Mode cmdline_value = NONE;
    if (command_line->HasSwitch(switches::kExtensionContentVerification)) {
      std::string switch_value = command_line->GetSwitchValueASCII(
          switches::kExtensionContentVerification);
      if (switch_value == switches::kExtensionContentVerificationBootstrap)
        cmdline_value = ContentVerifierDelegate::BOOTSTRAP;
      else if (switch_value == switches::kExtensionContentVerificationEnforce)
        cmdline_value = ContentVerifierDelegate::ENFORCE;
      else if (switch_value ==
              switches::kExtensionContentVerificationEnforceStrict)
        cmdline_value = ContentVerifierDelegate::ENFORCE_STRICT;
      else
        // If no value was provided (or the wrong one), just default to enforce.
        cmdline_value = ContentVerifierDelegate::ENFORCE;
    }

    // We don't want to allow the command-line flags to eg disable enforcement
    // if the experiment group says it should be on, or malware may just modify
    // the command line flags. So return the more restrictive of the 2 values.
    return std::max(experiment_value, cmdline_value);
  }

 private:
  base::WeakPtr<ExtensionService> service_;
  ContentVerifierDelegate::Mode default_mode_;

  // For reporting metrics in BOOTSTRAP mode, when an extension would be
  // disabled if content verification was in ENFORCE mode.
  std::set<std::string> would_be_disabled_ids_;

  DISALLOW_COPY_AND_ASSIGN(ContentVerifierDelegateImpl);
};

}  // namespace

void ExtensionSystemImpl::Shared::Init(bool extensions_enabled) {
  const CommandLine* command_line = CommandLine::ForCurrentProcess();

  navigation_observer_.reset(new NavigationObserver(profile_));

  bool allow_noisy_errors = !command_line->HasSwitch(switches::kNoErrorDialogs);
  ExtensionErrorReporter::Init(allow_noisy_errors);

  user_script_master_.reset(new UserScriptMaster(profile_));

  // ExtensionService depends on RuntimeData.
  runtime_data_.reset(new RuntimeData(ExtensionRegistry::Get(profile_)));

  bool autoupdate_enabled = !profile_->IsGuestSession();
#if defined(OS_CHROMEOS)
  if (!extensions_enabled)
    autoupdate_enabled = false;
#endif
  extension_service_.reset(new ExtensionService(
      profile_,
      CommandLine::ForCurrentProcess(),
      profile_->GetPath().AppendASCII(extensions::kInstallDirectoryName),
      ExtensionPrefs::Get(profile_),
      blacklist_.get(),
      autoupdate_enabled,
      extensions_enabled,
      &ready_));

  // These services must be registered before the ExtensionService tries to
  // load any extensions.
  {
    install_verifier_.reset(
        new InstallVerifier(ExtensionPrefs::Get(profile_), profile_));
    install_verifier_->Init();
    content_verifier_ = new ContentVerifier(
        profile_, new ContentVerifierDelegateImpl(extension_service_.get()));
    ContentVerifierDelegate::Mode mode =
        ContentVerifierDelegateImpl::GetDefaultMode();
#if defined(OS_CHROMEOS)
    mode = std::max(mode, ContentVerifierDelegate::BOOTSTRAP);
#endif
    if (mode >= ContentVerifierDelegate::BOOTSTRAP)
      content_verifier_->Start();
    info_map()->SetContentVerifier(content_verifier_.get());

    management_policy_.reset(new ManagementPolicy);
    RegisterManagementPolicyProviders();
  }

  bool skip_session_extensions = false;
#if defined(OS_CHROMEOS)
  // Skip loading session extensions if we are not in a user session.
  skip_session_extensions = !chromeos::LoginState::Get()->IsUserLoggedIn();
  if (chrome::IsRunningInForcedAppMode()) {
    extension_service_->component_loader()->
        AddDefaultComponentExtensionsForKioskMode(skip_session_extensions);
  } else {
    extension_service_->component_loader()->AddDefaultComponentExtensions(
        skip_session_extensions);
  }
#else
  extension_service_->component_loader()->AddDefaultComponentExtensions(
      skip_session_extensions);
#endif
  if (command_line->HasSwitch(switches::kLoadComponentExtension)) {
    CommandLine::StringType path_list = command_line->GetSwitchValueNative(
        switches::kLoadComponentExtension);
    base::StringTokenizerT<CommandLine::StringType,
        CommandLine::StringType::const_iterator> t(path_list,
                                                   FILE_PATH_LITERAL(","));
    while (t.GetNext()) {
      // Load the component extension manifest synchronously.
      // Blocking the UI thread is acceptable here since
      // this flag designated for developers.
      base::ThreadRestrictions::ScopedAllowIO allow_io;
      extension_service_->component_loader()->AddOrReplace(
          base::FilePath(t.token()));
    }
  }
  extension_service_->Init();

  // Make the chrome://extension-icon/ resource available.
  content::URLDataSource::Add(profile_, new ExtensionIconSource(profile_));

  extension_warning_service_.reset(new ExtensionWarningService(profile_));
  extension_warning_badge_service_.reset(
      new ExtensionWarningBadgeService(profile_));
  extension_warning_service_->AddObserver(
      extension_warning_badge_service_.get());
  error_console_.reset(new ErrorConsole(profile_));
  quota_service_.reset(new QuotaService);

  if (extensions_enabled) {
    // Load any extensions specified with --load-extension.
    // TODO(yoz): Seems like this should move into ExtensionService::Init.
    // But maybe it's no longer important.
    if (command_line->HasSwitch(switches::kLoadExtension)) {
      CommandLine::StringType path_list = command_line->GetSwitchValueNative(
          switches::kLoadExtension);
      base::StringTokenizerT<CommandLine::StringType,
          CommandLine::StringType::const_iterator> t(path_list,
                                                     FILE_PATH_LITERAL(","));
      while (t.GetNext()) {
        std::string extension_id;
        UnpackedInstaller::Create(extension_service_.get())->
            LoadFromCommandLine(base::FilePath(t.token()), &extension_id);
      }
    }
  }
}

void ExtensionSystemImpl::Shared::Shutdown() {
  if (extension_warning_service_) {
    extension_warning_service_->RemoveObserver(
        extension_warning_badge_service_.get());
  }
  if (content_verifier_)
    content_verifier_->Shutdown();
  if (extension_service_)
    extension_service_->Shutdown();
}

StateStore* ExtensionSystemImpl::Shared::state_store() {
  return state_store_.get();
}

StateStore* ExtensionSystemImpl::Shared::rules_store() {
  return rules_store_.get();
}

ExtensionService* ExtensionSystemImpl::Shared::extension_service() {
  return extension_service_.get();
}

RuntimeData* ExtensionSystemImpl::Shared::runtime_data() {
  return runtime_data_.get();
}

ManagementPolicy* ExtensionSystemImpl::Shared::management_policy() {
  return management_policy_.get();
}

UserScriptMaster* ExtensionSystemImpl::Shared::user_script_master() {
  return user_script_master_.get();
}

InfoMap* ExtensionSystemImpl::Shared::info_map() {
  if (!extension_info_map_.get())
    extension_info_map_ = new InfoMap();
  return extension_info_map_.get();
}

LazyBackgroundTaskQueue*
    ExtensionSystemImpl::Shared::lazy_background_task_queue() {
  return lazy_background_task_queue_.get();
}

EventRouter* ExtensionSystemImpl::Shared::event_router() {
  return event_router_.get();
}

ExtensionWarningService* ExtensionSystemImpl::Shared::warning_service() {
  return extension_warning_service_.get();
}

Blacklist* ExtensionSystemImpl::Shared::blacklist() {
  return blacklist_.get();
}

ErrorConsole* ExtensionSystemImpl::Shared::error_console() {
  return error_console_.get();
}

InstallVerifier* ExtensionSystemImpl::Shared::install_verifier() {
  return install_verifier_.get();
}

QuotaService* ExtensionSystemImpl::Shared::quota_service() {
  return quota_service_.get();
}

ContentVerifier* ExtensionSystemImpl::Shared::content_verifier() {
  return content_verifier_.get();
}

//
// ExtensionSystemImpl
//

ExtensionSystemImpl::ExtensionSystemImpl(Profile* profile)
    : profile_(profile) {
  shared_ = ExtensionSystemSharedFactory::GetForBrowserContext(profile);

  if (profile->IsOffTheRecord()) {
    process_manager_.reset(ProcessManager::Create(profile));
  } else {
    shared_->InitPrefs();
  }
}

ExtensionSystemImpl::~ExtensionSystemImpl() {
}

void ExtensionSystemImpl::Shutdown() {
  process_manager_.reset();
}

void ExtensionSystemImpl::InitForRegularProfile(bool extensions_enabled) {
  DCHECK(!profile_->IsOffTheRecord());
  if (user_script_master() || extension_service())
    return;  // Already initialized.

  // The InfoMap needs to be created before the ProcessManager.
  shared_->info_map();

  process_manager_.reset(ProcessManager::Create(profile_));

  shared_->Init(extensions_enabled);
}

ExtensionService* ExtensionSystemImpl::extension_service() {
  return shared_->extension_service();
}

RuntimeData* ExtensionSystemImpl::runtime_data() {
  return shared_->runtime_data();
}

ManagementPolicy* ExtensionSystemImpl::management_policy() {
  return shared_->management_policy();
}

UserScriptMaster* ExtensionSystemImpl::user_script_master() {
  return shared_->user_script_master();
}

ProcessManager* ExtensionSystemImpl::process_manager() {
  return process_manager_.get();
}

StateStore* ExtensionSystemImpl::state_store() {
  return shared_->state_store();
}

StateStore* ExtensionSystemImpl::rules_store() {
  return shared_->rules_store();
}

InfoMap* ExtensionSystemImpl::info_map() { return shared_->info_map(); }

LazyBackgroundTaskQueue* ExtensionSystemImpl::lazy_background_task_queue() {
  return shared_->lazy_background_task_queue();
}

EventRouter* ExtensionSystemImpl::event_router() {
  return shared_->event_router();
}

ExtensionWarningService* ExtensionSystemImpl::warning_service() {
  return shared_->warning_service();
}

Blacklist* ExtensionSystemImpl::blacklist() {
  return shared_->blacklist();
}

const OneShotEvent& ExtensionSystemImpl::ready() const {
  return shared_->ready();
}

ErrorConsole* ExtensionSystemImpl::error_console() {
  return shared_->error_console();
}

InstallVerifier* ExtensionSystemImpl::install_verifier() {
  return shared_->install_verifier();
}

QuotaService* ExtensionSystemImpl::quota_service() {
  return shared_->quota_service();
}

ContentVerifier* ExtensionSystemImpl::content_verifier() {
  return shared_->content_verifier();
}

scoped_ptr<ExtensionSet> ExtensionSystemImpl::GetDependentExtensions(
    const Extension* extension) {
  return extension_service()->shared_module_service()->GetDependentExtensions(
      extension);
}

void ExtensionSystemImpl::RegisterExtensionWithRequestContexts(
    const Extension* extension) {
  base::Time install_time;
  if (extension->location() != Manifest::COMPONENT) {
    install_time = ExtensionPrefs::Get(profile_)->
        GetInstallTime(extension->id());
  }
  bool incognito_enabled = util::IsIncognitoEnabled(extension->id(), profile_);

  bool notifications_disabled = false;
#if defined(ENABLE_NOTIFICATIONS)
  message_center::NotifierId notifier_id(
      message_center::NotifierId::APPLICATION,
      extension->id());

  DesktopNotificationService* notification_service =
      DesktopNotificationServiceFactory::GetForProfile(profile_);
  notifications_disabled =
      !notification_service->IsNotifierEnabled(notifier_id);
#endif

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&InfoMap::AddExtension, info_map(),
                 make_scoped_refptr(extension), install_time,
                 incognito_enabled, notifications_disabled));
}

void ExtensionSystemImpl::UnregisterExtensionWithRequestContexts(
    const std::string& extension_id,
    const UnloadedExtensionInfo::Reason reason) {
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&InfoMap::RemoveExtension, info_map(), extension_id, reason));
}

}  // namespace extensions
