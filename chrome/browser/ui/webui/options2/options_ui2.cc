// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options2/options_ui2.h"

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
#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/browser_about_handler.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/options2/autofill_options_handler2.h"
#include "chrome/browser/ui/webui/options2/browser_options_handler2.h"
#include "chrome/browser/ui/webui/options2/clear_browser_data_handler2.h"
#include "chrome/browser/ui/webui/options2/content_settings_handler2.h"
#include "chrome/browser/ui/webui/options2/cookies_view_handler2.h"
#include "chrome/browser/ui/webui/options2/core_options_handler2.h"
#include "chrome/browser/ui/webui/options2/font_settings_handler2.h"
#include "chrome/browser/ui/webui/options2/handler_options_handler2.h"
#include "chrome/browser/ui/webui/options2/home_page_overlay_handler2.h"
#include "chrome/browser/ui/webui/options2/import_data_handler2.h"
#include "chrome/browser/ui/webui/options2/language_options_handler2.h"
#include "chrome/browser/ui/webui/options2/manage_profile_handler2.h"
#include "chrome/browser/ui/webui/options2/options_sync_setup_handler.h"
#include "chrome/browser/ui/webui/options2/password_manager_handler2.h"
#include "chrome/browser/ui/webui/options2/search_engine_manager_handler2.h"
#include "chrome/browser/ui/webui/options2/startup_pages_handler2.h"
#include "chrome/browser/ui/webui/options2/web_intents_settings_handler2.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/time_format.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_ui.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/options2_resources.h"
#include "grit/theme_resources.h"
#include "grit/theme_resources_standard.h"
#include "net/base/escape.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/system/pointer_device_observer.h"
#include "chrome/browser/ui/webui/options2/chromeos/accounts_options_handler2.h"
#include "chrome/browser/ui/webui/options2/chromeos/bluetooth_options_handler2.h"
#include "chrome/browser/ui/webui/options2/chromeos/change_picture_options_handler2.h"
#include "chrome/browser/ui/webui/options2/chromeos/core_chromeos_options_handler2.h"
#include "chrome/browser/ui/webui/options2/chromeos/cros_language_options_handler2.h"
#include "chrome/browser/ui/webui/options2/chromeos/internet_options_handler2.h"
#include "chrome/browser/ui/webui/options2/chromeos/keyboard_handler2.h"
#include "chrome/browser/ui/webui/options2/chromeos/language_chewing_handler2.h"
#include "chrome/browser/ui/webui/options2/chromeos/language_hangul_handler2.h"
#include "chrome/browser/ui/webui/options2/chromeos/language_mozc_handler2.h"
#include "chrome/browser/ui/webui/options2/chromeos/language_pinyin_handler2.h"
#include "chrome/browser/ui/webui/options2/chromeos/pointer_handler2.h"
#include "chrome/browser/ui/webui/options2/chromeos/proxy_handler2.h"
#include "chrome/browser/ui/webui/options2/chromeos/stats_options_handler2.h"
#include "chrome/browser/ui/webui/options2/chromeos/user_image_source2.h"
#include "chrome/browser/ui/webui/options2/chromeos/wallpaper_thumbnail_source2.h"
#if defined(USE_VIRTUAL_KEYBOARD)
#include "chrome/browser/ui/webui/options2/chromeos/virtual_keyboard_manager_handler2.h"
#endif
#endif

#if defined(OS_CHROMEOS) && defined(USE_ASH)
#include "chrome/browser/ui/webui/options2/chromeos/set_wallpaper_options_handler2.h"
#endif

#if defined(USE_NSS)
#include "chrome/browser/ui/webui/options2/certificate_manager_handler2.h"
#endif

using content::RenderViewHost;

namespace {

const char kLocalizedStringsFile[] = "strings.js";
const char kOptionsBundleJsFile[]  = "options_bundle.js";

}  // namespace

namespace options2 {

////////////////////////////////////////////////////////////////////////////////
//
// OptionsUIHTMLSource
//
////////////////////////////////////////////////////////////////////////////////

class OptionsUIHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  // The constructor takes over ownership of |localized_strings|.
  explicit OptionsUIHTMLSource(DictionaryValue* localized_strings);

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                bool is_incognito,
                                int request_id);
  virtual std::string GetMimeType(const std::string&) const;

 private:
  virtual ~OptionsUIHTMLSource();

  // Localized strings collection.
  scoped_ptr<DictionaryValue> localized_strings_;

  DISALLOW_COPY_AND_ASSIGN(OptionsUIHTMLSource);
};

OptionsUIHTMLSource::OptionsUIHTMLSource(DictionaryValue* localized_strings)
    : DataSource(chrome::kChromeUISettingsFrameHost, MessageLoop::current()) {
  DCHECK(localized_strings);
  localized_strings_.reset(localized_strings);
}

