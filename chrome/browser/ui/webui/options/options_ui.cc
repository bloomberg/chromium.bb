// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/options_ui.h"

#include <algorithm>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/browser_about_handler.h"
#include "chrome/browser/browser_process.h"
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
#include "chrome/browser/ui/webui/options/supervised_user_create_confirm_handler.h"
#include "chrome/browser/ui/webui/options/supervised_user_import_handler.h"
#include "chrome/browser/ui/webui/options/supervised_user_learn_more_handler.h"
#include "chrome/browser/ui/webui/options/website_settings_handler.h"
#include "chrome/browser/ui/webui/sync_setup_handler.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/locale_settings.h"
#include "components/omnibox/autocomplete_match.h"
#include "components/omnibox/autocomplete_result.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_ui.h"
#include "grit/options_resources.h"
#include "grit/theme_resources.h"
#include "net/base/escape.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/base/webui/web_ui_util.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/consumer_management_service.h"
#include "chrome/browser/chromeos/system/pointer_device_observer.h"
#include "chrome/browser/ui/webui/options/chromeos/accounts_options_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/bluetooth_options_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/change_picture_options_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/consumer_management_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/core_chromeos_options_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/cros_language_options_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/date_time_options_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/display_options_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/display_overscan_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/internet_options_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/keyboard_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/pointer_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/proxy_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/stats_options_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/user_image_source.h"
#endif

#if defined(USE_NSS)
#include "chrome/browser/ui/webui/options/certificate_manager_handler.h"
#endif

#if defined(ENABLE_GOOGLE_NOW)
#include "chrome/browser/ui/webui/options/geolocation_options_handler.h"
#endif

using content::RenderViewHost;

namespace {

const char kLocalizedStringsFile[] = "strings.js";
const char kOptionsBundleJsFile[]  = "options_bundle.js";

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
  virtual std::string GetSource() const OVERRIDE;
  virtual void StartDataRequest(
      const std::string& path,
      int render_process_id,
      int render_frame_id,
      const content::URLDataSource::GotDataCallback& callback) OVERRIDE;
  virtual std::string GetMimeType(const std::string&) const OVERRIDE;
  virtual bool ShouldDenyXFrameOptions() const OVERRIDE;

 private:
  virtual ~OptionsUIHTMLSource();

  // Localized strings collection.
  scoped_ptr<base::DictionaryValue> localized_strings_;

  DISALLOW_COPY_AND_ASSIGN(OptionsUIHTMLSource);
};

OptionsUIHTMLSource::OptionsUIHTMLSource(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  localized_strings_.reset(localized_strings);
}

std::string OptionsUIHTMLSource::GetSource() const {
  return chrome::kChromeUISettingsFrameHost;
}

void OptionsUIHTMLSource::StartDataRequest(
    const std::string& path,
    int render_process_id,
    int render_frame_id,
    const content::URLDataSource::GotDataCallback& callback) {
  scoped_refptr<base::RefCountedMemory> response_bytes;
  webui::SetFontAndTextDirection(localized_strings_.get());

  if (path == kLocalizedStringsFile) {
    // Return dynamically-generated strings from memory.
    webui::UseVersion2 version;
    std::string strings_js;
    webui::AppendJsonJS(localized_strings_.get(), &strings_js);
    response_bytes = base::RefCountedString::TakeString(&strings_js);
  } else if (path == kOptionsBundleJsFile) {
    // Return (and cache) the options javascript code.
    response_bytes = ui::ResourceBundle::GetSharedInstance().
        LoadDataResourceBytes(IDR_OPTIONS_BUNDLE_JS);
  } else {
    // Return (and cache) the main options html page as the default.
    response_bytes = ui::ResourceBundle::GetSharedInstance().
        LoadDataResourceBytes(IDR_OPTIONS_HTML);
  }

  callback.Run(response_bytes.get());
}

std::string OptionsUIHTMLSource::GetMimeType(const std::string& path) const {
  if (path == kLocalizedStringsFile || path == kOptionsBundleJsFile)
    return "application/javascript";

  return "text/html";
}

bool OptionsUIHTMLSource::ShouldDenyXFrameOptions() const {
  return false;
}

OptionsUIHTMLSource::~OptionsUIHTMLSource() {}

////////////////////////////////////////////////////////////////////////////////
//
// OptionsPageUIHandler
//
////////////////////////////////////////////////////////////////////////////////

const char OptionsPageUIHandler::kSettingsAppKey[] = "settingsApp";

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
  localized_strings->Set(OptionsPageUIHandler::kSettingsAppKey,
                         new base::DictionaryValue());

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
#if defined(ENABLE_GOOGLE_NOW)
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
  AddOptionsPageUIHandler(localized_strings,
                          new SupervisedUserCreateConfirmHandler());
  AddOptionsPageUIHandler(localized_strings, new SupervisedUserImportHandler());
  AddOptionsPageUIHandler(localized_strings,
                          new SupervisedUserLearnMoreHandler());
  AddOptionsPageUIHandler(localized_strings, new SyncSetupHandler(
      g_browser_process->profile_manager()));
  AddOptionsPageUIHandler(localized_strings, new WebsiteSettingsHandler());
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
                          new chromeos::options::InternetOptionsHandler());
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::options::KeyboardHandler());

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

  policy::ConsumerManagementService* consumer_management =
      g_browser_process->platform_part()->browser_policy_connector_chromeos()->
          GetConsumerManagementService();
  chromeos::options::ConsumerManagementHandler* consumer_management_handler =
      new chromeos::options::ConsumerManagementHandler(consumer_management);
  AddOptionsPageUIHandler(localized_strings, consumer_management_handler);
#endif
#if defined(USE_NSS)
  AddOptionsPageUIHandler(localized_strings,
                          new CertificateManagerHandler(false));
#endif
  AddOptionsPageUIHandler(localized_strings, new HandlerOptionsHandler());

  web_ui->AddMessageHandler(new MetricsHandler());

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

scoped_ptr<OptionsUI::OnFinishedLoadingCallbackList::Subscription>
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
    base::DictionaryValue* entry = new base::DictionaryValue();
    entry->SetString("title", match.description);
    entry->SetString("displayURL", match.contents);
    entry->SetString("url", match.destination_url.spec());
    suggestions->Append(entry);
  }
}

// static
base::RefCountedMemory* OptionsUI::GetFaviconResourceBytes(
      ui::ScaleFactor scale_factor) {
  return ui::ResourceBundle::GetSharedInstance().
      LoadDataResourceBytesForScale(IDR_SETTINGS_FAVICON, scale_factor);
}

void OptionsUI::DidStartProvisionalLoadForFrame(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    bool is_error_page,
    bool is_iframe_srcdoc) {
  if (render_frame_host->GetRenderViewHost() ==
          web_ui()->GetWebContents()->GetRenderViewHost() &&
      validated_url.host() == chrome::kChromeUISettingsFrameHost) {
    for (size_t i = 0; i < handlers_.size(); ++i)
      handlers_[i]->PageLoadStarted();
  }
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

  web_ui()->CallJavascriptFunction(
      "BrowserOptions.notifyInitializationComplete");
}

void OptionsUI::OnFinishedLoading() {
  on_finished_loading_callbacks_.Notify();
}

void OptionsUI::AddOptionsPageUIHandler(
    base::DictionaryValue* localized_strings,
    OptionsPageUIHandler* handler_raw) {
  scoped_ptr<OptionsPageUIHandler> handler(handler_raw);
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
