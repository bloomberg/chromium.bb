// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/options_ui.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_about_handler.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/browser/ui/webui/options/autofill_options_handler.h"
#include "chrome/browser/ui/webui/options/automatic_settings_reset_handler.h"
#include "chrome/browser/ui/webui/options/browser_options_handler.h"
#include "chrome/browser/ui/webui/options/clear_browser_data_handler.h"
#include "chrome/browser/ui/webui/options/content_settings_handler.h"
#include "chrome/browser/ui/webui/options/cookies_view_handler.h"
#include "chrome/browser/ui/webui/options/core_options_handler.h"
#include "chrome/browser/ui/webui/options/create_profile_handler.h"
#include "chrome/browser/ui/webui/options/easy_unlock_handler.h"
#include "chrome/browser/ui/webui/options/font_settings_handler.h"
#include "chrome/browser/ui/webui/options/handler_options_handler.h"
#include "chrome/browser/ui/webui/options/help_overlay_handler.h"
#include "chrome/browser/ui/webui/options/home_page_overlay_handler.h"
#include "chrome/browser/ui/webui/options/import_data_handler.h"
#include "chrome/browser/ui/webui/options/language_dictionary_overlay_handler.h"
#include "chrome/browser/ui/webui/options/language_options_handler.h"
#include "chrome/browser/ui/webui/options/manage_profile_handler.h"
#include "chrome/browser/ui/webui/options/media_devices_selection_handler.h"
#include "chrome/browser/ui/webui/options/password_manager_handler.h"
#include "chrome/browser/ui/webui/options/reset_profile_settings_handler.h"
#include "chrome/browser/ui/webui/options/search_engine_manager_handler.h"
#include "chrome/browser/ui/webui/options/startup_pages_handler.h"
#include "chrome/browser/ui/webui/options/sync_setup_handler.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/features.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/locale_settings.h"
#include "chrome/grit/options_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_ui.h"
#include "net/base/escape.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/template_expressions.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/base/webui/web_ui_util.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/ui/webui/options/supervised_user_create_confirm_handler.h"
#include "chrome/browser/ui/webui/options/supervised_user_import_handler.h"
#include "chrome/browser/ui/webui/options/supervised_user_learn_more_handler.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/system/pointer_device_observer.h"
#include "chrome/browser/ui/webui/options/chromeos/accounts_options_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/bluetooth_options_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/change_picture_options_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/core_chromeos_options_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/cros_language_options_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/date_time_options_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/display_options_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/display_overscan_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/internet_options_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/keyboard_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/options_stylus_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/pointer_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/power_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/proxy_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/stats_options_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/storage_manager_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/user_image_source.h"
#endif

#if defined(USE_NSS_CERTS)
#include "chrome/browser/ui/webui/options/certificate_manager_handler.h"
#endif

#if BUILDFLAG(ENABLE_GOOGLE_NOW)
#include "chrome/browser/ui/webui/options/geolocation_options_handler.h"
#endif

using content::RenderViewHost;

namespace {

const char kLocalizedStringsFile[] = "strings.js";
const char kOptionsBundleJsFile[]  = "options_bundle.js";

#if defined(OS_CHROMEOS)
constexpr char kIconsHTMLPath[] = "icons.html";
constexpr char kPinKeyboardHTMLPath[] = "people_page/pin_keyboard.html";
constexpr char kPinKeyboardJSPath[] = "people_page/pin_keyboard.js";
constexpr char kPasswordPromptDialogHTMLPath[] =
    "people_page/password_prompt_dialog.html";
constexpr char kPasswordPromptDialogJSPath[] =
    "people_page/password_prompt_dialog.js";
constexpr char kLockStateBehaviorHTMLPath[] =
    "people_page/lock_state_behavior.html";
constexpr char kLockStateBehaviorJSPath[] =
    "people_page/lock_state_behavior.js";
constexpr char kLockScreenHTMLPath[] = "people_page/lock_screen.html";
constexpr char kLockScreenJSPath[] = "people_page/lock_screen.js";
constexpr char kSetupPinHTMLPath[] = "people_page/setup_pin_dialog.html";
constexpr char kSetupPinJSPath[] = "people_page/setup_pin_dialog.js";
constexpr char kSettingsRouteHTMLPath[] = "route.html";
constexpr char kSettingsRouteJSPath[] = "route.js";
constexpr char kSettingsSharedCSSHTMLPath[] = "settings_shared_css.html";
constexpr char kSettingsVarsCSSHTMLPath[] = "settings_vars_css.html";
constexpr char kSettingsPrefsBehaviorHTMLPath[] = "prefs/prefs_behavior.html";
constexpr char kSettingsPrefsBehaviorJSPath[] = "prefs/prefs_behavior.js";
constexpr char kSettingsPrefsTypesHTMLPath[] = "prefs/prefs_types.html";
constexpr char kSettingsPrefsTypesJSPath[] = "prefs/prefs_types.js";
constexpr char kOptionsPolymerHTMLPath[] = "options_polymer.html";
#endif

}  // namespace

