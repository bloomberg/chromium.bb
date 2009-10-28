// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/print_ui.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/message_loop.h"
#include "base/thread.h"
#include "base/values.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/url_constants.h"

#include "grit/browser_resources.h"
#include "grit/generated_resources.h"

///////////////////////////////////////////////////////////////////////////////
//
// PrintUI
//
///////////////////////////////////////////////////////////////////////////////

PrintUI::PrintUI(TabContents* contents) : DOMUI(contents) {
  PrintUIHTMLSource* html_source = new PrintUIHTMLSource();

  // Set up the print:url source.
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(
          &chrome_url_data_manager,
          &ChromeURLDataManager::AddDataSource,
          html_source));
}

////////////////////////////////////////////////////////////////////////////////
//
// PrintUIHTMLSource
//
////////////////////////////////////////////////////////////////////////////////

PrintUIHTMLSource::PrintUIHTMLSource()
    : DataSource(chrome::kPrintScheme, MessageLoop::current()) {
}

void PrintUIHTMLSource::StartDataRequest(const std::string& path,
                                         int request_id) {
  // Setup a dictionary so that the html page could read the values.
  DictionaryValue localized_strings;
  localized_strings.SetString(L"title", l10n_util::GetString(IDS_PRINT));

  SetFontAndTextDirection(&localized_strings);

  // Setup the print html page.
  static const base::StringPiece print_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_PRINT_TAB_HTML));
  const std::string full_html = jstemplate_builder::GetI18nTemplateHtml(
      print_html, &localized_strings);

  // Load the print html page into the tab contents.
  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);
  html_bytes->data.resize(full_html.size());
  std::copy(full_html.begin(), full_html.end(), html_bytes->data.begin());
  SendResponse(request_id, html_bytes);
}
