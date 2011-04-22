// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/options_ui.h"

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
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/options/about_page_handler.h"
#include "chrome/browser/ui/webui/options/advanced_options_handler.h"
#include "chrome/browser/ui/webui/options/autofill_options_handler.h"
#include "chrome/browser/ui/webui/options/browser_options_handler.h"
#include "chrome/browser/ui/webui/options/clear_browser_data_handler.h"
#include "chrome/browser/ui/webui/options/content_settings_handler.h"
#include "chrome/browser/ui/webui/options/cookies_view_handler.h"
#include "chrome/browser/ui/webui/options/core_options_handler.h"
#include "chrome/browser/ui/webui/options/font_settings_handler.h"
#include "chrome/browser/ui/webui/options/import_data_handler.h"
#include "chrome/browser/ui/webui/options/language_options_handler.h"
#include "chrome/browser/ui/webui/options/password_manager_handler.h"
#include "chrome/browser/ui/webui/options/personal_options_handler.h"
#include "chrome/browser/ui/webui/options/search_engine_manager_handler.h"
#include "chrome/browser/ui/webui/options/stop_syncing_handler.h"
#include "chrome/browser/ui/webui/options/sync_setup_handler.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/time_format.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browser_thread.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_delegate.h"
#include "content/common/notification_type.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "net/base/escape.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/webui/options/chromeos/accounts_options_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/change_picture_options_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/core_chromeos_options_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/cros_language_options_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/internet_options_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/language_chewing_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/language_customize_modifier_keys_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/language_hangul_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/language_mozc_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/language_pinyin_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/proxy_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/stats_options_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/system_options_handler.h"
#include "chrome/browser/ui/webui/options/chromeos/user_image_source.h"
#endif

#if defined(USE_NSS)
#include "chrome/browser/ui/webui/options/certificate_manager_handler.h"
#endif

////////////////////////////////////////////////////////////////////////////////
//
// OptionsUIHTMLSource
//
////////////////////////////////////////////////////////////////////////////////

class OptionsUIHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  // The constructor takes over ownership of |localized_strings|.
  explicit OptionsUIHTMLSource(DictionaryValue* localized_strings);
  virtual ~OptionsUIHTMLSource();

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                bool is_incognito,
                                int request_id);
  virtual std::string GetMimeType(const std::string&) const;

 private:
   // Localized strings collection.
  scoped_ptr<DictionaryValue> localized_strings_;

  DISALLOW_COPY_AND_ASSIGN(OptionsUIHTMLSource);
};

OptionsUIHTMLSource::OptionsUIHTMLSource(DictionaryValue* localized_strings)
    : DataSource(chrome::kChromeUISettingsHost, MessageLoop::current()) {
  DCHECK(localized_strings);
  localized_strings_.reset(localized_strings);
}

OptionsUIHTMLSource::~OptionsUIHTMLSource() {}

void OptionsUIHTMLSource::StartDataRequest(const std::string& path,
                                           bool is_incognito,
                                           int request_id) {
  SetFontAndTextDirection(localized_strings_.get());

  static const base::StringPiece options_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_OPTIONS_HTML));
  const std::string full_html = jstemplate_builder::GetI18nTemplateHtml(
      options_html, localized_strings_.get());

  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);
  html_bytes->data.resize(full_html.size());
  std::copy(full_html.begin(), full_html.end(), html_bytes->data.begin());

  SendResponse(request_id, html_bytes);
}