namespace options {

////////////////////////////////////////////////////////////////////////////////
//
// OptionsUIHTMLSource
//
////////////////////////////////////////////////////////////////////////////////

class OptionsUIHTMLSource : public content::URLDataSource {
 public:
  // The constructor takes over ownership of |localized_strings|.
  explicit OptionsUIHTMLSource(base::DictionaryValue* localized_strings);

  // content::URLDataSource implementation.
  std::string GetSource() const override;
  void StartDataRequest(
      const std::string& path,
      const content::ResourceRequestInfo::WebContentsGetter& wc_getter,
      const content::URLDataSource::GotDataCallback& callback) override;
  std::string GetMimeType(const std::string&) const override;
  bool ShouldDenyXFrameOptions() const override;

 private:
  ~OptionsUIHTMLSource() override;
  void CreateDataSourceMap();
  void AddReplacements(base::DictionaryValue* localized_strings);

  // Localized strings collection.
  std::unique_ptr<base::DictionaryValue> localized_strings_;
  std::map<std::string, int> path_to_idr_map_;
  ui::TemplateReplacements replacements_;

  DISALLOW_COPY_AND_ASSIGN(OptionsUIHTMLSource);
};

OptionsUIHTMLSource::OptionsUIHTMLSource(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  AddReplacements(localized_strings);
  localized_strings_.reset(localized_strings);
  CreateDataSourceMap();
}

std::string OptionsUIHTMLSource::GetSource() const {
  return chrome::kChromeUISettingsFrameHost;
}

void OptionsUIHTMLSource::StartDataRequest(
    const std::string& path,
    const content::ResourceRequestInfo::WebContentsGetter& wc_getter,
    const content::URLDataSource::GotDataCallback& callback) {
  scoped_refptr<base::RefCountedMemory> response_bytes;
  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  webui::SetLoadTimeDataDefaults(app_locale, localized_strings_.get());

  std::map<std::string, int>::iterator result;
  result = path_to_idr_map_.find(path);

  if (path == kLocalizedStringsFile) {
    // Return dynamically-generated strings from memory.
    std::string strings_js;
    webui::AppendJsonJS(localized_strings_.get(), &strings_js);
    response_bytes = base::RefCountedString::TakeString(&strings_js);
  } else if (path == kOptionsBundleJsFile) {
    // Return (and cache) the options javascript code.
    response_bytes = ui::ResourceBundle::GetSharedInstance().
        LoadDataResourceBytes(IDR_OPTIONS_BUNDLE_JS);
  } else if (result != path_to_idr_map_.end()) {
    response_bytes =
        ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
            result->second);
  } else {
    // Return (and cache) the main options html page as the default.
    response_bytes = ui::ResourceBundle::GetSharedInstance().
        LoadDataResourceBytes(IDR_OPTIONS_HTML);
  }

  // pre-process i18n strings
  if (GetMimeType(path) == "text/html") {
    std::string replaced = ui::ReplaceTemplateExpressions(
        base::StringPiece(response_bytes->front_as<char>(),
                          response_bytes->size()),
        replacements_);
    response_bytes = base::RefCountedString::TakeString(&replaced);
  }

  callback.Run(response_bytes.get());
}

std::string OptionsUIHTMLSource::GetMimeType(const std::string& path) const {
  if (base::EndsWith(path, ".js", base::CompareCase::INSENSITIVE_ASCII))
    return "application/javascript";

  return "text/html";
}

bool OptionsUIHTMLSource::ShouldDenyXFrameOptions() const {
  return false;
}

OptionsUIHTMLSource::~OptionsUIHTMLSource() {}

