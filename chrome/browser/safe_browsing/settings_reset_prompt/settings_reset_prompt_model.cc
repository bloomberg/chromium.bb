// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_model.h"

#include <utility>

#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profile_resetter/brandcode_config_fetcher.h"
#include "chrome/browser/profile_resetter/brandcoded_default_settings.h"
#include "chrome/browser/profile_resetter/resettable_settings_snapshot.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_config.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/extensions/manifest_handlers/settings_overrides_handler.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/url_formatter/url_fixer.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_set.h"

namespace safe_browsing {

namespace {

#if defined(GOOGLE_CHROME_BUILD)
constexpr char kOmahaUrl[] = "https://tools.google.com/service/update2";
#endif  // defined(GOOGLE_CHROME_BUILD)

// These values are used for UMA metrics reporting. New enum values can be
// added, but existing enums must never be renumbered or deleted and reused.
enum SettingsReset {
  SETTINGS_RESET_HOMEPAGE = 1,
  SETTINGS_RESET_DEFAULT_SEARCH = 2,
  SETTINGS_RESET_STARTUP_URLS = 3,
  SETTINGS_RESET_MAX,
};

// Used to keep track of which settings types have been initialized in
// |SettingsResetPromptModel|.
enum SettingsType : uint32_t {
  SETTINGS_TYPE_HOMEPAGE = 1 << 0,
  SETTINGS_TYPE_DEFAULT_SEARCH = 1 << 1,
  SETTINGS_TYPE_STARTUP_URLS = 1 << 2,
  SETTINGS_TYPE_ALL = SETTINGS_TYPE_HOMEPAGE | SETTINGS_TYPE_DEFAULT_SEARCH |
                      SETTINGS_TYPE_STARTUP_URLS,
};

// A helper class that fetches default settings to be used for the settings
// reset prompt. The static |FetchDefaultSettings()| function will create and
// manage the lifetime of |DefaultSettingsFetcher| instances.
class DefaultSettingsFetcher {
 public:
  using SettingsCallback =
      base::Callback<void(std::unique_ptr<BrandcodedDefaultSettings>)>;

  // Fetches default settings and passes the corresponding
  // |BrandcodedDefaultSettings| object to |callback| on the UI thread. The
  // function should be called on the UI thread as well.
  static void FetchDefaultSettings(SettingsCallback callback);

 private:
  // Instances of |DefaultSettingsFetcher| own themselves and will delete
  // themselves once default settings have been fetched and |callback| has been
  // posted on the UI thread.
  //
  // The main reason for this design is that |BrandcodeConfigFetcher| takes a
  // callback and initiates the fetching process in its constructor, and we need
  // to hold on to the instance of the fetcher until settings have been
  // fetched. This design saves us from having to explicitly manage global
  // |BrandcodeConfigFetcher| instances.
  explicit DefaultSettingsFetcher(SettingsCallback callback);
  ~DefaultSettingsFetcher();

  // Starts the process of fetching default settings and will ensure that
  // |PostCallbackAndDeleteSelf| is called once settings have been fetched.
  void Start();
  void OnSettingsFetched();
  // Posts a call to |callback_| on the UI thread, passing to it
  // |default_settings|, and deletes |this|.
  void PostCallbackAndDeleteSelf(
      std::unique_ptr<BrandcodedDefaultSettings> default_settings);