std::string OptionsUIHTMLSource::GetMimeType(const std::string&) const {
  return "text/html";
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

void OptionsPageUIHandler::UserMetricsRecordAction(
    const UserMetricsAction& action) {
  UserMetrics::RecordAction(action, web_ui_->GetProfile());
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

OptionsUI::OptionsUI(TabContents* contents)
    : WebUI(contents), initialized_handlers_(false) {
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
  AddOptionsPageUIHandler(localized_strings, new FontSettingsHandler());
#if defined(OS_CHROMEOS)
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::CrosLanguageOptionsHandler());
#else
  AddOptionsPageUIHandler(localized_strings, new LanguageOptionsHandler());
#endif
  AddOptionsPageUIHandler(localized_strings, new PasswordManagerHandler());
  AddOptionsPageUIHandler(localized_strings, new PersonalOptionsHandler());
  AddOptionsPageUIHandler(localized_strings, new SearchEngineManagerHandler());
  AddOptionsPageUIHandler(localized_strings, new ImportDataHandler());
  AddOptionsPageUIHandler(localized_strings, new StopSyncingHandler());
  AddOptionsPageUIHandler(localized_strings, new SyncSetupHandler());
#if defined(OS_CHROMEOS)
  AddOptionsPageUIHandler(localized_strings, new AboutPageHandler());
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::AccountsOptionsHandler());
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
  AddOptionsPageUIHandler(localized_strings, new chromeos::ProxyHandler());
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::ChangePictureOptionsHandler());
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::StatsOptionsHandler());
  AddOptionsPageUIHandler(localized_strings, new SystemOptionsHandler());
#endif
#if defined(USE_NSS)
  AddOptionsPageUIHandler(localized_strings, new CertificateManagerHandler());
#endif

  // |localized_strings| ownership is taken over by this constructor.
  OptionsUIHTMLSource* html_source =
      new OptionsUIHTMLSource(localized_strings);

  // Set up the chrome://settings/ source.
  contents->profile()->GetChromeURLDataManager()->AddDataSource(html_source);

  // Set up the chrome://theme/ source.
  ThemeSource* theme = new ThemeSource(contents->profile());
  contents->profile()->GetChromeURLDataManager()->AddDataSource(theme);

#if defined(OS_CHROMEOS)
  // Set up the chrome://userimage/ source.
  chromeos::UserImageSource* user_image_source =
      new chromeos::UserImageSource();
  contents->profile()->GetChromeURLDataManager()->AddDataSource(
      user_image_source);
#endif

  // Initialize the chrome://about/ source in case the user clicks the credits
  // link.
  InitializeAboutDataSource(contents->profile());
}

OptionsUI::~OptionsUI() {
  // Uninitialize all registered handlers. The base class owns them and it will
  // eventually delete them. Skip over the generic handler.
  for (std::vector<WebUIMessageHandler*>::iterator iter = handlers_.begin() + 1;
       iter != handlers_.end();
       ++iter) {
    static_cast<OptionsPageUIHandler*>(*iter)->Uninitialize();
  }
}

// Override.
void OptionsUI::RenderViewCreated(RenderViewHost* render_view_host) {
  std::string command_line_string;

#if defined(OS_WIN)
  std::wstring wstr = CommandLine::ForCurrentProcess()->command_line_string();
  command_line_string = WideToASCII(wstr);
#else
  command_line_string = CommandLine::ForCurrentProcess()->command_line_string();
#endif

  render_view_host->SetWebUIProperty("commandLineString", command_line_string);
  WebUI::RenderViewCreated(render_view_host);
}

void OptionsUI::DidBecomeActiveForReusedRenderView() {
  // When the renderer is re-used (e.g., for back/forward navigation within
  // options), the handlers are torn down and rebuilt, so are no longer
  // initialized, but the web page's DOM may remain intact, in which case onload
  // won't fire to initilize the handlers. To make sure initialization always
  // happens, call reinitializeCore (which is a no-op unless the DOM was already
  // initialized).
  CallJavascriptFunction("OptionsPage.reinitializeCore");

  WebUI::DidBecomeActiveForReusedRenderView();
}

// static
RefCountedMemory* OptionsUI::GetFaviconResourceBytes() {
  return ResourceBundle::GetSharedInstance().
      LoadDataResourceBytes(IDR_SETTINGS_FAVICON);
}

void OptionsUI::InitializeHandlers() {
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
    (static_cast<OptionsPageUIHandler*>(*iter))->Initialize();
  }
}

void OptionsUI::AddOptionsPageUIHandler(DictionaryValue* localized_strings,
                                        OptionsPageUIHandler* handler_raw) {
  scoped_ptr<OptionsPageUIHandler> handler(handler_raw);
  DCHECK(handler.get());
  // Add only if handler's service is enabled.
  if (handler->IsEnabled()) {
    handler->GetLocalizedValues(localized_strings);
    // Add handler to the list and also pass the ownership.
    AddMessageHandler(handler.release()->Attach(this));
  }
}