void OptionsUIHTMLSource::CreateDataSourceMap() {
#if defined(OS_CHROMEOS)
  path_to_idr_map_[kIconsHTMLPath] = IDR_OPTIONS_ICONS_HTML;
  path_to_idr_map_[kPinKeyboardHTMLPath] = IDR_OPTIONS_PIN_KEYBOARD_HTML;
  path_to_idr_map_[kPinKeyboardJSPath] = IDR_OPTIONS_PIN_KEYBOARD_JS;
  path_to_idr_map_[kPasswordPromptDialogHTMLPath] =
      IDR_OPTIONS_PASSWORD_PROMPT_DIALOG_HTML;
  path_to_idr_map_[kPasswordPromptDialogJSPath] =
      IDR_OPTIONS_PASSWORD_PROMPT_DIALOG_JS;
  path_to_idr_map_[kLockStateBehaviorHTMLPath] =
      IDR_OPTIONS_LOCK_STATE_BEHAVIOR_HTML;
  path_to_idr_map_[kLockStateBehaviorJSPath] =
      IDR_OPTIONS_LOCK_STATE_BEHAVIOR_JS;
  path_to_idr_map_[kLockScreenHTMLPath] = IDR_OPTIONS_LOCK_SCREEN_HTML;
  path_to_idr_map_[kLockScreenJSPath] = IDR_OPTIONS_LOCK_SCREEN_JS;
  path_to_idr_map_[kSetupPinHTMLPath] = IDR_OPTIONS_SETUP_PIN_DIALOG_HTML;
  path_to_idr_map_[kSetupPinJSPath] = IDR_OPTIONS_SETUP_PIN_DIALOG_JS;
  path_to_idr_map_[kSettingsRouteHTMLPath] = IDR_OPTIONS_ROUTE_HTML;
  path_to_idr_map_[kSettingsRouteJSPath] = IDR_OPTIONS_ROUTE_JS;
  path_to_idr_map_[kSettingsSharedCSSHTMLPath] = IDR_SETTINGS_SHARED_CSS_HTML;
  path_to_idr_map_[kSettingsVarsCSSHTMLPath] = IDR_SETTINGS_VARS_CSS_HTML;
  path_to_idr_map_[kSettingsPrefsBehaviorHTMLPath] =
      IDR_SETTINGS_PREFS_BEHAVIOR_HTML;
  path_to_idr_map_[kSettingsPrefsBehaviorJSPath] =
      IDR_SETTINGS_PREFS_BEHAVIOR_JS;
  path_to_idr_map_[kSettingsPrefsTypesHTMLPath] = IDR_SETTINGS_PREFS_TYPES_HTML;
  path_to_idr_map_[kSettingsPrefsTypesJSPath] = IDR_SETTINGS_PREFS_TYPES_JS;
  path_to_idr_map_[kOptionsPolymerHTMLPath] = IDR_OPTIONS_POLYMER_ELEMENTS_HTML;
#endif
}

void OptionsUIHTMLSource::AddReplacements(
    base::DictionaryValue* localized_strings) {
  for (auto it = base::DictionaryValue::Iterator(*localized_strings);
       !it.IsAtEnd(); it.Advance()) {
    std::string str_value;
    if (!it.value().GetAsString(&str_value)) {
      continue;
    }
    replacements_[it.key()] = str_value;
  }
}

////////////////////////////////////////////////////////////////////////////////
//
// OptionsPageUIHandler
//
////////////////////////////////////////////////////////////////////////////////

OptionsPageUIHandler::OptionsPageUIHandler() {
}

OptionsPageUIHandler::~OptionsPageUIHandler() {
}

bool OptionsPageUIHandler::IsEnabled() {
  return true;
}

// static
void OptionsPageUIHandler::RegisterStrings(
    base::DictionaryValue* localized_strings,
    const OptionsStringResource* resources,
    size_t length) {
  for (size_t i = 0; i < length; ++i) {
    base::string16 value;
    if (resources[i].substitution_id == 0) {
      value = l10n_util::GetStringUTF16(resources[i].id);
    } else {
      value = l10n_util::GetStringFUTF16(
          resources[i].id,
          l10n_util::GetStringUTF16(resources[i].substitution_id));
    }
    localized_strings->SetString(resources[i].name, value);
  }
}

void OptionsPageUIHandler::RegisterTitle(
    base::DictionaryValue* localized_strings,
    const std::string& variable_name,
    int title_id) {
  localized_strings->SetString(variable_name,
      l10n_util::GetStringUTF16(title_id));
  localized_strings->SetString(variable_name + "TabTitle",
      l10n_util::GetStringFUTF16(IDS_OPTIONS_TAB_TITLE,
          l10n_util::GetStringUTF16(IDS_SETTINGS_TITLE),
          l10n_util::GetStringUTF16(title_id)));
}

////////////////////////////////////////////////////////////////////////////////
//
// OptionsUI
//
////////////////////////////////////////////////////////////////////////////////