  std::unique_ptr<BrandcodeConfigFetcher> config_fetcher_;
  SettingsCallback callback_;
};

// static
void DefaultSettingsFetcher::FetchDefaultSettings(SettingsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // |settings_fetcher| will delete itself when default settings have been
  // fetched after the call to |Start()|.
  DefaultSettingsFetcher* settings_fetcher =
      new DefaultSettingsFetcher(std::move(callback));
  settings_fetcher->Start();
}

DefaultSettingsFetcher::DefaultSettingsFetcher(SettingsCallback callback)
    : callback_(std::move(callback)) {}

DefaultSettingsFetcher::~DefaultSettingsFetcher() {}

void DefaultSettingsFetcher::Start() {
  DCHECK(!config_fetcher_);

#if defined(GOOGLE_CHROME_BUILD)
  std::string brandcode;
  if (google_brand::GetBrand(&brandcode) && !brandcode.empty()) {
    config_fetcher_.reset(new BrandcodeConfigFetcher(
        base::Bind(&DefaultSettingsFetcher::OnSettingsFetched,
                   base::Unretained(this)),
        GURL(kOmahaUrl), brandcode));
    return;
  }
#endif  // defined(GOOGLE_CHROME_BUILD)

  // For non Google Chrome builds and cases with an empty |brandcode|, we create
  // a default-constructed |BrandcodedDefaultSettings| object and post the
  // callback immediately.
  PostCallbackAndDeleteSelf(base::MakeUnique<BrandcodedDefaultSettings>());
}

void DefaultSettingsFetcher::OnSettingsFetched() {
  DCHECK(config_fetcher_);
  DCHECK(!config_fetcher_->IsActive());

  PostCallbackAndDeleteSelf(config_fetcher_->GetSettings());
}

void DefaultSettingsFetcher::PostCallbackAndDeleteSelf(
    std::unique_ptr<BrandcodedDefaultSettings> default_settings) {
  DCHECK(default_settings);
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(std::move(callback_), base::Passed(&default_settings)));
  delete this;
}

const extensions::Extension* GetExtension(
    Profile* profile,
    const extensions::ExtensionId& extension_id) {
  return extensions::ExtensionRegistry::Get(profile)->GetInstalledExtension(
      extension_id);
}

GURL FixupUrl(const std::string& url_text) {
  return url_formatter::FixupURL(url_text, /*desired_tld=*/std::string());
}

}  // namespace

// static
void SettingsResetPromptModel::Create(
    Profile* profile,
    std::unique_ptr<SettingsResetPromptConfig> prompt_config,
    CreateCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile);
  DCHECK(prompt_config);

  DefaultSettingsFetcher::FetchDefaultSettings(
      base::Bind(SettingsResetPromptModel::OnSettingsFetched, profile,
                 base::Passed(&prompt_config), base::Passed(&callback)));
}

// static
std::unique_ptr<SettingsResetPromptModel>
SettingsResetPromptModel::CreateForTesting(
    Profile* profile,
    std::unique_ptr<SettingsResetPromptConfig> prompt_config,
    std::unique_ptr<ResettableSettingsSnapshot> settings_snapshot,
    std::unique_ptr<BrandcodedDefaultSettings> default_settings,
    std::unique_ptr<ProfileResetter> profile_resetter) {
  return base::WrapUnique(new SettingsResetPromptModel(
      profile, std::move(prompt_config), std::move(settings_snapshot),
      std::move(default_settings), std::move(profile_resetter)));
}

SettingsResetPromptModel::~SettingsResetPromptModel() {}

Profile* SettingsResetPromptModel::profile() const {
  return profile_;
}

SettingsResetPromptConfig* SettingsResetPromptModel::config() const {
  return prompt_config_.get();
}

bool SettingsResetPromptModel::ShouldPromptForReset() const {
  return SomeSettingRequiresReset();
}

void SettingsResetPromptModel::PerformReset(
    const base::Closure& done_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // |default_settings_| is set in the constructor and will be passed on to
  // |profile_resetter_| who will take over ownership. This method should never
  // be called more than once during the lifetime of this object.
  DCHECK(default_settings_);

  // Disable all extensions that override settings that need to be reset.
  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  DCHECK(extension_service);
  for (const auto& item : extensions_to_disable()) {
    const extensions::ExtensionId& extension_id = item.first;
    extension_service->DisableExtension(
        extension_id, extensions::Extension::DISABLE_USER_ACTION);
  }
  UMA_HISTOGRAM_COUNTS_100("SettingsResetPrompt.NumberOfExtensionsDisabled",
                           extensions_to_disable().size());

  // Disable all the settings that need to be reset.
  ProfileResetter::ResettableFlags reset_flags = 0;
  if (homepage_reset_state() == RESET_REQUIRED) {
    reset_flags |= ProfileResetter::HOMEPAGE;
    UMA_HISTOGRAM_ENUMERATION("SettingsResetPrompt.SettingsReset",
                              SETTINGS_RESET_HOMEPAGE, SETTINGS_RESET_MAX);
  }
  if (default_search_reset_state() == RESET_REQUIRED) {
    reset_flags |= ProfileResetter::DEFAULT_SEARCH_ENGINE;
    UMA_HISTOGRAM_ENUMERATION("SettingsResetPrompt.SettingsReset",
                              SETTINGS_RESET_DEFAULT_SEARCH,
                              SETTINGS_RESET_MAX);
  }
  if (startup_urls_reset_state() == RESET_REQUIRED) {
    reset_flags |= ProfileResetter::STARTUP_PAGES;
    UMA_HISTOGRAM_ENUMERATION("SettingsResetPrompt.SettingsReset",
                              SETTINGS_RESET_STARTUP_URLS, SETTINGS_RESET_MAX);
  }

  profile_resetter_->Reset(reset_flags, std::move(default_settings_),
                           done_callback);
}

