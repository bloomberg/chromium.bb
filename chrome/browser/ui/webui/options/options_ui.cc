// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/options_ui.h"

#include <algorithm>
#include <vector>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/singleton.h"
#include "base/message_loop.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/autocomplete_result.h"
#include "chrome/browser/browser_about_handler.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/options/autofill_options_handler.h"
#include "chrome/browser/ui/webui/options/browser_options_handler.h"
#include "chrome/browser/ui/webui/options/clear_browser_data_handler.h"
#include "chrome/browser/ui/webui/options/content_settings_handler.h"
#include "chrome/browser/ui/webui/options/cookies_view_handler.h"
#include "chrome/browser/ui/webui/options/core_options_handler.h"
#include "chrome/browser/ui/webui/options/font_settings_handler.h"
#include "chrome/browser/ui/webui/options/handler_options_handler.h"
#include "chrome/browser/ui/webui/options/home_page_overlay_handler.h"
#include "chrome/browser/ui/webui/options/import_data_handler.h"
#include "chrome/browser/ui/webui/options/language_dictionary_overlay_handler.h"
#include "chrome/browser/ui/webui/options/language_options_handler.h"
#include "chrome/browser/ui/webui/options/manage_profile_handler.h"
#include "chrome/browser/ui/webui/options/managed_user_settings_handler.h"
#include "chrome/browser/ui/webui/options/media_devices_selection_handler.h"
#include "chrome/browser/ui/webui/options/media_galleries_handler.h"
#include "chrome/browser/ui/webui/options/options_sync_setup_handler.h"
#include "chrome/browser/ui/webui/options/password_manager_handler.h"
#include "chrome/browser/ui/webui/options/search_engine_manager_handler.h"
#include "chrome/browser/ui/webui/options/startup_pages_handler.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/browser/ui/webui/web_ui_util.h"
#include "chrome/common/time_format.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_ui.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/options_resources.h"
#include "grit/theme_resources.h"
#include "net/base/escape.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/webui/jstemplate_builder.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/system/pointer_device_observer.h"
#include "chrome/browser/ui/webui/options/chromeos/accounts_options_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/bluetooth_options_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/change_picture_options_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/core_chromeos_options_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/cros_language_options_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/display_options_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/internet_options_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/keyboard_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/language_chewing_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/language_hangul_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/language_mozc_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/language_pinyin_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/pointer_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/proxy_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/stats_options_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/user_image_source.h"
#endif

#if defined(USE_NSS)
#include "chrome/browser/ui/webui/options/certificate_manager_handler.h"
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
  explicit OptionsUIHTMLSource(DictionaryValue* localized_strings);

  // content::URLDataSource implementation.
  virtual std::string GetSource() OVERRIDE;
  virtual void StartDataRequest(
      const std::string& path,
      bool is_incognito,
      const content::URLDataSource::GotDataCallback& callback);
  virtual std::string GetMimeType(const std::string&) const;

 private:
  virtual ~OptionsUIHTMLSource();

  // Localized strings collection.
  scoped_ptr<DictionaryValue> localized_strings_;

  DISALLOW_COPY_AND_ASSIGN(OptionsUIHTMLSource);
};

OptionsUIHTMLSource::OptionsUIHTMLSource(DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  localized_strings_.reset(localized_strings);
}

std::string OptionsUIHTMLSource::GetSource() {
  return chrome::kChromeUISettingsFrameHost;
}

void OptionsUIHTMLSource::StartDataRequest(
    const std::string& path,
    bool is_incognito,
    const content::URLDataSource::GotDataCallback& callback) {
  scoped_refptr<base::RefCountedMemory> response_bytes;
  web_ui_util::SetFontAndTextDirection(localized_strings_.get());

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

  callback.Run(response_bytes);
}