OptionsUI::OptionsUI(content::WebUI* web_ui)
    : WebUIController(web_ui),
      WebContentsObserver(web_ui->GetWebContents()),
      initialized_handlers_(false) {
  base::DictionaryValue* localized_strings = new base::DictionaryValue();
  CoreOptionsHandler* core_handler;
#if defined(OS_CHROMEOS)
  core_handler = new chromeos::options::CoreChromeOSOptionsHandler();
#else
  core_handler = new CoreOptionsHandler();
#endif
  core_handler->set_handlers_host(this);
  AddOptionsPageUIHandler(localized_strings, core_handler);

  AddOptionsPageUIHandler(localized_strings, new AutofillOptionsHandler());
  AddOptionsPageUIHandler(localized_strings,
                          new AutomaticSettingsResetHandler());

  BrowserOptionsHandler* browser_options_handler = new BrowserOptionsHandler();
  AddOptionsPageUIHandler(localized_strings, browser_options_handler);

  AddOptionsPageUIHandler(localized_strings, new ClearBrowserDataHandler());
  AddOptionsPageUIHandler(localized_strings, new ContentSettingsHandler());
  AddOptionsPageUIHandler(localized_strings, new CookiesViewHandler());
  AddOptionsPageUIHandler(localized_strings, new CreateProfileHandler());
  AddOptionsPageUIHandler(localized_strings, new EasyUnlockHandler());
  AddOptionsPageUIHandler(localized_strings, new FontSettingsHandler());
#if BUILDFLAG(ENABLE_GOOGLE_NOW)
  AddOptionsPageUIHandler(localized_strings, new GeolocationOptionsHandler());
#endif
  AddOptionsPageUIHandler(localized_strings, new options::HelpOverlayHandler());
  AddOptionsPageUIHandler(localized_strings, new HomePageOverlayHandler());
  AddOptionsPageUIHandler(localized_strings,
                          new MediaDevicesSelectionHandler());
#if defined(OS_CHROMEOS)
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::options::CrosLanguageOptionsHandler());
#else
  AddOptionsPageUIHandler(localized_strings, new LanguageOptionsHandler());
#endif
  AddOptionsPageUIHandler(localized_strings,
                          new LanguageDictionaryOverlayHandler());
  AddOptionsPageUIHandler(localized_strings, new ManageProfileHandler());
  AddOptionsPageUIHandler(localized_strings, new PasswordManagerHandler());
  AddOptionsPageUIHandler(localized_strings, new ResetProfileSettingsHandler());
  AddOptionsPageUIHandler(localized_strings, new SearchEngineManagerHandler());
  AddOptionsPageUIHandler(localized_strings, new ImportDataHandler());
  AddOptionsPageUIHandler(localized_strings, new StartupPagesHandler());
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  AddOptionsPageUIHandler(localized_strings,
                          new SupervisedUserCreateConfirmHandler());
  AddOptionsPageUIHandler(localized_strings, new SupervisedUserImportHandler());
  AddOptionsPageUIHandler(localized_strings,
                          new SupervisedUserLearnMoreHandler());
#endif
  AddOptionsPageUIHandler(localized_strings, new SyncSetupHandler());
#if defined(OS_CHROMEOS)
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::options::AccountsOptionsHandler());
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::options::BluetoothOptionsHandler());
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::options::DateTimeOptionsHandler());
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::options::DisplayOptionsHandler());
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::options::DisplayOverscanHandler());
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::options::PowerHandler());
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::options::InternetOptionsHandler());
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::options::KeyboardHandler());
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::options::OptionsStylusHandler());

  chromeos::options::PointerHandler* pointer_handler =
      new chromeos::options::PointerHandler();
  AddOptionsPageUIHandler(localized_strings, pointer_handler);

  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::options::ProxyHandler());
  AddOptionsPageUIHandler(
      localized_strings,
      new chromeos::options::ChangePictureOptionsHandler());
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::options::StatsOptionsHandler());
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::options::StorageManagerHandler());
#endif
#if defined(USE_NSS_CERTS)
  AddOptionsPageUIHandler(localized_strings,
                          new CertificateManagerHandler(false));
#endif
  AddOptionsPageUIHandler(localized_strings, new HandlerOptionsHandler());

  web_ui->AddMessageHandler(new MetricsHandler());

  // Enable extension API calls in the WebUI.
  extensions::TabHelper::CreateForWebContents(web_ui->GetWebContents());

  // |localized_strings| ownership is taken over by this constructor.
  OptionsUIHTMLSource* html_source =
      new OptionsUIHTMLSource(localized_strings);

  // Set up the chrome://settings-frame/ source.
  Profile* profile = Profile::FromWebUI(web_ui);
  content::URLDataSource::Add(profile, html_source);

  // Set up the chrome://theme/ source.
  ThemeSource* theme = new ThemeSource(profile);
  content::URLDataSource::Add(profile, theme);