void SettingsResetPromptModel::DialogShown() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(SomeSettingRequiresReset());

  base::Time now = base::Time::Now();
  if (default_search_reset_state() == RESET_REQUIRED)
    prefs_manager_.RecordPromptShownForDefaultSearch(now);
  if (startup_urls_reset_state() == RESET_REQUIRED)
    prefs_manager_.RecordPromptShownForStartupUrls(now);
  if (homepage_reset_state() == RESET_REQUIRED)
    prefs_manager_.RecordPromptShownForHomepage(now);
}

GURL SettingsResetPromptModel::homepage() const {
  return homepage_url_;
}

SettingsResetPromptModel::ResetState
SettingsResetPromptModel::homepage_reset_state() const {
  DCHECK(homepage_reset_state_ != RESET_REQUIRED ||
         homepage_reset_domain_id_ >= 0);
  return homepage_reset_state_;
}

GURL SettingsResetPromptModel::default_search() const {
  return default_search_url_;
}

SettingsResetPromptModel::ResetState
SettingsResetPromptModel::default_search_reset_state() const {
  DCHECK(default_search_reset_state_ != RESET_REQUIRED ||
         default_search_reset_domain_id_ >= 0);
  return default_search_reset_state_;
}

const std::vector<GURL>& SettingsResetPromptModel::startup_urls() const {
  return startup_urls_;
}

const std::vector<GURL>& SettingsResetPromptModel::startup_urls_to_reset()
    const {
  return startup_urls_to_reset_;
}

SettingsResetPromptModel::ResetState
SettingsResetPromptModel::startup_urls_reset_state() const {
  return startup_urls_reset_state_;
}

const SettingsResetPromptModel::ExtensionMap&
SettingsResetPromptModel::extensions_to_disable() const {
  return extensions_to_disable_;
}

void SettingsResetPromptModel::ReportUmaMetrics() const {
  UMA_HISTOGRAM_BOOLEAN("SettingsResetPrompt.PromptRequired",
                        ShouldPromptForReset());
  UMA_HISTOGRAM_ENUMERATION("SettingsResetPrompt.ResetState_DefaultSearch",
                            default_search_reset_state(), RESET_STATE_MAX);
  UMA_HISTOGRAM_ENUMERATION("SettingsResetPrompt.ResetState_StartupUrls",
                            startup_urls_reset_state(), RESET_STATE_MAX);
  UMA_HISTOGRAM_ENUMERATION("SettingsResetPrompt.ResetState_Homepage",
                            homepage_reset_state(), RESET_STATE_MAX);
  UMA_HISTOGRAM_COUNTS_100("SettingsResetPrompt.NumberOfExtensionsToDisable",
                           extensions_to_disable().size());
  UMA_HISTOGRAM_SPARSE_SLOWLY(
      "SettingsResetPrompt.DelayBeforePromptParam",
      prompt_config_->delay_before_prompt().InSeconds());
}

// static
void SettingsResetPromptModel::OnSettingsFetched(
    Profile* profile,
    std::unique_ptr<SettingsResetPromptConfig> prompt_config,
    SettingsResetPromptModel::CreateCallback callback,
    std::unique_ptr<BrandcodedDefaultSettings> default_settings) {
  DCHECK(profile);
  DCHECK(prompt_config);
  DCHECK(default_settings);

  callback.Run(base::WrapUnique(new SettingsResetPromptModel(
      profile, std::move(prompt_config),
      base::MakeUnique<ResettableSettingsSnapshot>(profile),
      std::move(default_settings),
      base::MakeUnique<ProfileResetter>(profile))));
}

