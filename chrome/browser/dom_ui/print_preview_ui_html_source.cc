// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/print_preview_ui_html_source.h"

#include <algorithm>
#include <vector>

#include "base/message_loop.h"
#include "base/shared_memory.h"
#include "base/string_piece.h"
#include "base/values.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/url_constants.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

void SetLocalizedStrings(DictionaryValue* localized_strings) {
  localized_strings->SetString(std::string("title"),
      l10n_util::GetStringUTF8(IDS_PRINT_PREVIEW_TITLE));
  localized_strings->SetString(std::string("loading"),
      l10n_util::GetStringUTF8(IDS_PRINT_PREVIEW_LOADING));
  localized_strings->SetString(std::string("noPlugin"),
      l10n_util::GetStringUTF8(IDS_PRINT_PREVIEW_NO_PLUGIN));
  localized_strings->SetString(std::string("noPrinter"),
      l10n_util::GetStringUTF8(IDS_PRINT_PREVIEW_NO_PRINTER));

  localized_strings->SetString(std::string("printButton"),
      l10n_util::GetStringUTF8(IDS_PRINT_PREVIEW_PRINT_BUTTON));
  localized_strings->SetString(std::string("cancelButton"),
      l10n_util::GetStringUTF8(IDS_PRINT_PREVIEW_CANCEL_BUTTON));

  localized_strings->SetString(std::string("optionAllPages"),
      l10n_util::GetStringUTF8(IDS_PRINT_PREVIEW_OPTION_ALL_PAGES));
  localized_strings->SetString(std::string("optionBw"),
      l10n_util::GetStringUTF8(IDS_PRINT_PREVIEW_OPTION_BW));
  localized_strings->SetString(std::string("optionCollate"),
      l10n_util::GetStringUTF8(IDS_PRINT_PREVIEW_OPTION_COLLATE));
  localized_strings->SetString(std::string("optionColor"),
      l10n_util::GetStringUTF8(IDS_PRINT_PREVIEW_OPTION_COLOR));
  localized_strings->SetString(std::string("optionLandscape"),
      l10n_util::GetStringUTF8(IDS_PRINT_PREVIEW_OPTION_LANDSCAPE));
  localized_strings->SetString(std::string("optionPortrait"),
      l10n_util::GetStringUTF8(IDS_PRINT_PREVIEW_OPTION_PORTRAIT));
  localized_strings->SetString(std::string("optionTwoSided"),
      l10n_util::GetStringUTF8(IDS_PRINT_PREVIEW_OPTION_TWO_SIDED));
}

}  // namespace

PrintPreviewUIHTMLSource::PrintPreviewUIHTMLSource()
    : DataSource(chrome::kChromeUIPrintHost, MessageLoop::current()),
      data_(std::make_pair(static_cast<base::SharedMemory*>(NULL), 0U)) {
}

PrintPreviewUIHTMLSource::~PrintPreviewUIHTMLSource() {
  delete data_.first;
}

void PrintPreviewUIHTMLSource::GetPrintPreviewData(PrintPreviewData* data) {
  *data = data_;
}

void PrintPreviewUIHTMLSource::SetPrintPreviewData(
    const PrintPreviewData& data) {
  delete data_.first;
  data_ = data;
}

void PrintPreviewUIHTMLSource::StartDataRequest(const std::string& path,
                                                bool is_off_the_record,
                                                int request_id) {
  if (path.empty()) {
    // Print Preview Index page.
    DictionaryValue localized_strings;
    SetLocalizedStrings(&localized_strings);
    SetFontAndTextDirection(&localized_strings);

    static const base::StringPiece print_html(
        ResourceBundle::GetSharedInstance().GetRawDataResource(
            IDR_PRINT_PREVIEW_HTML));
    const std::string full_html = jstemplate_builder::GetI18nTemplateHtml(
        print_html, &localized_strings);

    scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);
    html_bytes->data.resize(full_html.size());
    std::copy(full_html.begin(), full_html.end(), html_bytes->data.begin());

    SendResponse(request_id, html_bytes);
    return;
  } else if (path == "print.pdf" && data_.first) {
    // Print Preview data.
    char* preview_data = reinterpret_cast<char*>(data_.first->memory());
    uint32 preview_data_size = data_.second;

    scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);
    html_bytes->data.resize(preview_data_size);
    std::vector<unsigned char>::iterator it = html_bytes->data.begin();
    for (uint32 i = 0; i < preview_data_size; ++i, ++it)
      *it = *(preview_data + i);
    SendResponse(request_id, html_bytes);
    return;
  } else {
    // Invalid request.
    scoped_refptr<RefCountedBytes> empty_bytes(new RefCountedBytes);
    SendResponse(request_id, empty_bytes);
    return;
  }
}

std::string PrintPreviewUIHTMLSource::GetMimeType(
    const std::string& path) const {
  // Print Preview Index page.
  if (path.empty())
    return "text/html";
  // Print Preview data.
  return "application/pdf";
}