#if defined(OS_CHROMEOS)
  // Set up the chrome://userimage/ source.
  chromeos::options::UserImageSource* user_image_source =
      new chromeos::options::UserImageSource();
  content::URLDataSource::Add(profile, user_image_source);

  pointer_device_observer_.reset(
      new chromeos::system::PointerDeviceObserver());
  pointer_device_observer_->AddObserver(browser_options_handler);
  pointer_device_observer_->AddObserver(pointer_handler);
#endif
}

OptionsUI::~OptionsUI() {
  // Uninitialize all registered handlers. Deleted by WebUIImpl.
  for (size_t i = 0; i < handlers_.size(); ++i)
    handlers_[i]->Uninitialize();
}

std::unique_ptr<OptionsUI::OnFinishedLoadingCallbackList::Subscription>
OptionsUI::RegisterOnFinishedLoadingCallback(const base::Closure& callback) {
  return on_finished_loading_callbacks_.Add(callback);
}

// static
void OptionsUI::ProcessAutocompleteSuggestions(
    const AutocompleteResult& result,
    base::ListValue* const suggestions) {
  for (size_t i = 0; i < result.size(); ++i) {
    const AutocompleteMatch& match = result.match_at(i);
    AutocompleteMatchType::Type type = match.type;
    if (type != AutocompleteMatchType::HISTORY_URL &&
        type != AutocompleteMatchType::HISTORY_TITLE &&
        type != AutocompleteMatchType::HISTORY_BODY &&
        type != AutocompleteMatchType::HISTORY_KEYWORD &&
        type != AutocompleteMatchType::NAVSUGGEST &&
        type != AutocompleteMatchType::NAVSUGGEST_PERSONALIZED) {
      continue;
    }
    std::unique_ptr<base::DictionaryValue> entry(new base::DictionaryValue());
    entry->SetString("title", match.description);
    entry->SetString("displayURL", match.contents);
    entry->SetString("url", match.destination_url.spec());
    suggestions->Append(std::move(entry));
  }
}

void OptionsUI::DidStartProvisionalLoadForFrame(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    bool is_error_page) {
  load_start_time_ = base::Time::Now();
  if (render_frame_host->GetRenderViewHost() ==
          web_ui()->GetWebContents()->GetRenderViewHost() &&
      validated_url.host_piece() == chrome::kChromeUISettingsFrameHost) {
    for (size_t i = 0; i < handlers_.size(); ++i)
      handlers_[i]->PageLoadStarted();
  }
}

void OptionsUI::DocumentLoadedInFrame(
    content::RenderFrameHost *render_frame_host) {
  UMA_HISTOGRAM_TIMES("Settings.LoadDocumentTime",
                      base::Time::Now() - load_start_time_);
}

void OptionsUI::DocumentOnLoadCompletedInMainFrame() {
  UMA_HISTOGRAM_TIMES("Settings.LoadCompletedTime",
                      base::Time::Now() - load_start_time_);
}

void OptionsUI::InitializeHandlers() {
  Profile* profile = Profile::FromWebUI(web_ui());
  DCHECK(!profile->IsOffTheRecord() || profile->IsGuestSession());

  // A new web page DOM has been brought up in an existing renderer, causing
  // this method to be called twice. If that happens, ignore the second call.
  if (!initialized_handlers_) {
    for (size_t i = 0; i < handlers_.size(); ++i)
      handlers_[i]->InitializeHandler();
    initialized_handlers_ = true;

#if defined(OS_CHROMEOS)
    pointer_device_observer_->Init();
#endif
  }

#if defined(OS_CHROMEOS)
  pointer_device_observer_->CheckDevices();
#endif

  // Always initialize the page as when handlers are left over we still need to
  // do various things like show/hide sections and send data to the Javascript.
  for (size_t i = 0; i < handlers_.size(); ++i)
    handlers_[i]->InitializePage();

  web_ui()->CallJavascriptFunctionUnsafe(
      "BrowserOptions.notifyInitializationComplete");
}

void OptionsUI::OnFinishedLoading() {
  on_finished_loading_callbacks_.Notify();
}

void OptionsUI::AddOptionsPageUIHandler(
    base::DictionaryValue* localized_strings,
    OptionsPageUIHandler* handler_raw) {
  std::unique_ptr<OptionsPageUIHandler> handler(handler_raw);
  DCHECK(handler.get());
  // Add only if handler's service is enabled.
  if (handler->IsEnabled()) {
    // Add handler to the list and also pass the ownership.
    web_ui()->AddMessageHandler(handler.release());
    handler_raw->GetLocalizedValues(localized_strings);
    handlers_.push_back(handler_raw);
  }
}

}  // namespace options
