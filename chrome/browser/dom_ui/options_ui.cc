// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/options_ui.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/callback.h"
#include "base/i18n/time_formatting.h"
#include "base/message_loop.h"
#include "base/singleton.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/dom_ui/advanced_options_handler.h"
#include "chrome/browser/dom_ui/browser_options_handler.h"
#include "chrome/browser/dom_ui/clear_browser_data_handler.h"
#include "chrome/browser/dom_ui/content_settings_handler.h"
#include "chrome/browser/dom_ui/core_options_handler.h"
#include "chrome/browser/dom_ui/font_settings_handler.h"
#include "chrome/browser/dom_ui/personal_options_handler.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/pref_service.h"
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
#include "chrome/browser/chromeos/dom_ui/labs_handler.h"
#include "chrome/browser/chromeos/dom_ui/language_hangul_options_handler.h"
#include "chrome/browser/chromeos/dom_ui/language_options_handler.h"
#include "chrome/browser/chromeos/dom_ui/sync_options_handler.h"
#include "chrome/browser/chromeos/dom_ui/system_options_handler.h"
#endif

////////////////////////////////////////////////////////////////////////////////
//
// OptionsUIHTMLSource
//
////////////////////////////////////////////////////////////////////////////////

OptionsUIHTMLSource::OptionsUIHTMLSource(DictionaryValue* localized_strings)
    : DataSource(chrome::kChromeUIOptionsHost, MessageLoop::current()) {
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
    const UserMetricsAction& action, PrefService* prefs) {
  UserMetrics::RecordAction(action, dom_ui_->GetProfile());
  if (prefs)
    prefs->ScheduleSavePersistentPrefs();
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

  // TODO(zelidrag): Add all other page handlers here as we implement them.
  AddOptionsPageUIHandler(localized_strings, new BrowserOptionsHandler());
  AddOptionsPageUIHandler(localized_strings, new PersonalOptionsHandler());
  AddOptionsPageUIHandler(localized_strings, new AdvancedOptionsHandler());
#if defined(OS_CHROMEOS)
  AddOptionsPageUIHandler(localized_strings, new SystemOptionsHandler());
  AddOptionsPageUIHandler(localized_strings, new SyncOptionsHandler());
  AddOptionsPageUIHandler(localized_strings, new LabsHandler());
  AddOptionsPageUIHandler(localized_strings,
                          new LanguageHangulOptionsHandler());
  AddOptionsPageUIHandler(localized_strings, new LanguageOptionsHandler());
  AddOptionsPageUIHandler(localized_strings,
                          new chromeos::AccountsOptionsHandler());
#endif
  AddOptionsPageUIHandler(localized_strings, new ContentSettingsHandler());
  AddOptionsPageUIHandler(localized_strings, new ClearBrowserDataHandler());
  AddOptionsPageUIHandler(localized_strings, new FontSettingsHandler());

  // |localized_strings| ownership is taken over by this constructor.
  OptionsUIHTMLSource* html_source =
      new OptionsUIHTMLSource(localized_strings);

  // Set up the chrome://options/ source.
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(
          Singleton<ChromeURLDataManager>::get(),
          &ChromeURLDataManager::AddDataSource,
          make_scoped_refptr(html_source)));
}

// static
RefCountedMemory* OptionsUI::GetFaviconResourceBytes() {
// TODO(csilv): uncomment this once we have a FAVICON
//  return ResourceBundle::GetSharedInstance().
//      LoadDataResourceBytes(IDR_OPTIONS_FAVICON);
  return NULL;
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
