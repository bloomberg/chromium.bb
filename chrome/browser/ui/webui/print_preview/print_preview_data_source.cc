// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/print_preview_data_source.h"

#include <algorithm>
#include <vector>

#include "base/memory/ref_counted_memory.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "base/string_piece.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/printing/print_preview_data_service.h"
#include "chrome/common/url_constants.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/google_chrome_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

#if defined(OS_MACOSX)
// U+0028 U+21E7 U+2318 U+0050 U+0029 in UTF8
const char kAdvancedPrintShortcut[] = "\x28\xE2\x8c\xA5\xE2\x8C\x98\x50\x29";
#elif defined(OS_WIN)
const char kAdvancedPrintShortcut[] = "(Ctrl+Shift+P)";
#else
const char kAdvancedPrintShortcut[] = "(Shift+Ctrl+P)";
#endif

};  // namespace

PrintPreviewDataSource::PrintPreviewDataSource()
    : ChromeWebUIDataSource(chrome::kChromeUIPrintHost) {
  Init();
}

void PrintPreviewDataSource::Init() {
#if defined(OS_CHROMEOS)
  AddLocalizedString("title", IDS_PRINT_PREVIEW_GOOGLE_CLOUD_PRINT_TITLE);
#else
  AddLocalizedString("title", IDS_PRINT_PREVIEW_TITLE);
#endif
  AddLocalizedString("loading", IDS_PRINT_PREVIEW_LOADING);
  AddLocalizedString("noPlugin", IDS_PRINT_PREVIEW_NO_PLUGIN);
  AddLocalizedString("launchNativeDialog", IDS_PRINT_PREVIEW_NATIVE_DIALOG);
  AddLocalizedString("previewFailed", IDS_PRINT_PREVIEW_FAILED);
  AddLocalizedString("invalidPrinterSettings",
                     IDS_PRINT_PREVIEW_INVALID_PRINTER_SETTINGS);
  AddLocalizedString("printButton", IDS_PRINT_PREVIEW_PRINT_BUTTON);
  AddLocalizedString("saveButton", IDS_PRINT_PREVIEW_SAVE_BUTTON);
  AddLocalizedString("cancelButton", IDS_PRINT_PREVIEW_CANCEL_BUTTON);
  AddLocalizedString("printing", IDS_PRINT_PREVIEW_PRINTING);
  AddLocalizedString("printingToPDFInProgress",
                     IDS_PRINT_PREVIEW_PRINTING_TO_PDF_IN_PROGRESS);
#if defined(OS_MACOSX)
  AddLocalizedString("openingPDFInPreview",
                     IDS_PRINT_PREVIEW_OPENING_PDF_IN_PREVIEW);
#endif
  AddLocalizedString("destinationLabel", IDS_PRINT_PREVIEW_DESTINATION_LABEL);
  AddLocalizedString("copiesLabel", IDS_PRINT_PREVIEW_COPIES_LABEL);
  AddLocalizedString("examplePageRangeText",
                     IDS_PRINT_PREVIEW_EXAMPLE_PAGE_RANGE_TEXT);
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
  const string16 shortcut_text(UTF8ToUTF16(kAdvancedPrintShortcut));
#if defined(OS_CHROMEOS)
  AddString("systemDialogOption", l10n_util::GetStringFUTF16(
      IDS_PRINT_PREVIEW_CLOUD_DIALOG_OPTION,
      l10n_util::GetStringUTF16(IDS_GOOGLE_CLOUD_PRINT),
      shortcut_text));
#else
  AddString("systemDialogOption", l10n_util::GetStringFUTF16(
      IDS_PRINT_PREVIEW_SYSTEM_DIALOG_OPTION,
      shortcut_text));
#endif
  AddString("cloudPrintDialogOption",
            l10n_util::GetStringFUTF16(
                IDS_PRINT_PREVIEW_CLOUD_DIALOG_OPTION_NO_SHORTCUT,
                l10n_util::GetStringUTF16(IDS_GOOGLE_CLOUD_PRINT)));
#if defined(OS_MACOSX)
  AddLocalizedString("openPdfInPreviewOption",
                     IDS_PRINT_PREVIEW_OPEN_PDF_IN_PREVIEW_APP);
#endif
  AddString("printWithCloudPrintWait", l10n_util::GetStringFUTF16(
      IDS_PRINT_PREVIEW_PRINT_WITH_CLOUD_PRINT_WAIT,
      l10n_util::GetStringUTF16(IDS_GOOGLE_CLOUD_PRINT)));
  AddLocalizedString("pageRangeInstruction",
                     IDS_PRINT_PREVIEW_PAGE_RANGE_INSTRUCTION);
  AddLocalizedString("copiesInstruction", IDS_PRINT_PREVIEW_COPIES_INSTRUCTION);
  AddLocalizedString("incrementTitle", IDS_PRINT_PREVIEW_INCREMENT_TITLE);
  AddLocalizedString("decrementTitle", IDS_PRINT_PREVIEW_DECREMENT_TITLE);
  AddLocalizedString("printPagesLabel", IDS_PRINT_PREVIEW_PRINT_PAGES_LABEL);
  AddLocalizedString("optionsLabel", IDS_PRINT_PREVIEW_OPTIONS_LABEL);
  AddLocalizedString("optionHeaderFooter",
                     IDS_PRINT_PREVIEW_OPTION_HEADER_FOOTER);
  AddLocalizedString("optionFitToPage",
                     IDS_PRINT_PREVIEW_OPTION_FIT_TO_PAGE);
  AddLocalizedString("optionBackgroundColorsAndImages",
                     IDS_PRINT_PREVIEW_OPTION_BACKGROUND_COLORS_AND_IMAGES);
  AddLocalizedString("marginsLabel", IDS_PRINT_PREVIEW_MARGINS_LABEL);
  AddLocalizedString("defaultMargins", IDS_PRINT_PREVIEW_DEFAULT_MARGINS);
  AddLocalizedString("noMargins", IDS_PRINT_PREVIEW_NO_MARGINS);
  AddLocalizedString("customMargins", IDS_PRINT_PREVIEW_CUSTOM_MARGINS);
  AddLocalizedString("minimumMargins", IDS_PRINT_PREVIEW_MINIMUM_MARGINS);
  AddLocalizedString("top", IDS_PRINT_PREVIEW_TOP_MARGIN_LABEL);
  AddLocalizedString("bottom", IDS_PRINT_PREVIEW_BOTTOM_MARGIN_LABEL);
  AddLocalizedString("left", IDS_PRINT_PREVIEW_LEFT_MARGIN_LABEL);
  AddLocalizedString("right", IDS_PRINT_PREVIEW_RIGHT_MARGIN_LABEL);
  AddLocalizedString("destinationSearchTitle",
                     IDS_PRINT_PREVIEW_DESTINATION_SEARCH_TITLE);
  AddLocalizedString("userInfo", IDS_PRINT_PREVIEW_USER_INFO);
  AddLocalizedString("cloudPrintPromotion",
                     IDS_PRINT_PREVIEW_CLOUD_PRINT_PROMOTION);
  AddLocalizedString("searchBoxPlaceholder",
                     IDS_PRINT_PREVIEW_SEARCH_BOX_PLACEHOLDER);
  AddLocalizedString("noDestinationsMessage",
                     IDS_PRINT_PREVIEW_NO_DESTINATIONS_MESSAGE);
  AddLocalizedString("showAllButtonText",
                     IDS_PRINT_PREVIEW_SHOW_ALL_BUTTON_TEXT);
  AddLocalizedString("destinationCount", IDS_PRINT_PREVIEW_DESTINATION_COUNT);
  AddLocalizedString("recentDestinationsTitle",
                     IDS_PRINT_PREVIEW_RECENT_DESTINATIONS_TITLE);
  AddLocalizedString("localDestinationsTitle",
                     IDS_PRINT_PREVIEW_LOCAL_DESTINATIONS_TITLE);
  AddLocalizedString("cloudDestinationsTitle",
                     IDS_PRINT_PREVIEW_CLOUD_DESTINATIONS_TITLE);
  AddLocalizedString("manage", IDS_PRINT_PREVIEW_MANAGE);
  AddLocalizedString("setupCloudPrinters",
                     IDS_PRINT_PREVIEW_SETUP_CLOUD_PRINTERS);
  AddLocalizedString("changeDestination",
                     IDS_PRINT_PREVIEW_CHANGE_DESTINATION);
  AddLocalizedString("offlineForYear", IDS_PRINT_PREVIEW_OFFLINE_FOR_YEAR);
  AddLocalizedString("offlineForMonth", IDS_PRINT_PREVIEW_OFFLINE_FOR_MONTH);
  AddLocalizedString("offlineForWeek", IDS_PRINT_PREVIEW_OFFLINE_FOR_WEEK);
  AddLocalizedString("offline", IDS_PRINT_PREVIEW_OFFLINE);
  AddLocalizedString("fedexTos", IDS_PRINT_PREVIEW_FEDEX_TOS);
  AddLocalizedString("tosCheckboxLabel", IDS_PRINT_PREVIEW_TOS_CHECKBOX_LABEL);
  AddLocalizedString("noDestsPromoTitle",
                     IDS_PRINT_PREVIEW_NO_DESTS_PROMO_TITLE);
  AddLocalizedString("noDestsPromoBody", IDS_PRINT_PREVIEW_NO_DESTS_PROMO_BODY);
  AddLocalizedString("noDestsPromoGcpDesc",
                     IDS_PRINT_PREVIEW_NO_DESTS_GCP_DESC);
  AddLocalizedString("noDestsPromoAddPrinterButtonLabel",
                     IDS_PRINT_PREVIEW_NO_DESTS_PROMO_ADD_PRINTER_BUTTON_LABEL);
  AddLocalizedString("noDestsPromoNotNowButtonLabel",
                     IDS_PRINT_PREVIEW_NO_DESTS_PROMO_NOT_NOW_BUTTON_LABEL);

  set_json_path("strings.js");
  add_resource_path("print_preview.js", IDR_PRINT_PREVIEW_JS);
  add_resource_path("images/printer.png", IDR_PRINT_PREVIEW_IMAGES_PRINTER);
  add_resource_path("images/printer_shared.png",
                    IDR_PRINT_PREVIEW_IMAGES_PRINTER_SHARED);
  add_resource_path("images/third_party.png",
                    IDR_PRINT_PREVIEW_IMAGES_THIRD_PARTY);
  add_resource_path("images/third_party_fedex.png",
                    IDR_PRINT_PREVIEW_IMAGES_THIRD_PARTY_FEDEX);
  add_resource_path("images/google_doc.png",
                    IDR_PRINT_PREVIEW_IMAGES_GOOGLE_DOC);
  add_resource_path("images/pdf.png", IDR_PRINT_PREVIEW_IMAGES_PDF);
  add_resource_path("images/mobile.png", IDR_PRINT_PREVIEW_IMAGES_MOBILE);
  add_resource_path("images/mobile_shared.png",
                    IDR_PRINT_PREVIEW_IMAGES_MOBILE_SHARED);
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
  scoped_refptr<base::RefCountedBytes> data;
  std::vector<std::string> url_substr;
  base::SplitString(path, '/', &url_substr);
  int preview_ui_id = -1;
  int page_index = 0;
  if (url_substr.size() == 3 &&
      base::StringToInt(url_substr[0], &preview_ui_id),
      base::StringToInt(url_substr[1], &page_index) &&
      preview_ui_id >= 0) {
    PrintPreviewDataService::GetInstance()->GetDataEntry(
        preview_ui_id, page_index, &data);
  }
  if (data.get()) {
    SendResponse(request_id, data);
    return;
  }
  // Invalid request.
  scoped_refptr<base::RefCountedBytes> empty_bytes(new base::RefCountedBytes);
  SendResponse(request_id, empty_bytes);
}