std::string OptionsUIHTMLSource::GetMimeType(const std::string& path) const {
  if (path == kLocalizedStringsFile || path == kOptionsBundleJsFile)
    return "application/javascript";

  return "text/html";
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
    DictionaryValue* localized_strings,
    const OptionsStringResource* resources,
    size_t length) {
  for (size_t i = 0; i < length; ++i) {
    string16 value;
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

void OptionsPageUIHandler::RegisterTitle(DictionaryValue* localized_strings,
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
      initialized_handlers_(false) {
  DictionaryValue* localized_strings = new DictionaryValue();
  localized_strings->Set(OptionsPageUIHandler::kSettingsAppKey,
                         new DictionaryValue());

  CoreOptionsHandler* core_handler;
#if defined(OS_CHROMEOS)
  core_handler = new chromeos::options::CoreChromeOSOptionsHandler();
#else
  core_handler = new CoreOptionsHandler();
#endif
  core_handler->set_handlers_host(this);
  AddOptionsPageUIHandler(localized_strings, core_handler);

  AddOptionsPageUIHandler(localized_strings, new AutofillOptionsHandler());

  BrowserOptionsHandler* browser_options_handler = new BrowserOptionsHandler();
  AddOptionsPageUIHandler(localized_strings, browser_options_handler);

  AddOptionsPageUIHandler(localized_strings, new ClearBrowserDataHandler());
  AddOptionsPageUIHandler(localized_strings, new ContentSettingsHandler());
  AddOptionsPageUIHandler(localized_strings, new CookiesViewHandler());
  AddOptionsPageUIHandler(localized_strings, new FontSettingsHandler());
  AddOptionsPageUIHandler(localized_strings, new HomePageOverlayHandler());
  AddOptionsPageUIHandler(localized_strings,
                          new MediaDevicesSelectionHandler());
  AddOptionsPageUIHandler(localized_strings, new MediaGalleriesHandler());
#if defined(OS_CHROMEOS)
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::options::CrosLanguageOptionsHandler());
#else
  AddOptionsPageUIHandler(localized_strings, new LanguageOptionsHandler());
#endif
  AddOptionsPageUIHandler(localized_strings,
                          new LanguageDictionaryOverlayHandler());
  AddOptionsPageUIHandler(localized_strings, new ManageProfileHandler());
  AddOptionsPageUIHandler(localized_strings, new ManagedUserSettingsHandler());
  AddOptionsPageUIHandler(localized_strings, new PasswordManagerHandler());
  AddOptionsPageUIHandler(localized_strings, new SearchEngineManagerHandler());
  AddOptionsPageUIHandler(localized_strings, new ImportDataHandler());
  AddOptionsPageUIHandler(localized_strings, new StartupPagesHandler());
  AddOptionsPageUIHandler(localized_strings, new OptionsSyncSetupHandler(
      g_browser_process->profile_manager()));
#if defined(OS_CHROMEOS)
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::options::AccountsOptionsHandler());
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::options::BluetoothOptionsHandler());
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::options::DisplayOptionsHandler());
  AddOptionsPageUIHandler(localized_strings, new InternetOptionsHandler());
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::options::LanguageChewingHandler());
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::options::KeyboardHandler());
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::options::LanguageHangulHandler());
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::options::LanguageMozcHandler());
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::options::LanguagePinyinHandler());

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
#endif
#if defined(USE_NSS)
  AddOptionsPageUIHandler(localized_strings, new CertificateManagerHandler());
#endif
  AddOptionsPageUIHandler(localized_strings, new HandlerOptionsHandler());

  // |localized_strings| ownership is taken over by this constructor.
  OptionsUIHTMLSource* html_source =
      new OptionsUIHTMLSource(localized_strings);

  // Set up the chrome://settings-frame/ source.
  Profile* profile = Profile::FromWebUI(web_ui);
  ChromeURLDataManager::AddDataSource(profile, html_source);

  // Set up the chrome://theme/ source.
  ThemeSource* theme = new ThemeSource(profile);
  ChromeURLDataManager::AddDataSource(profile, theme);

#if defined(OS_CHROMEOS)
  // Set up the chrome://userimage/ source.
  chromeos::options::UserImageSource* user_image_source =
      new chromeos::options::UserImageSource();
  ChromeURLDataManager::AddDataSource(profile, user_image_source);

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

// static
void OptionsUI::ProcessAutocompleteSuggestions(
    const AutocompleteResult& result,
    base::ListValue* const suggestions) {
  for (size_t i = 0; i < result.size(); ++i) {
    const AutocompleteMatch& match = result.match_at(i);
    AutocompleteMatch::Type type = match.type;
    if (type != AutocompleteMatch::HISTORY_URL &&
        type != AutocompleteMatch::HISTORY_TITLE &&
        type != AutocompleteMatch::HISTORY_BODY &&
        type != AutocompleteMatch::HISTORY_KEYWORD &&
        type != AutocompleteMatch::NAVSUGGEST)
      continue;
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

void OptionsUI::RenderViewCreated(content::RenderViewHost* render_view_host) {
  content::WebUIController::RenderViewCreated(render_view_host);
  for (size_t i = 0; i < handlers_.size(); ++i)
    handlers_[i]->PageLoadStarted();
}

void OptionsUI::RenderViewReused(content::RenderViewHost* render_view_host) {
  content::WebUIController::RenderViewReused(render_view_host);
  for (size_t i = 0; i < handlers_.size(); ++i)
    handlers_[i]->PageLoadStarted();
}

void OptionsUI::AddOptionsPageUIHandler(DictionaryValue* localized_strings,
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
