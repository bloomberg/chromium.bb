// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/options_ui.h"

#include <algorithm>

#include "app/resource_bundle.h"
#include "base/callback.h"
#include "base/message_loop.h"
#include "base/singleton.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/dom_ui/about_page_handler.h"
#include "chrome/browser/dom_ui/add_startup_page_handler.h"
#include "chrome/browser/dom_ui/advanced_options_handler.h"
#include "chrome/browser/dom_ui/autofill_options_handler.h"
#include "chrome/browser/dom_ui/browser_options_handler.h"
#include "chrome/browser/dom_ui/clear_browser_data_handler.h"
#include "chrome/browser/dom_ui/content_settings_handler.h"
#include "chrome/browser/dom_ui/cookies_view_handler.h"
#include "chrome/browser/dom_ui/core_options_handler.h"
#include "chrome/browser/dom_ui/dom_ui_theme_source.h"
#include "chrome/browser/dom_ui/font_settings_handler.h"
#include "chrome/browser/dom_ui/import_data_handler.h"
#include "chrome/browser/dom_ui/passwords_exceptions_handler.h"
#include "chrome/browser/dom_ui/personal_options_handler.h"
#include "chrome/browser/dom_ui/search_engine_manager_handler.h"
#include "chrome/browser/dom_ui/sync_options_handler.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/time_format.h"
#include "chrome/common/url_constants.h"
#include "net/base/escape.h"

#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/dom_ui/accounts_options_handler.h"
#include "chrome/browser/chromeos/dom_ui/core_chromeos_options_handler.h"
#include "chrome/browser/chromeos/dom_ui/internet_options_handler.h"
#include "chrome/browser/chromeos/dom_ui/labs_handler.h"
#include "chrome/browser/chromeos/dom_ui/language_chewing_options_handler.h"
#include "chrome/browser/chromeos/dom_ui/language_customize_modifier_keys_handler.h"
#include "chrome/browser/chromeos/dom_ui/language_hangul_options_handler.h"
#include "chrome/browser/chromeos/dom_ui/language_mozc_options_handler.h"
#include "chrome/browser/chromeos/dom_ui/language_options_handler.h"
#include "chrome/browser/chromeos/dom_ui/language_pinyin_options_handler.h"
#include "chrome/browser/chromeos/dom_ui/proxy_handler.h"
#include "chrome/browser/chromeos/dom_ui/system_options_handler.h"
#endif

#if defined(USE_NSS)
#include "chrome/browser/dom_ui/certificate_manager_handler.h"
#endif

////////////////////////////////////////////////////////////////////////////////
//
// OptionsUIHTMLSource
//
////////////////////////////////////////////////////////////////////////////////

OptionsUIHTMLSource::OptionsUIHTMLSource(DictionaryValue* localized_strings)
    : DataSource(chrome::kChromeUISettingsHost, MessageLoop::current()) {
  DCHECK(localized_strings);
  localized_strings_.reset(localized_strings);
}