SettingsResetPromptModel::SettingsResetPromptModel(
    Profile* profile,
    std::unique_ptr<SettingsResetPromptConfig> prompt_config,
    std::unique_ptr<ResettableSettingsSnapshot> settings_snapshot,
    std::unique_ptr<BrandcodedDefaultSettings> default_settings,
    std::unique_ptr<ProfileResetter> profile_resetter)
    : profile_(profile),
      prefs_manager_(profile, prompt_config->prompt_wave()),
      prompt_config_(std::move(prompt_config)),
      settings_snapshot_(std::move(settings_snapshot)),
      default_settings_(std::move(default_settings)),
      profile_resetter_(std::move(profile_resetter)),
      time_since_last_prompt_(base::Time::Now() -
                              prefs_manager_.LastTriggeredPrompt()),
      settings_types_initialized_(0),
      homepage_reset_domain_id_(-1),
      homepage_reset_state_(NO_RESET_REQUIRED_DUE_TO_DOMAIN_NOT_MATCHED),
      default_search_reset_domain_id_(-1),
      default_search_reset_state_(NO_RESET_REQUIRED_DUE_TO_DOMAIN_NOT_MATCHED),
      startup_urls_reset_state_(NO_RESET_REQUIRED_DUE_TO_DOMAIN_NOT_MATCHED) {
  DCHECK(profile_);
  DCHECK(prompt_config_);
  DCHECK(settings_snapshot_);
  DCHECK(default_settings_);
  DCHECK(profile_resetter_);

  InitDefaultSearchData();
  InitStartupUrlsData();
  InitHomepageData();
  DCHECK_EQ(settings_types_initialized_, SETTINGS_TYPE_ALL);

  InitExtensionData();

  if (!SomeSettingRequiresReset())
    return;

  // For now, during the experimental phase, if policy controls any of the
  // settings that we consider for reset (search, startup pages, homepage) or if
  // an extension that needs to be disabled is managed by policy, then we do not
  // show the reset prompt.
  //
  // TODO(alito): Consider how clients with policies should be prompted for
  // reset.
  if (SomeSettingIsManaged() || SomeExtensionMustRemainEnabled()) {
    if (homepage_reset_state_ == RESET_REQUIRED)
      homepage_reset_state_ = NO_RESET_REQUIRED_DUE_TO_POLICY;
    if (default_search_reset_state_ == RESET_REQUIRED)
      default_search_reset_state_ = NO_RESET_REQUIRED_DUE_TO_POLICY;
    if (startup_urls_reset_state_ == RESET_REQUIRED)
      startup_urls_reset_state_ = NO_RESET_REQUIRED_DUE_TO_POLICY;
  }
}

void SettingsResetPromptModel::InitDefaultSearchData() {
  // Default search data must be the first setting type to be initialized.
  DCHECK_EQ(settings_types_initialized_, 0U);

  settings_types_initialized_ |= SETTINGS_TYPE_DEFAULT_SEARCH;

  default_search_url_ = FixupUrl(settings_snapshot_->dse_url());
  default_search_reset_domain_id_ =
      prompt_config_->UrlToResetDomainId(default_search_url_);
  if (default_search_reset_domain_id_ < 0)
    return;

  default_search_reset_state_ = GetResetStateForSetting(
      prefs_manager_.LastTriggeredPromptForDefaultSearch());
}

void SettingsResetPromptModel::InitStartupUrlsData() {
  // Default search data must have been initialized before startup URLs data.
  DCHECK_EQ(settings_types_initialized_, SETTINGS_TYPE_DEFAULT_SEARCH);

  settings_types_initialized_ |= SETTINGS_TYPE_STARTUP_URLS;

  // Only the SessionStartupPref::URLS startup type is a candidate for
  // resetting.
  if (settings_snapshot_->startup_type() != SessionStartupPref::URLS)
    return;

  for (const GURL& startup_url : settings_snapshot_->startup_urls()) {
    GURL fixed_url = FixupUrl(startup_url.possibly_invalid_spec());
    startup_urls_.push_back(fixed_url);
    int reset_domain_id = prompt_config_->UrlToResetDomainId(fixed_url);
    if (reset_domain_id >= 0) {
      startup_urls_to_reset_.push_back(fixed_url);
      domain_ids_for_startup_urls_to_reset_.insert(reset_domain_id);
    }
  }

  if (startup_urls_to_reset_.empty())
    return;

  startup_urls_reset_state_ = GetResetStateForSetting(
      prefs_manager_.LastTriggeredPromptForStartupUrls());
}

