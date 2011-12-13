// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options2/options_ui.h"

#include <algorithm>
#include <vector>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/memory/singleton.h"
#include "base/message_loop.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/browser_about_handler.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/options2/advanced_options_handler.h"
#include "chrome/browser/ui/webui/options2/autofill_options_handler.h"
#include "chrome/browser/ui/webui/options2/browser_options_handler.h"
#include "chrome/browser/ui/webui/options2/clear_browser_data_handler.h"
#include "chrome/browser/ui/webui/options2/content_settings_handler.h"
#include "chrome/browser/ui/webui/options2/cookies_view_handler.h"
#include "chrome/browser/ui/webui/options2/core_options_handler.h"
#include "chrome/browser/ui/webui/options2/extension_settings_handler.h"
#include "chrome/browser/ui/webui/options2/font_settings_handler.h"
#include "chrome/browser/ui/webui/options2/handler_options_handler.h"
#include "chrome/browser/ui/webui/options2/import_data_handler.h"
#include "chrome/browser/ui/webui/options2/language_options_handler.h"
#include "chrome/browser/ui/webui/options2/manage_profile_handler.h"
#include "chrome/browser/ui/webui/options2/options_sync_setup_handler.h"
#include "chrome/browser/ui/webui/options2/pack_extension_handler.h"
#include "chrome/browser/ui/webui/options2/password_manager_handler.h"
#include "chrome/browser/ui/webui/options2/personal_options_handler.h"
#include "chrome/browser/ui/webui/options2/search_engine_manager_handler.h"
#include "chrome/browser/ui/webui/options2/stop_syncing_handler.h"
#include "chrome/browser/ui/webui/options2/web_intents_settings_handler.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/time_format.h"
#include "chrome/common/url_constants.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_delegate.h"
#include "content/browser/user_metrics.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_types.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/options2_resources.h"
#include "grit/theme_resources.h"
#include "grit/theme_resources_standard.h"
#include "net/base/escape.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/webui/options2/chromeos/about_page_handler.h"
#include "chrome/browser/ui/webui/options2/chromeos/accounts_options_handler.h"
#include "chrome/browser/ui/webui/options2/chromeos/bluetooth_options_handler.h"
#include "chrome/browser/ui/webui/options2/chromeos/change_picture_options_handler.h"
#include "chrome/browser/ui/webui/options2/chromeos/core_chromeos_options_handler.h"
#include "chrome/browser/ui/webui/options2/chromeos/cros_language_options_handler.h"
#include "chrome/browser/ui/webui/options2/chromeos/internet_options_handler.h"
#include "chrome/browser/ui/webui/options2/chromeos/language_chewing_handler.h"
#include "chrome/browser/ui/webui/options2/chromeos/language_customize_modifier_keys_handler.h"
#include "chrome/browser/ui/webui/options2/chromeos/language_hangul_handler.h"
#include "chrome/browser/ui/webui/options2/chromeos/language_mozc_handler.h"
#include "chrome/browser/ui/webui/options2/chromeos/language_pinyin_handler.h"
#include "chrome/browser/ui/webui/options2/chromeos/proxy_handler.h"
#include "chrome/browser/ui/webui/options2/chromeos/stats_options_handler.h"
#include "chrome/browser/ui/webui/options2/chromeos/system_options_handler.h"
#include "chrome/browser/ui/webui/options2/chromeos/user_image_source.h"
#include "chrome/browser/ui/webui/options2/chromeos/virtual_keyboard_manager_handler.h"
#endif

#if defined(USE_NSS)
#include "chrome/browser/ui/webui/options2/certificate_manager_handler.h"
#endif

static const char kLocalizedStringsFile[] = "strings.js";
static const char kOptionsBundleJsFile[]  = "options_bundle.js";

////////////////////////////////////////////////////////////////////////////////
//
// Options2UIHTMLSource
//
////////////////////////////////////////////////////////////////////////////////

class Options2UIHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  // The constructor takes over ownership of |localized_strings|.
  explicit Options2UIHTMLSource(DictionaryValue* localized_strings);
  virtual ~Options2UIHTMLSource();

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                bool is_incognito,
                                int request_id);
  virtual std::string GetMimeType(const std::string&) const;

 private:
   // Localized strings collection.
  scoped_ptr<DictionaryValue> localized_strings_;

  DISALLOW_COPY_AND_ASSIGN(Options2UIHTMLSource);
};

