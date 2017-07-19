// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_print_manager.h"

#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "components/printing/browser/print_manager_utils.h"
#include "components/printing/common/print_messages.h"
#include "printing/pdf_metafile_skia.h"
#include "printing/print_job_constants.h"
#include "printing/units.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(headless::HeadlessPrintManager);

namespace headless {

HeadlessPrintSettings::HeadlessPrintSettings()
    : landscape(false),
      display_header_footer(false),
      should_print_backgrounds(false),
      scale(1),
      ignore_invalid_page_ranges(false) {}

HeadlessPrintManager::HeadlessPrintManager(content::WebContents* web_contents)
    : PrintManager(web_contents) {
  Reset();
}

HeadlessPrintManager::~HeadlessPrintManager() {}

// static
std::string HeadlessPrintManager::PrintResultToString(PrintResult result) {
  switch (result) {
    case PRINT_SUCCESS:
      return std::string();  // no error message
    case PRINTING_FAILED:
      return "Printing failed";
    case INVALID_PRINTER_SETTINGS:
      return "Show invalid printer settings error";
    case INVALID_MEMORY_HANDLE:
      return "Invalid memory handle";
    case METAFILE_MAP_ERROR:
      return "Map to shared memory error";
    case UNEXPECTED_VALID_MEMORY_HANDLE:
      return "Unexpected valide memory handle";
    case METAFILE_INVALID_HEADER:
      return "Invalid metafile header";
    case METAFILE_GET_DATA_ERROR:
      return "Get data from metafile error";
    case SIMULTANEOUS_PRINT_ACTIVE:
      return "The previous printing job hasn't finished";
    case PAGE_RANGE_SYNTAX_ERROR:
      return "Page range syntax error";
    case PAGE_COUNT_EXCEEDED:
      return "Page range exceeds page count";
    default:
      NOTREACHED();
      return "Unknown PrintResult";
  }
}

// static
std::unique_ptr<base::DictionaryValue>
HeadlessPrintManager::PDFContentsToDictionaryValue(const std::string& data) {
  std::string base_64_data;
  base::Base64Encode(data, &base_64_data);
  auto result = base::MakeUnique<base::DictionaryValue>();
  result->SetString("data", base_64_data);
  return result;
}

// static
HeadlessPrintManager::PageRangeStatus
HeadlessPrintManager::PageRangeTextToPages(base::StringPiece page_range_text,
                                           bool ignore_invalid_page_ranges,
                                           int pages_count,
                                           std::vector<int>* pages) {
  printing::PageRanges page_ranges;
  for (const auto& range_string :
       base::SplitStringPiece(page_range_text, ",", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    printing::PageRange range;
    if (range_string.find("-") == base::StringPiece::npos) {
      if (!base::StringToInt(range_string, &range.from))
        return SYNTAX_ERROR;
      range.to = range.from;
    } else if (range_string == "-") {
      range.from = 1;
      range.to = pages_count;
    } else if (range_string.starts_with("-")) {
      range.from = 1;
      if (!base::StringToInt(range_string.substr(1), &range.to))
        return SYNTAX_ERROR;
    } else if (range_string.ends_with("-")) {
      range.to = pages_count;
      if (!base::StringToInt(range_string.substr(0, range_string.length() - 1),
                             &range.from))
        return SYNTAX_ERROR;
    } else {
      auto tokens = base::SplitStringPiece(
          range_string, "-", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
      if (tokens.size() != 2 || !base::StringToInt(tokens[0], &range.from) ||
          !base::StringToInt(tokens[1], &range.to))
        return SYNTAX_ERROR;
    }

    if (range.from < 1 || range.from > range.to) {
      if (!ignore_invalid_page_ranges)
        return SYNTAX_ERROR;
      continue;
    }
    if (range.from > pages_count) {
      if (!ignore_invalid_page_ranges)
        return LIMIT_ERROR;
      continue;
    }

    if (range.to > pages_count)
      range.to = pages_count;

    // Page numbers are 1-based in the dictionary.
    // Page numbers are 0-based for the print settings.
    range.from--;
    range.to--;
    page_ranges.push_back(range);
  }
  *pages = printing::PageRange::GetPages(page_ranges);
  return PRINT_NO_ERROR;
}

void HeadlessPrintManager::GetPDFContents(content::RenderFrameHost* rfh,
                                          const HeadlessPrintSettings& settings,
                                          const GetPDFCallback& callback) {
  DCHECK(callback);

  if (callback_) {
    callback.Run(SIMULTANEOUS_PRINT_ACTIVE, std::string());
    return;
  }
  printing_rfh_ = rfh;
  callback_ = callback;
  print_params_ = GetPrintParamsFromSettings(settings);
  page_ranges_text_ = settings.page_ranges;
  ignore_invalid_page_ranges_ = settings.ignore_invalid_page_ranges;
  rfh->Send(new PrintMsg_PrintPages(rfh->GetRoutingID()));
}

std::unique_ptr<PrintMsg_PrintPages_Params>
HeadlessPrintManager::GetPrintParamsFromSettings(
    const HeadlessPrintSettings& settings) {
  printing::PrintSettings print_settings;
  print_settings.set_dpi(printing::kPointsPerInch);
  print_settings.set_should_print_backgrounds(
      settings.should_print_backgrounds);
  print_settings.set_scale_factor(settings.scale);
  print_settings.SetOrientation(settings.landscape);

  print_settings.set_display_header_footer(settings.display_header_footer);
  if (print_settings.display_header_footer()) {
    url::Replacements<char> url_sanitizer;
    url_sanitizer.ClearUsername();
    url_sanitizer.ClearPassword();
    std::string url = printing_rfh_->GetLastCommittedURL()
                          .ReplaceComponents(url_sanitizer)
                          .spec();
    print_settings.set_url(base::UTF8ToUTF16(url));
  }

  print_settings.set_margin_type(printing::CUSTOM_MARGINS);
  print_settings.SetCustomMargins(settings.margins_in_points);

  gfx::Rect printable_area_device_units(settings.paper_size_in_points);
  print_settings.SetPrinterPrintableArea(settings.paper_size_in_points,
                                         printable_area_device_units, true);

  auto print_params = base::MakeUnique<PrintMsg_PrintPages_Params>();
  printing::RenderParamsFromPrintSettings(print_settings,
                                          &print_params->params);
  print_params->params.document_cookie = printing::PrintSettings::NewCookie();
  return print_params;
}

bool HeadlessPrintManager::OnMessageReceived(
    const IPC::Message& message,
    content::RenderFrameHost* render_frame_host) {
  if (!printing_rfh_ &&
      (message.type() == PrintHostMsg_GetDefaultPrintSettings::ID ||
       message.type() == PrintHostMsg_ScriptedPrint::ID)) {
    std::string type;
    switch (message.type()) {
      case PrintHostMsg_GetDefaultPrintSettings::ID:
        type = "GetDefaultPrintSettings";
        break;
      case PrintHostMsg_ScriptedPrint::ID:
        type = "ScriptedPrint";
        break;
      default:
        type = "Unknown";
        break;
    }
    DLOG(ERROR)
        << "Unexpected message received before GetPDFContents is called: "
        << type;

    // TODO: consider propagating the error back to the caller, rather than
    // effectively dropping the request.
    render_frame_host->Send(IPC::SyncMessage::GenerateReply(&message));
    return true;
  }

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(HeadlessPrintManager, message)
    IPC_MESSAGE_HANDLER(PrintHostMsg_ShowInvalidPrinterSettingsError,
                        OnShowInvalidPrinterSettingsError)
    IPC_MESSAGE_HANDLER(PrintHostMsg_DidPrintPage, OnDidPrintPage)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(PrintHostMsg_GetDefaultPrintSettings,
                                    OnGetDefaultPrintSettings)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(PrintHostMsg_ScriptedPrint, OnScriptedPrint)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled || PrintManager::OnMessageReceived(message, render_frame_host);
}

void HeadlessPrintManager::OnGetDefaultPrintSettings(IPC::Message* reply_msg) {
  PrintHostMsg_GetDefaultPrintSettings::WriteReplyParams(reply_msg,
                                                         print_params_->params);
  printing_rfh_->Send(reply_msg);
}

void HeadlessPrintManager::OnScriptedPrint(
    const PrintHostMsg_ScriptedPrint_Params& params,
    IPC::Message* reply_msg) {
  PageRangeStatus status =
      PageRangeTextToPages(page_ranges_text_, ignore_invalid_page_ranges_,
                           params.expected_pages_count, &print_params_->pages);
  switch (status) {
    case SYNTAX_ERROR:
      printing_rfh_->Send(reply_msg);
      ReleaseJob(PAGE_RANGE_SYNTAX_ERROR);
      return;
    case LIMIT_ERROR:
      printing_rfh_->Send(reply_msg);
      ReleaseJob(PAGE_COUNT_EXCEEDED);
      return;
    case PRINT_NO_ERROR:
      PrintHostMsg_ScriptedPrint::WriteReplyParams(reply_msg, *print_params_);
      printing_rfh_->Send(reply_msg);
      return;
    default:
      NOTREACHED();
      return;
  }
}

void HeadlessPrintManager::OnShowInvalidPrinterSettingsError() {
  ReleaseJob(INVALID_PRINTER_SETTINGS);
}

void HeadlessPrintManager::OnPrintingFailed(int cookie) {
  ReleaseJob(PRINTING_FAILED);
}

void HeadlessPrintManager::OnDidGetPrintedPagesCount(int cookie,
                                                     int number_pages) {
  PrintManager::OnDidGetPrintedPagesCount(cookie, number_pages);
  if (!print_params_->pages.empty())
    number_pages_ = print_params_->pages.size();
}

void HeadlessPrintManager::OnDidPrintPage(
    const PrintHostMsg_DidPrintPage_Params& params) {
  const bool metafile_must_be_valid = expecting_first_page_;
  expecting_first_page_ = false;

  if (metafile_must_be_valid) {
    if (!base::SharedMemory::IsHandleValid(params.metafile_data_handle)) {
      ReleaseJob(INVALID_MEMORY_HANDLE);
      return;
    }
    auto shared_buf =
        base::MakeUnique<base::SharedMemory>(params.metafile_data_handle, true);
    if (!shared_buf->Map(params.data_size)) {
      ReleaseJob(METAFILE_MAP_ERROR);
      return;
    }
    auto metafile = base::MakeUnique<printing::PdfMetafileSkia>(
        printing::PDF_SKIA_DOCUMENT_TYPE);
    if (!metafile->InitFromData(shared_buf->memory(), params.data_size)) {
      ReleaseJob(METAFILE_INVALID_HEADER);
      return;
    }
    std::vector<char> buffer;
    if (!metafile->GetDataAsVector(&buffer)) {
      ReleaseJob(METAFILE_GET_DATA_ERROR);
      return;
    }
    data_ = std::string(buffer.data(), buffer.size());
  } else {
    if (base::SharedMemory::IsHandleValid(params.metafile_data_handle)) {
      base::SharedMemory::CloseHandle(params.metafile_data_handle);
      ReleaseJob(UNEXPECTED_VALID_MEMORY_HANDLE);
      return;
    }
  }

  if (--number_pages_ == 0)
    ReleaseJob(PRINT_SUCCESS);
}

void HeadlessPrintManager::Reset() {
  printing_rfh_ = nullptr;
  callback_.Reset();
  print_params_.reset();
  page_ranges_text_.clear();
  ignore_invalid_page_ranges_ = false;
  data_.clear();
  expecting_first_page_ = true;
  number_pages_ = 0;
}

void HeadlessPrintManager::ReleaseJob(PrintResult result) {
  if (!callback_) {
    DLOG(ERROR) << "ReleaseJob is called when callback_ is null. Check whether "
                   "ReleaseJob is called more than once.";
    return;
  }

  if (result == PRINT_SUCCESS)
    callback_.Run(result, std::move(data_));
  else
    callback_.Run(result, std::string());
  printing_rfh_->Send(new PrintMsg_PrintingDone(printing_rfh_->GetRoutingID(),
                                                result == PRINT_SUCCESS));
  Reset();
}

}  // namespace headless