void SettingsResetPromptModel::InitHomepageData() {
  // Homepage data must be initialized after default search and startup URLs
  // data.
  DCHECK_EQ(settings_types_initialized_,
            SETTINGS_TYPE_DEFAULT_SEARCH | SETTINGS_TYPE_STARTUP_URLS);

  settings_types_initialized_ |= SETTINGS_TYPE_HOMEPAGE;

  homepage_url_ = FixupUrl(settings_snapshot_->homepage());

  // If the home button is not visible to the user, then the homepage setting
  // has no real user-visible effect.
  if (!settings_snapshot_->show_home_button())
    return;

  // We do not currently support resetting New Tab pages that are set by
  // extensions.
  if (settings_snapshot_->homepage_is_ntp())
    return;

  homepage_reset_domain_id_ = prompt_config_->UrlToResetDomainId(homepage_url_);
  if (homepage_reset_domain_id_ < 0)
    return;

  homepage_reset_state_ =
      GetResetStateForSetting(prefs_manager_.LastTriggeredPromptForHomepage());
}

// Populate |extensions_to_disable_| with all enabled extensions that override
// the settings whose values were determined to need resetting. Note that all
// extensions that override such settings are included in the list, not just the
// one that is currently actively overriding the setting, in order to ensure
// that default values can be restored. This function should be called after
// other Init*() functions.
void SettingsResetPromptModel::InitExtensionData() {
  DCHECK_EQ(settings_types_initialized_, SETTINGS_TYPE_ALL);

  // |enabled_extensions()| is a container of [id, name] pairs.
  for (const auto& id_name : settings_snapshot_->enabled_extensions()) {
    // Just in case there are duplicates in the list of enabled extensions.
    if (extensions_to_disable_.find(id_name.first) !=
        extensions_to_disable_.end()) {
      continue;
    }

    const extensions::Extension* extension =
        GetExtension(profile_, id_name.first);
    if (!extension)
      continue;

    const extensions::SettingsOverrides* overrides =
        extensions::SettingsOverrides::Get(extension);
    if (!overrides)
      continue;

    if ((homepage_reset_state_ == RESET_REQUIRED && overrides->homepage) ||
        (default_search_reset_state_ == RESET_REQUIRED &&
         overrides->search_engine) ||
        (startup_urls_reset_state_ == RESET_REQUIRED &&
         !overrides->startup_pages.empty())) {
      ExtensionInfo extension_info(extension);
      extensions_to_disable_.insert(
          std::make_pair(extension_info.id, extension_info));
    }
  }
}

SettingsResetPromptModel::ResetState
SettingsResetPromptModel::GetResetStateForSetting(
    const base::Time& last_triggered_for_setting) const {
  if (!last_triggered_for_setting.is_null())
    return NO_RESET_REQUIRED_DUE_TO_ALREADY_PROMPTED_FOR_SETTING;

  if (time_since_last_prompt_ < prompt_config_->time_between_prompts())
    return NO_RESET_REQUIRED_DUE_TO_RECENTLY_PROMPTED;

  if (SomeSettingRequiresReset())
    return NO_RESET_REQUIRED_DUE_TO_OTHER_SETTING_REQUIRING_RESET;

  return RESET_REQUIRED;
}

bool SettingsResetPromptModel::SomeSettingRequiresReset() const {
  return default_search_reset_state_ == RESET_REQUIRED ||
         startup_urls_reset_state_ == RESET_REQUIRED ||
         homepage_reset_state_ == RESET_REQUIRED;
}

bool SettingsResetPromptModel::SomeSettingIsManaged() const {
  PrefService* prefs = profile_->GetPrefs();
  DCHECK(prefs);

  // Check if homepage is managed.
  const PrefService::Preference* homepage =
      prefs->FindPreference(prefs::kHomePage);
  if (homepage && (homepage->IsManaged() || homepage->IsManagedByCustodian()))
    return true;

  // Check if startup pages are managed.
  if (SessionStartupPref::TypeIsManaged(prefs) ||
      SessionStartupPref::URLsAreManaged(prefs)) {
    return true;
  }

  // Check if default search is managed.
  TemplateURLService* service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  if (service && service->is_default_search_managed())
    return true;

  return false;
}

bool SettingsResetPromptModel::SomeExtensionMustRemainEnabled() const {
  extensions::ManagementPolicy* management_policy =
      extensions::ExtensionSystem::Get(profile_)->management_policy();

  if (management_policy) {
    for (const auto& item : extensions_to_disable()) {
      const extensions::ExtensionId& extension_id = item.first;
      const extensions::Extension* extension =
          GetExtension(profile_, extension_id);
      if (extension && management_policy->MustRemainEnabled(extension, nullptr))
        return true;
    }
  }

  return false;
}

}  // namespace safe_browsing.
