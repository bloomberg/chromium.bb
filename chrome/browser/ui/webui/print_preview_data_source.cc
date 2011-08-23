// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview_data_source.h"

#include <algorithm>

#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "base/string_piece.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/printing/print_preview_data_service.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/url_constants.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/google_chrome_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

PrintPreviewDataSource::PrintPreviewDataSource()
    : ChromeWebUIDataSource(chrome::kChromeUIPrintHost) {

  AddLocalizedString("title", IDS_PRINT_PREVIEW_TITLE);
  AddLocalizedString("loading", IDS_PRINT_PREVIEW_LOADING);
#if defined(GOOGLE_CHROME_BUILD)
  AddString("noPlugin", l10n_util::GetStringFUTF16(
      IDS_PRINT_PREVIEW_NO_PLUGIN, ASCIIToUTF16("chrome://plugins/")));
#else
  AddLocalizedString("noPlugin", IDS_PRINT_PREVIEW_NO_PLUGIN);
#endif
  AddLocalizedString("launchNativeDialog", IDS_PRINT_PREVIEW_NATIVE_DIALOG);
  AddLocalizedString("previewFailed", IDS_PRINT_PREVIEW_FAILED);
  AddLocalizedString("initiatorTabCrashed",
                     IDS_PRINT_PREVIEW_INITIATOR_TAB_CRASHED);
  AddLocalizedString("initiatorTabClosed",
                     IDS_PRINT_PREVIEW_INITIATOR_TAB_CLOSED);
  AddLocalizedString("reopenPage", IDS_PRINT_PREVIEW_REOPEN_PAGE);

  AddLocalizedString("printButton", IDS_PRINT_PREVIEW_PRINT_BUTTON);
  AddLocalizedString("cancelButton", IDS_PRINT_PREVIEW_CANCEL_BUTTON);
  AddLocalizedString("printing", IDS_PRINT_PREVIEW_PRINTING);

  AddLocalizedString("destinationLabel", IDS_PRINT_PREVIEW_DESTINATION_LABEL);
  AddLocalizedString("copiesLabel", IDS_PRINT_PREVIEW_COPIES_LABEL);
  AddLocalizedString("examplePageRangeText",
                     IDS_PRINT_PREVIEW_EXAMPLE_PAGE_RANGE_TEXT);
  AddLocalizedString("invalidNumberOfCopies",
                     IDS_PRINT_PREVIEW_INVALID_NUMBER_OF_COPIES);
  AddLocalizedString("layoutLabel", IDS_PRINT_PREVIEW_LAYOUT_LABEL);
  AddLocalizedString("optionAllPages", IDS_PRINT_PREVIEW_OPTION_ALL_PAGES);
  AddLocalizedString("optionBw", IDS_PRINT_PREVIEW_OPTION_BW);
  AddLocalizedString("optionCollate", IDS_PRINT_PREVIEW_OPTION_COLLATE);
  AddLocalizedString("optionColor", IDS_PRINT_PREVIEW_OPTION_COLOR);
  AddLocalizedString("optionLandscape", IDS_PRINT_PREVIEW_OPTION_LANDSCAPE);
  AddLocalizedString("optionPortrait", IDS_PRINT_PREVIEW_OPTION_PORTRAIT);
  AddLocalizedString("optionTwoSided", IDS_PRINT_PREVIEW_OPTION_TWO_SIDED);
  AddLocalizedString("pagesLabel", IDS_PRINT_PREVIEW_PAGES_LABEL);
  AddLocalizedString("pageRangeTextBox", IDS_PRINT_PREVIEW_PAGE_RANGE_TEXT);
  AddLocalizedString("pageRangeRadio", IDS_PRINT_PREVIEW_PAGE_RANGE_RADIO);
  AddLocalizedString("printToPDF", IDS_PRINT_PREVIEW_PRINT_TO_PDF);
  AddLocalizedString("printPreviewTitleFormat", IDS_PRINT_PREVIEW_TITLE_FORMAT);
  AddLocalizedString("printPreviewSummaryFormatShort",
                     IDS_PRINT_PREVIEW_SUMMARY_FORMAT_SHORT);
  AddLocalizedString("printPreviewSummaryFormatLong",
                     IDS_PRINT_PREVIEW_SUMMARY_FORMAT_LONG);
  AddLocalizedString("printPreviewSheetsLabelSingular",
                     IDS_PRINT_PREVIEW_SHEETS_LABEL_SINGULAR);
  AddLocalizedString("printPreviewSheetsLabelPlural",
                     IDS_PRINT_PREVIEW_SHEETS_LABEL_PLURAL);
  AddLocalizedString("printPreviewPageLabelSingular",
                     IDS_PRINT_PREVIEW_PAGE_LABEL_SINGULAR);
  AddLocalizedString("printPreviewPageLabelPlural",
                     IDS_PRINT_PREVIEW_PAGE_LABEL_PLURAL);
  AddLocalizedString("systemDialogOption",
                     IDS_PRINT_PREVIEW_SYSTEM_DIALOG_OPTION);
  AddLocalizedString("pageRangeInstruction",
                     IDS_PRINT_PREVIEW_PAGE_RANGE_INSTRUCTION);
  AddLocalizedString("copiesInstruction", IDS_PRINT_PREVIEW_COPIES_INSTRUCTION);
  AddLocalizedString("signIn", IDS_PRINT_PREVIEW_SIGN_IN);
  AddLocalizedString("morePrinters", IDS_PRINT_PREVIEW_MORE_PRINTERS);
  AddLocalizedString("addCloudPrinter", IDS_PRINT_PREVIEW_ADD_CLOUD_PRINTER);
  AddLocalizedString("cloudPrinters", IDS_PRINT_PREVIEW_CLOUD_PRINTERS);
  AddLocalizedString("localPrinters", IDS_PRINT_PREVIEW_LOCAL_PRINTERS);
  AddLocalizedString("manageCloudPrinters",
                     IDS_PRINT_PREVIEW_MANAGE_CLOUD_PRINTERS);
  AddLocalizedString("manageLocalPrinters",
                     IDS_PRINT_PREVIEW_MANAGE_LOCAL_PRINTERS);
  AddLocalizedString("managePrinters", IDS_PRINT_PREVIEW_MANAGE_PRINTERS);
  AddLocalizedString("incrementTitle", IDS_PRINT_PREVIEW_INCREMENT_TITLE);
  AddLocalizedString("decrementTitle", IDS_PRINT_PREVIEW_DECREMENT_TITLE);
  AddLocalizedString("printPagesLabel", IDS_PRINT_PREVIEW_PRINT_PAGES_LABEL);
  AddLocalizedString("optionsLabel", IDS_PRINT_PREVIEW_OPTIONS_LABEL);
  AddLocalizedString("optionHeaderFooter",
                     IDS_PRINT_PREVIEW_OPTION_HEADER_FOOTER);
  AddLocalizedString("marginsLabel", IDS_PRINT_PREVIEW_MARGINS_LABEL);
  AddLocalizedString("defaultMargins", IDS_PRINT_PREVIEW_DEFAULT_MARGINS);
  AddLocalizedString("noMargins", IDS_PRINT_PREVIEW_NO_MARGINS);
  AddLocalizedString("customMargins", IDS_PRINT_PREVIEW_CUSTOM_MARGINS);

  set_json_path("strings.js");
  add_resource_path("print_preview.js", IDR_PRINT_PREVIEW_JS);
  set_default_resource(IDR_PRINT_PREVIEW_HTML);
}

PrintPreviewDataSource::~PrintPreviewDataSource() {
}

void PrintPreviewDataSource::StartDataRequest(const std::string& path,
                                              bool is_incognito,
                                              int request_id) {
  // Parent class handles most requests except for the print preview data.
  if (!EndsWith(path, "/print.pdf", true)) {
    ChromeWebUIDataSource::StartDataRequest(path, is_incognito, request_id);
    return;
  }

  // Print Preview data.
  scoped_refptr<RefCountedBytes> data;
  std::vector<std::string> url_substr;
  base::SplitString(path, '/', &url_substr);
  int page_index = 0;
  if (url_substr.size() == 3 && base::StringToInt(url_substr[1], &page_index)) {
      PrintPreviewDataService::GetInstance()->GetDataEntry(
          url_substr[0], page_index, &data);
  }
  if (data.get()) {
    SendResponse(request_id, data);
    return;
  }
  // Invalid request.
  scoped_refptr<RefCountedBytes> empty_bytes(new RefCountedBytes);
  SendResponse(request_id, empty_bytes);
}
