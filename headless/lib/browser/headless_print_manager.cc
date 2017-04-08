// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_print_manager.h"

#include <utility>
#include <vector>

#include "base/base64.h"
#include "components/printing/browser/print_manager_utils.h"
#include "components/printing/common/print_messages.h"
#include "printing/pdf_metafile_skia.h"
#include "printing/print_settings.h"
#include "printing/units.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(printing::HeadlessPrintManager);

namespace printing {

namespace {

// TODO(jzfeng): let the print settings to be configurable.
const double kTopMarginInInch = 0.25;
const double kBottomMarginInInch = 0.56;
const double kLeftMarginInInch = 0.25;
const double kRightMarginInInch = 0.25;

PrintSettings GetDefaultPDFPrinterSettings() {
#if defined(OS_MACOSX)
  // On the Mac, the printable area is in points, use kPointsPerInch to compute
  // its bounds.
  int dpi = kPointsPerInch;
#else
  int dpi = kDefaultPdfDpi;
#endif
  gfx::Size physical_size_device_units;
  gfx::Rect printable_area_device_units;
  double page_width_in_pixel = kLetterWidthInch * dpi;
  double page_height_in_pixel = kLetterHeightInch * dpi;
  physical_size_device_units.SetSize(static_cast<int>(page_width_in_pixel),
                                     static_cast<int>(page_height_in_pixel));
  printable_area_device_units.SetRect(
      static_cast<int>(kLeftMarginInInch * dpi),
      static_cast<int>(kTopMarginInInch * dpi),
      page_width_in_pixel - (kLeftMarginInInch + kRightMarginInInch) * dpi,
      page_height_in_pixel - (kTopMarginInInch + kBottomMarginInInch) * dpi);

  PrintSettings settings;
  settings.set_dpi(dpi);
  settings.SetPrinterPrintableArea(physical_size_device_units,
                                   printable_area_device_units, true);
  settings.set_should_print_backgrounds(true);
  return settings;
}

}  // namespace

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

void HeadlessPrintManager::GetPDFContents(content::RenderFrameHost* rfh,
                                          const GetPDFCallback& callback) {
  DCHECK(callback);

  if (callback_) {
    callback.Run(SIMULTANEOUS_PRINT_ACTIVE, std::string());
    return;
  }
  printing_rfh_ = rfh;
  callback_ = callback;
  rfh->Send(new PrintMsg_PrintPages(rfh->GetRoutingID()));
}

bool HeadlessPrintManager::OnMessageReceived(
    const IPC::Message& message,
    content::RenderFrameHost* render_frame_host) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(HeadlessPrintManager, message)
    IPC_MESSAGE_HANDLER(PrintHostMsg_ShowInvalidPrinterSettingsError,
                        OnShowInvalidPrinterSettingsError)
    IPC_MESSAGE_HANDLER(PrintHostMsg_DidPrintPage, OnDidPrintPage)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(PrintHostMsg_GetDefaultPrintSettings,
                                    OnGetDefaultPrintSettings)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled || PrintManager::OnMessageReceived(message, render_frame_host);
}

void HeadlessPrintManager::OnGetDefaultPrintSettings(IPC::Message* reply_msg) {
  PrintMsg_Print_Params print_params;
  RenderParamsFromPrintSettings(GetDefaultPDFPrinterSettings(), &print_params);
  print_params.document_cookie = PrintSettings::NewCookie();
  PrintHostMsg_GetDefaultPrintSettings::WriteReplyParams(reply_msg,
                                                         print_params);
  printing_rfh_->Send(reply_msg);
}

void HeadlessPrintManager::OnShowInvalidPrinterSettingsError() {
  ReleaseJob(INVALID_PRINTER_SETTINGS);
}

void HeadlessPrintManager::OnPrintingFailed(int cookie) {
  ReleaseJob(PRINTING_FAILED);
}

void HeadlessPrintManager::OnDidPrintPage(
    const PrintHostMsg_DidPrintPage_Params& params) {
  if (!callback_) {
    DLOG(ERROR)
        << "Unexpected PrintHostMsg_DidPrintPage message from the renderer";
    return;
  }

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
    auto metafile = base::MakeUnique<PdfMetafileSkia>(PDF_SKIA_DOCUMENT_TYPE);
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

}  // namespace printing