Options2UIHTMLSource::Options2UIHTMLSource(DictionaryValue* localized_strings)
    : DataSource(chrome::kChromeUISettingsFrameHost, MessageLoop::current()) {
  DCHECK(localized_strings);
  localized_strings_.reset(localized_strings);
}

Options2UIHTMLSource::~Options2UIHTMLSource() {}

void Options2UIHTMLSource::StartDataRequest(const std::string& path,
                                            bool is_incognito,
                                            int request_id) {
  scoped_refptr<RefCountedMemory> response_bytes;
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

std::string Options2UIHTMLSource::GetMimeType(const std::string& path) const {
  if (path == kLocalizedStringsFile || path == kOptionsBundleJsFile)
    return "application/javascript";

  return "text/html";
}

////////////////////////////////////////////////////////////////////////////////
//
// OptionsPage2UIHandler
//
////////////////////////////////////////////////////////////////////////////////

OptionsPage2UIHandler::OptionsPage2UIHandler() {
}

OptionsPage2UIHandler::~OptionsPage2UIHandler() {
}

bool OptionsPage2UIHandler::IsEnabled() {
  return true;
}

// static
void OptionsPage2UIHandler::RegisterStrings(
    DictionaryValue* localized_strings,
    const OptionsStringResource* resources,
    size_t length) {
  for (size_t i = 0; i < length; ++i) {
    localized_strings->SetString(
        resources[i].name, l10n_util::GetStringUTF16(resources[i].id));
  }
}

void OptionsPage2UIHandler::RegisterTitle(DictionaryValue* localized_strings,
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
// Options2UI
//
////////////////////////////////////////////////////////////////////////////////

Options2UI::Options2UI(TabContents* contents)
    : ChromeWebUI(contents),
      initialized_handlers_(false) {
  DictionaryValue* localized_strings = new DictionaryValue();

  CoreOptionsHandler* core_handler;
#if defined(OS_CHROMEOS)
  core_handler = new chromeos::CoreChromeOSOptionsHandler();
#else
  core_handler = new CoreOptionsHandler();
#endif
  core_handler->set_handlers_host(this);
  AddOptionsPageUIHandler(localized_strings, core_handler);

  AddOptionsPageUIHandler(localized_strings, new AdvancedOptionsHandler());
  AddOptionsPageUIHandler(localized_strings, new AutofillOptionsHandler());
  AddOptionsPageUIHandler(localized_strings, new BrowserOptionsHandler());
  AddOptionsPageUIHandler(localized_strings, new ClearBrowserDataHandler());
  AddOptionsPageUIHandler(localized_strings, new ContentSettingsHandler());
  AddOptionsPageUIHandler(localized_strings, new CookiesViewHandler());
  AddOptionsPageUIHandler(localized_strings, new ExtensionSettingsHandler());
  AddOptionsPageUIHandler(localized_strings, new FontSettingsHandler());
  AddOptionsPageUIHandler(localized_strings, new WebIntentsSettingsHandler());
#if defined(OS_CHROMEOS)
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::CrosLanguageOptionsHandler());
#else
  AddOptionsPageUIHandler(localized_strings, new LanguageOptionsHandler());
#endif
  AddOptionsPageUIHandler(localized_strings, new ManageProfileHandler());
  AddOptionsPageUIHandler(localized_strings, new PackExtensionHandler());
  AddOptionsPageUIHandler(localized_strings, new PasswordManagerHandler());
  AddOptionsPageUIHandler(localized_strings, new PersonalOptionsHandler());
  AddOptionsPageUIHandler(localized_strings, new SearchEngineManagerHandler());
  AddOptionsPageUIHandler(localized_strings, new ImportDataHandler());
  AddOptionsPageUIHandler(localized_strings, new StopSyncingHandler());
  AddOptionsPageUIHandler(localized_strings, new OptionsSyncSetupHandler(
      g_browser_process->profile_manager()));
#if defined(OS_CHROMEOS)
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::AboutPageHandler());
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::AccountsOptionsHandler());
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::BluetoothOptionsHandler());
  AddOptionsPageUIHandler(localized_strings, new InternetOptionsHandler());
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::LanguageChewingHandler());
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::LanguageCustomizeModifierKeysHandler());
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::LanguageHangulHandler());
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::LanguageMozcHandler());
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::LanguagePinyinHandler());
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::VirtualKeyboardManagerHandler());
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::ProxyHandler());
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::ChangePictureOptionsHandler());
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::StatsOptionsHandler());
  AddOptionsPageUIHandler(localized_strings, new SystemOptionsHandler());