void OptionsUIHTMLSource::StartDataRequest(const std::string& path,
                                           bool is_off_the_record,
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

////////////////////////////////////////////////////////////////////////////////
//
// OptionsPageUIHandler
//
////////////////////////////////////////////////////////////////////////////////

OptionsPageUIHandler::OptionsPageUIHandler() {
}

OptionsPageUIHandler::~OptionsPageUIHandler() {
}

void OptionsPageUIHandler::UserMetricsRecordAction(
    const UserMetricsAction& action) {
  UserMetrics::RecordAction(action, dom_ui_->GetProfile());
}

////////////////////////////////////////////////////////////////////////////////
//
// OptionsUI
//
////////////////////////////////////////////////////////////////////////////////

OptionsUI::OptionsUI(TabContents* contents) : DOMUI(contents) {
  DictionaryValue* localized_strings = new DictionaryValue();

#if defined(OS_CHROMEOS)
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::CoreChromeOSOptionsHandler());
#else
  AddOptionsPageUIHandler(localized_strings, new CoreOptionsHandler());
#endif

  AddOptionsPageUIHandler(localized_strings, new AddStartupPageHandler());
  AddOptionsPageUIHandler(localized_strings, new AdvancedOptionsHandler());
  AddOptionsPageUIHandler(localized_strings, new AutoFillOptionsHandler());
  AddOptionsPageUIHandler(localized_strings, new BrowserOptionsHandler());
  AddOptionsPageUIHandler(localized_strings, new ClearBrowserDataHandler());
  AddOptionsPageUIHandler(localized_strings, new ContentSettingsHandler());
  AddOptionsPageUIHandler(localized_strings, new CookiesViewHandler());
  AddOptionsPageUIHandler(localized_strings, new FontSettingsHandler());
  AddOptionsPageUIHandler(localized_strings, new PasswordsExceptionsHandler());
  AddOptionsPageUIHandler(localized_strings, new PersonalOptionsHandler());
  AddOptionsPageUIHandler(localized_strings, new SearchEngineManagerHandler());
  AddOptionsPageUIHandler(localized_strings, new ImportDataHandler());
  AddOptionsPageUIHandler(localized_strings, new SyncOptionsHandler());
#if defined(OS_CHROMEOS)
  AddOptionsPageUIHandler(localized_strings, new AboutPageHandler());
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::AccountsOptionsHandler());
  AddOptionsPageUIHandler(localized_strings, new InternetOptionsHandler());
  AddOptionsPageUIHandler(localized_strings, new LabsHandler());
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::LanguageChewingOptionsHandler());
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::LanguageCustomizeModifierKeysHandler());
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::LanguageHangulOptionsHandler());
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::LanguageMozcOptionsHandler());
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::LanguageOptionsHandler());
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::LanguagePinyinOptionsHandler());
  AddOptionsPageUIHandler(localized_strings, new chromeos::ProxyHandler());
  AddOptionsPageUIHandler(localized_strings, new SystemOptionsHandler());
#endif
#if defined(USE_NSS)
  AddOptionsPageUIHandler(localized_strings, new CertificateManagerHandler());
#endif

  // |localized_strings| ownership is taken over by this constructor.
  OptionsUIHTMLSource* html_source =
      new OptionsUIHTMLSource(localized_strings);

  // Set up the chrome://settings/ source.
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(
          Singleton<ChromeURLDataManager>::get(),
          &ChromeURLDataManager::AddDataSource,
          make_scoped_refptr(html_source)));

  // Set up chrome://theme/ source.
  DOMUIThemeSource* theme = new DOMUIThemeSource(GetProfile());
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(
          Singleton<ChromeURLDataManager>::get(),
          &ChromeURLDataManager::AddDataSource,
          make_scoped_refptr(theme)));
}

OptionsUI::~OptionsUI() {
  // Uninitialize all registered handlers. The base class owns them and it will
  // eventually delete them.
  for (std::vector<DOMMessageHandler*>::iterator iter = handlers_.begin();
       iter != handlers_.end();
       ++iter) {
    reinterpret_cast<OptionsPageUIHandler*>(*iter)->Uninitialize();
  }
}

// static
RefCountedMemory* OptionsUI::GetFaviconResourceBytes() {
  return ResourceBundle::GetSharedInstance().
      LoadDataResourceBytes(IDR_SETTINGS_FAVICON);
}

void OptionsUI::InitializeHandlers() {
  std::vector<DOMMessageHandler*>::iterator iter;
  for (iter = handlers_.begin(); iter != handlers_.end(); ++iter) {
    (static_cast<OptionsPageUIHandler*>(*iter))->Initialize();
  }
}

void OptionsUI::AddOptionsPageUIHandler(DictionaryValue* localized_strings,
                                        OptionsPageUIHandler* handler) {
  DCHECK(handler);
  handler->GetLocalizedValues(localized_strings);
  AddMessageHandler(handler->Attach(this));
}