void OptionsUIHTMLSource::StartDataRequest(const std::string& path,
                                            bool is_incognito,
                                            int request_id) {
  scoped_refptr<base::RefCountedMemory> response_bytes;
  SetFontAndTextDirection(localized_strings_.get());

  if (path == kLocalizedStringsFile) {
    // Return dynamically-generated strings from memory.
    std::string strings_js;
    jstemplate_builder::AppendJsonJS(localized_strings_.get(), &strings_js);
    response_bytes = base::RefCountedString::TakeString(&strings_js);
  } else if (path == kOptionsBundleJsFile) {
    // Return (and cache) the options javascript code.
    response_bytes = ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
        IDR_OPTIONS2_BUNDLE_JS);
  } else {
    // Return (and cache) the main options html page as the default.
    response_bytes = ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
        IDR_OPTIONS2_HTML);
  }

  SendResponse(request_id, response_bytes);
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
    localized_strings->SetString(
        resources[i].name, l10n_util::GetStringUTF16(resources[i].id));
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

  CoreOptionsHandler* core_handler;
#if defined(OS_CHROMEOS)
  core_handler = new chromeos::options2::CoreChromeOSOptionsHandler();
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
  AddOptionsPageUIHandler(localized_strings, new WebIntentsSettingsHandler());
#if defined(OS_CHROMEOS)
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::options2::CrosLanguageOptionsHandler());
#else
  AddOptionsPageUIHandler(localized_strings, new LanguageOptionsHandler());
#endif
  AddOptionsPageUIHandler(localized_strings, new ManageProfileHandler());
  AddOptionsPageUIHandler(localized_strings, new PasswordManagerHandler());
  AddOptionsPageUIHandler(localized_strings, new SearchEngineManagerHandler());
  AddOptionsPageUIHandler(localized_strings, new ImportDataHandler());
  AddOptionsPageUIHandler(localized_strings, new StartupPagesHandler());
  AddOptionsPageUIHandler(localized_strings, new OptionsSyncSetupHandler(
      g_browser_process->profile_manager()));
#if defined(OS_CHROMEOS)
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::options2::AccountsOptionsHandler());
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::options2::BluetoothOptionsHandler());
  AddOptionsPageUIHandler(localized_strings, new InternetOptionsHandler());
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::options2::LanguageChewingHandler());
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::options2::KeyboardHandler());
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::options2::LanguageHangulHandler());
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::options2::LanguageMozcHandler());
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::options2::LanguagePinyinHandler());

  chromeos::options2::PointerHandler* pointer_handler =
      new chromeos::options2::PointerHandler();
  AddOptionsPageUIHandler(localized_strings, pointer_handler);

#if defined(USE_VIRTUAL_KEYBOARD)
  AddOptionsPageUIHandler(
      localized_strings,
      new chromeos::options2::VirtualKeyboardManagerHandler());
#endif

  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::options2::ProxyHandler());
  AddOptionsPageUIHandler(
      localized_strings,
      new chromeos::options2::ChangePictureOptionsHandler());
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::options2::StatsOptionsHandler());
#endif
#if defined(OS_CHROMEOS) && defined(USE_ASH)
  AddOptionsPageUIHandler(
      localized_strings,
      new chromeos::options2::SetWallpaperOptionsHandler());
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
  chromeos::options2::UserImageSource* user_image_source =
      new chromeos::options2::UserImageSource();
  ChromeURLDataManager::AddDataSource(profile, user_image_source);

  // Set up the chrome://wallpaper/ source.
  chromeos::options2::WallpaperThumbnailSource* wallpaper_thumbnail_source =
      new chromeos::options2::WallpaperThumbnailSource();
  ChromeURLDataManager::AddDataSource(profile, wallpaper_thumbnail_source);

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
      const AutocompleteResult& autocompleteResult,
      ListValue * const suggestions) {
  for (size_t i = 0; i < autocompleteResult.size(); ++i) {
    const AutocompleteMatch& match = autocompleteResult.match_at(i);
    AutocompleteMatch::Type type = match.type;
    if (type != AutocompleteMatch::HISTORY_URL &&
        type != AutocompleteMatch::HISTORY_TITLE &&
        type != AutocompleteMatch::HISTORY_BODY &&
        type != AutocompleteMatch::HISTORY_KEYWORD &&
        type != AutocompleteMatch::NAVSUGGEST)
      continue;
    DictionaryValue* entry = new DictionaryValue();
    entry->SetString("title", match.description);
    entry->SetString("displayURL", match.contents);
    entry->SetString("url", match.destination_url.spec());
    suggestions->Append(entry);
  }
}

// static
base::RefCountedMemory* OptionsUI::GetFaviconResourceBytes() {
  return ResourceBundle::GetSharedInstance().
      LoadDataResourceBytes(IDR_SETTINGS_FAVICON);
}

void OptionsUI::InitializeHandlers() {
  Profile* profile = Profile::FromWebUI(web_ui());
  DCHECK(!profile->IsOffTheRecord() || Profile::IsGuestSession());

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

}  // namespace options2