#endif
#if defined(USE_NSS)
  AddOptionsPageUIHandler(localized_strings, new CertificateManagerHandler());
#endif
  AddOptionsPageUIHandler(localized_strings, new HandlerOptionsHandler());

  // |localized_strings| ownership is taken over by this constructor.
  Options2UIHTMLSource* html_source =
      new Options2UIHTMLSource(localized_strings);

  // Set up the chrome://settings-frame/ source.
  Profile* profile = Profile::FromBrowserContext(contents->browser_context());
  profile->GetChromeURLDataManager()->AddDataSource(html_source);

  // Set up the chrome://theme/ source.
  ThemeSource* theme = new ThemeSource(profile);
  profile->GetChromeURLDataManager()->AddDataSource(theme);

#if defined(OS_CHROMEOS)
  // Set up the chrome://userimage/ source.
  chromeos::UserImageSource* user_image_source =
      new chromeos::UserImageSource();
  profile->GetChromeURLDataManager()->AddDataSource(user_image_source);
#endif
}

Options2UI::~Options2UI() {
  // Uninitialize all registered handlers. The base class owns them and it will
  // eventually delete them. Skip over the generic handler.
  for (std::vector<WebUIMessageHandler*>::iterator iter = handlers_.begin() + 1;
       iter != handlers_.end();
       ++iter) {
    static_cast<OptionsPage2UIHandler*>(*iter)->Uninitialize();
  }
}

// Override.
void Options2UI::RenderViewCreated(RenderViewHost* render_view_host) {
  SetCommandLineString(render_view_host);
  ChromeWebUI::RenderViewCreated(render_view_host);
}

void Options2UI::RenderViewReused(RenderViewHost* render_view_host) {
  SetCommandLineString(render_view_host);
  ChromeWebUI::RenderViewReused(render_view_host);
}

void Options2UI::DidBecomeActiveForReusedRenderView() {
  // When the renderer is re-used (e.g., for back/forward navigation within
  // options), the handlers are torn down and rebuilt, so are no longer
  // initialized, but the web page's DOM may remain intact, in which case onload
  // won't fire to initilize the handlers. To make sure initialization always
  // happens, call reinitializeCore (which is a no-op unless the DOM was already
  // initialized).
  CallJavascriptFunction("OptionsPage.reinitializeCore");

  ChromeWebUI::DidBecomeActiveForReusedRenderView();
}

// static
RefCountedMemory* Options2UI::GetFaviconResourceBytes() {
  return ResourceBundle::GetSharedInstance().
      LoadDataResourceBytes(IDR_SETTINGS_FAVICON);
}

void Options2UI::InitializeHandlers() {
  DCHECK(!GetProfile()->IsOffTheRecord() || Profile::IsGuestSession());

  // The reinitialize call from DidBecomeActiveForReusedRenderView end up being
  // delivered after a new web page DOM has been brought up in an existing
  // renderer (due to IPC delays), causing this method to be called twice. If
  // that happens, ignore the second call.
  if (initialized_handlers_)
    return;
  initialized_handlers_ = true;

  std::vector<WebUIMessageHandler*>::iterator iter;
  // Skip over the generic handler.
  for (iter = handlers_.begin() + 1; iter != handlers_.end(); ++iter) {
    (static_cast<OptionsPage2UIHandler*>(*iter))->Initialize();
  }
}

void Options2UI::AddOptionsPageUIHandler(DictionaryValue* localized_strings,
                                         OptionsPage2UIHandler* handler_raw) {
  scoped_ptr<OptionsPage2UIHandler> handler(handler_raw);
  DCHECK(handler.get());
  // Add only if handler's service is enabled.
  if (handler->IsEnabled()) {
    handler->GetLocalizedValues(localized_strings);
    // Add handler to the list and also pass the ownership.
    AddMessageHandler(handler.release()->Attach(this));
  }
}

void Options2UI::SetCommandLineString(RenderViewHost* render_view_host) {
  std::string command_line_string;

#if defined(OS_WIN)
  command_line_string =
      WideToASCII(CommandLine::ForCurrentProcess()->GetCommandLineString());
#else
  command_line_string =
      CommandLine::ForCurrentProcess()->GetCommandLineString();
#endif

  render_view_host->SetWebUIProperty("commandLineString", command_line_string);
}
