// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/cloud_print/printer_info.h"

#include <windows.h>
#include <objidl.h>
#include <ocidl.h>
#include <olectl.h>
#include <prntvpt.h>
#include <winspool.h>

#include "base/lock.h"
#include "base/object_watcher.h"
#include "base/scoped_bstr_win.h"
#include "base/scoped_comptr_win.h"
#include "base/scoped_handle_win.h"
#include "base/scoped_ptr.h"
#include "base/utf_string_conversions.h"

#pragma comment(lib, "prntvpt.lib")
#pragma comment(lib, "rpcrt4.lib")

namespace {

class DevMode {
 public:
  DevMode() : dm_(NULL) {}
  ~DevMode() { Free(); }

  void Allocate(int size) {
    Free();
    dm_ = reinterpret_cast<DEVMODE*>(new char[size]);
  }

  void Free() {
    if (dm_)
      delete dm_;
    dm_ = NULL;
  }

  DEVMODE* dm_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DevMode);
};

bool InitXPSModule() {
  HMODULE prntvpt_module = LoadLibrary(L"prntvpt.dll");
  return (NULL != prntvpt_module);
}

inline HRESULT GetLastErrorHR() {
  LONG error = GetLastError();
  return HRESULT_FROM_WIN32(error);
}

HRESULT StreamFromPrintTicket(const std::string& print_ticket,
                              IStream** stream) {
  DCHECK(stream);
  HRESULT hr = CreateStreamOnHGlobal(NULL, TRUE, stream);
  if (FAILED(hr)) {
    return hr;
  }
  ULONG bytes_written = 0;
  (*stream)->Write(print_ticket.c_str(), print_ticket.length(), &bytes_written);
  DCHECK(bytes_written == print_ticket.length());
  LARGE_INTEGER pos = {0};
  ULARGE_INTEGER new_pos = {0};
  (*stream)->Seek(pos, STREAM_SEEK_SET, &new_pos);
  return S_OK;
}

HRESULT StreamOnHGlobalToString(IStream* stream, std::string* out) {
  DCHECK(stream);
  DCHECK(out);
  HGLOBAL hdata = NULL;
  HRESULT hr = GetHGlobalFromStream(stream, &hdata);
  if (SUCCEEDED(hr)) {
    DCHECK(hdata);
    ScopedHGlobal<char> locked_data(hdata);
    out->assign(locked_data.release(), locked_data.Size());
  }
  return hr;
}

HRESULT PrintTicketToDevMode(const std::string& printer_name,
                             const std::string& print_ticket,
                             DevMode* dev_mode) {
  DCHECK(dev_mode);

  ScopedComPtr<IStream> pt_stream;
  HRESULT hr = StreamFromPrintTicket(print_ticket, pt_stream.Receive());
  if (FAILED(hr))
    return hr;

  HPTPROVIDER provider = NULL;
  hr = PTOpenProvider(UTF8ToWide(printer_name).c_str(), 1, &provider);
  if (SUCCEEDED(hr)) {
    ULONG size = 0;
    DEVMODE* dm = NULL;
    hr = PTConvertPrintTicketToDevMode(provider,
                                       pt_stream,
                                       kUserDefaultDevmode,
                                       kPTDocumentScope,
                                       &size,
                                       &dm,
                                       NULL);
    if (SUCCEEDED(hr)) {
      dev_mode->Allocate(size);
      memcpy(dev_mode->dm_, dm, size);
      PTReleaseMemory(dm);
    }
    PTCloseProvider(provider);
  }
  return hr;
}

HRESULT PrintPdf2DC(HDC dc, const FilePath& pdf_filename) {
  HRESULT hr = E_NOTIMPL;
  // TODO(sanjeevr): Implement this.
  NOTIMPLEMENTED();
  return hr;
}

}  // namespace

namespace cloud_print {

void EnumeratePrinters(PrinterList* printer_list) {
  DCHECK(printer_list);
  DWORD bytes_needed = 0;
  DWORD count_returned = 0;
  BOOL ret = EnumPrinters(PRINTER_ENUM_LOCAL|PRINTER_ENUM_CONNECTIONS, NULL, 2,
                          NULL, 0, &bytes_needed, &count_returned);
  if (0 != bytes_needed) {
    scoped_ptr<BYTE> printer_info_buffer(new BYTE[bytes_needed]);
    ret = EnumPrinters(PRINTER_ENUM_LOCAL|PRINTER_ENUM_CONNECTIONS, NULL, 2,
                       printer_info_buffer.get(), bytes_needed, &bytes_needed,
                       &count_returned);
    DCHECK(ret);
    PRINTER_INFO_2* printer_info =
        reinterpret_cast<PRINTER_INFO_2*>(printer_info_buffer.get());
    for (DWORD index = 0; index < count_returned; index++) {
      PrinterBasicInfo info;
      info.printer_name = WideToUTF8(printer_info[index].pPrinterName);
      if (printer_info[index].pComment)
        info.printer_description = WideToUTF8(printer_info[index].pComment);
      info.printer_status = printer_info[index].Status;
      printer_list->push_back(info);
    }
  }
}

bool GetPrinterCapsAndDefaults(const std::string& printer_name,
                               PrinterCapsAndDefaults* printer_info) {
  if (!InitXPSModule()) {
    // TODO(sanjeevr): Handle legacy proxy case (with no prntvpt.dll)
    return false;
  }
  if (!IsValidPrinter(printer_name)) {
    return false;
  }
  DCHECK(printer_info);
  HPTPROVIDER provider = NULL;
  std::wstring printer_name_wide = UTF8ToWide(printer_name);
  HRESULT hr = PTOpenProvider(printer_name_wide.c_str(), 1, &provider);
  DCHECK(SUCCEEDED(hr));
  if (provider) {
    ScopedComPtr<IStream> print_capabilities_stream;
    hr = CreateStreamOnHGlobal(NULL, TRUE,
                               print_capabilities_stream.Receive());
    DCHECK(SUCCEEDED(hr));
    if (print_capabilities_stream) {
      ScopedBstr error;
      hr = PTGetPrintCapabilities(provider, NULL, print_capabilities_stream,
                                  error.Receive());
      DCHECK(SUCCEEDED(hr));
      if (FAILED(hr)) {
        return false;
      }
      hr = StreamOnHGlobalToString(print_capabilities_stream.get(),
                                   &printer_info->printer_capabilities);
      DCHECK(SUCCEEDED(hr));
      printer_info->caps_mime_type = "text/xml";
    }
    // TODO(sanjeevr): Add ScopedPrinterHandle
    HANDLE printer_handle = NULL;
    OpenPrinter(const_cast<LPTSTR>(printer_name_wide.c_str()), &printer_handle,
                NULL);
    DCHECK(printer_handle);
    if (printer_handle) {
      DWORD devmode_size = DocumentProperties(
          NULL, printer_handle, const_cast<LPTSTR>(printer_name_wide.c_str()),
          NULL, NULL, 0);
      DCHECK(0 != devmode_size);
      scoped_ptr<BYTE> devmode_out_buffer(new BYTE[devmode_size]);
      DEVMODE* devmode_out =
          reinterpret_cast<DEVMODE*>(devmode_out_buffer.get());
      DocumentProperties(
          NULL, printer_handle, const_cast<LPTSTR>(printer_name_wide.c_str()),
          devmode_out, NULL, DM_OUT_BUFFER);
      ScopedComPtr<IStream> printer_defaults_stream;
      hr = CreateStreamOnHGlobal(NULL, TRUE,
                                 printer_defaults_stream.Receive());
      DCHECK(SUCCEEDED(hr));
      if (printer_defaults_stream) {
        hr = PTConvertDevModeToPrintTicket(provider, devmode_size,
                                           devmode_out, kPTJobScope,
                                           printer_defaults_stream);
        DCHECK(SUCCEEDED(hr));
        if (SUCCEEDED(hr)) {
          hr = StreamOnHGlobalToString(printer_defaults_stream.get(),
                                       &printer_info->printer_defaults);
          DCHECK(SUCCEEDED(hr));
          printer_info->defaults_mime_type = "text/xml";
        }
      }
      ClosePrinter(printer_handle);
    }
    PTCloseProvider(provider);
  }
  return true;
}

bool ValidatePrintTicket(const std::string& printer_name,
                         const std::string& print_ticket_data) {
  if (!InitXPSModule()) {
    // TODO(sanjeevr): Handle legacy proxy case (with no prntvpt.dll)
    return false;
  }
  bool ret = false;
  HPTPROVIDER provider = NULL;
  PTOpenProvider(UTF8ToWide(printer_name.c_str()).c_str(), 1, &provider);
  if (provider) {
    ScopedComPtr<IStream> print_ticket_stream;
    CreateStreamOnHGlobal(NULL, TRUE, print_ticket_stream.Receive());
    ULONG bytes_written = 0;
    print_ticket_stream->Write(print_ticket_data.c_str(),
                               print_ticket_data.length(),
                               &bytes_written);
    DCHECK(bytes_written == print_ticket_data.length());
    LARGE_INTEGER pos = {0};
    ULARGE_INTEGER new_pos = {0};
    print_ticket_stream->Seek(pos, STREAM_SEEK_SET, &new_pos);
    ScopedBstr error;
    ScopedComPtr<IStream> result_ticket_stream;
    CreateStreamOnHGlobal(NULL, TRUE, result_ticket_stream.Receive());
    ret = SUCCEEDED(PTMergeAndValidatePrintTicket(provider,
                                                  print_ticket_stream.get(),
                                                  NULL,
                                                  kPTJobScope,
                                                  result_ticket_stream.get(),
                                                  error.Receive()));
    PTCloseProvider(provider);
  }
  return ret;
}

std::string GenerateProxyId() {
  GUID proxy_id = {0};
  HRESULT hr = UuidCreate(&proxy_id);
  DCHECK(SUCCEEDED(hr));
  wchar_t* proxy_id_as_string = NULL;
  UuidToString(&proxy_id, reinterpret_cast<RPC_WSTR *>(&proxy_id_as_string));
  DCHECK(proxy_id_as_string);
  std::string ret;
  WideToUTF8(proxy_id_as_string, wcslen(proxy_id_as_string), &ret);
  RpcStringFree(reinterpret_cast<RPC_WSTR *>(&proxy_id_as_string));
  return ret;
}

bool SpoolPrintJob(const std::string& print_ticket,
                   const FilePath& print_data_file_path,
                   const std::string& print_data_mime_type,
                   const std::string& printer_name,
                   const std::string& job_title,
                   PlatformJobId* job_id_ret) {
  if (!InitXPSModule()) {
    // TODO(sanjeevr): Handle legacy proxy case (with no prntvpt.dll)
    return false;
  }
  DevMode pt_dev_mode;
  HRESULT hr = PrintTicketToDevMode(printer_name, print_ticket, &pt_dev_mode);
  if (FAILED(hr)) {
    NOTREACHED();
    return false;
  }
  ScopedHDC dc(CreateDC(L"WINSPOOL", UTF8ToWide(printer_name).c_str(), NULL,
                        pt_dev_mode.dm_));
  if (!dc.Get()) {
    NOTREACHED();
    return false;
  }
  hr = E_FAIL;
  DOCINFO di = {0};
  di.cbSize = sizeof(DOCINFO);
  std::wstring doc_name = UTF8ToWide(job_title);
  di.lpszDocName = doc_name.c_str();
  int job_id = StartDoc(dc.Get(), &di);
  if (SP_ERROR != job_id) {
    if (print_data_mime_type == "application/pdf") {
      hr = PrintPdf2DC(dc.Get(), print_data_file_path);
    } else {
      NOTREACHED();
    }
    EndDoc(dc.Get());
    if (SUCCEEDED(hr) && job_id_ret) {
      *job_id_ret = job_id;
    }
  }
  return SUCCEEDED(hr);
}

bool GetJobDetails(const std::string& printer_name,
                   PlatformJobId job_id,
                   PrintJobDetails *job_details) {
  DCHECK(job_details);
  HANDLE printer_handle = NULL;
  std::wstring printer_name_wide = UTF8ToWide(printer_name);
  OpenPrinter(const_cast<LPTSTR>(printer_name_wide.c_str()), &printer_handle,
              NULL);
  DCHECK(printer_handle);
  bool ret = false;
  if (printer_handle) {
    DWORD bytes_needed = 0;
    GetJob(printer_handle, job_id, 1, NULL, 0, &bytes_needed);
    DWORD last_error = GetLastError();
    if (ERROR_INVALID_PARAMETER != last_error) {
      // ERROR_INVALID_PARAMETER normally means that the job id is not valid.
      DCHECK(last_error == ERROR_INSUFFICIENT_BUFFER);
      scoped_ptr<BYTE> job_info_buffer(new BYTE[bytes_needed]);
      if (GetJob(printer_handle, job_id, 1, job_info_buffer.get(), bytes_needed,
                &bytes_needed)) {
        JOB_INFO_1 *job_info =
            reinterpret_cast<JOB_INFO_1 *>(job_info_buffer.get());
        if (job_info->pStatus) {
          WideToUTF8(job_info->pStatus, wcslen(job_info->pStatus),
                     &job_details->status_message);
        }
        job_details->platform_status_flags = job_info->Status;
        if ((job_info->Status & JOB_STATUS_COMPLETE) ||
            (job_info->Status & JOB_STATUS_PRINTED)) {
          job_details->status = PRINT_JOB_STATUS_COMPLETED;
        } else if (job_info->Status & JOB_STATUS_ERROR) {
          job_details->status = PRINT_JOB_STATUS_ERROR;
        } else {
          job_details->status = PRINT_JOB_STATUS_IN_PROGRESS;
        }
        job_details->total_pages = job_info->TotalPages;
        job_details->pages_printed = job_info->PagesPrinted;
        ret = true;
      }
    }
    ClosePrinter(printer_handle);
  }
  return ret;
}

bool IsValidPrinter(const std::string& printer_name) {
  std::wstring printer_name_wide = UTF8ToWide(printer_name);
  HANDLE printer_handle = NULL;
  OpenPrinter(const_cast<LPTSTR>(printer_name_wide.c_str()), &printer_handle,
              NULL);
  bool ret = false;
  if (printer_handle) {
    ret = true;
    ClosePrinter(printer_handle);
  }
  return ret;
}

class PrinterChangeNotifier::NotificationState
    : public base::ObjectWatcher::Delegate {
 public:
  NotificationState() : printer_(NULL), printer_change_(NULL), delegate_(NULL) {
  }
  ~NotificationState() {
    Stop();
  }
  bool Start(const std::string& printer_name,
             PrinterChangeNotifier::Delegate* delegate) {
    delegate_ = delegate;
    // An empty printer name means watch the current server, we need to pass
    // NULL to OpenPrinter.
    LPTSTR printer_name_to_use = NULL;
    std::wstring printer_name_wide;
    if (!printer_name.empty()) {
      printer_name_wide = UTF8ToWide(printer_name);
      printer_name_to_use = const_cast<LPTSTR>(printer_name_wide.c_str());
    }
    bool ret = false;
    OpenPrinter(printer_name_to_use, &printer_, NULL);
    if (printer_) {
      printer_change_ = FindFirstPrinterChangeNotification(
          printer_, PRINTER_CHANGE_PRINTER|PRINTER_CHANGE_JOB, 0, NULL);
      if (printer_change_) {
        ret = watcher_.StartWatching(printer_change_, this);
      }
    }
    if (!ret) {
      Stop();
    }
    return ret;
  }
  bool Stop() {
    watcher_.StopWatching();
    if (printer_) {
      ClosePrinter(printer_);
      printer_ = NULL;
    }
    if (printer_change_) {
      FindClosePrinterChangeNotification(printer_change_);
      printer_change_ = NULL;
    }
    return true;
  }

  void OnObjectSignaled(HANDLE object) {
    DWORD change = 0;
    FindNextPrinterChangeNotification(object, &change, NULL, NULL);

    if (change != ((PRINTER_CHANGE_PRINTER|PRINTER_CHANGE_JOB) &
                  (~PRINTER_CHANGE_FAILED_CONNECTION_PRINTER))) {
      // For printer connections, we get spurious change notifications with
      // all flags set except PRINTER_CHANGE_FAILED_CONNECTION_PRINTER.
      // Ignore these.
      if (change & PRINTER_CHANGE_ADD_PRINTER) {
        delegate_->OnPrinterAdded();
      } else if (change & PRINTER_CHANGE_DELETE_PRINTER) {
        delegate_->OnPrinterDeleted();
      } else if (change & PRINTER_CHANGE_SET_PRINTER) {
        delegate_->OnPrinterChanged();
      }
      if (change & PRINTER_CHANGE_JOB) {
        delegate_->OnJobChanged();
      }
    }
    watcher_.StartWatching(printer_change_, this);
  }
  HANDLE printer_handle() const {
    return printer_;
  }
 private:
  base::ObjectWatcher watcher_;
  HANDLE printer_;            // The printer being watched
  HANDLE printer_change_;     // Returned by FindFirstPrinterChangeNotifier
  PrinterChangeNotifier::Delegate* delegate_;  // Delegate to notify
  bool did_signal_;           // DoneWaiting was called
};

PrinterChangeNotifier::PrinterChangeNotifier() : state_(NULL) {
}

PrinterChangeNotifier::~PrinterChangeNotifier() {
  StopWatching();
}

bool PrinterChangeNotifier::StartWatching(const std::string& printer_name,
                                          Delegate* delegate) {
  if (state_) {
    NOTREACHED();
    return false;
  }
  state_ = new NotificationState;
  if (!state_->Start(printer_name, delegate)) {
    StopWatching();
    return false;
  }
  return true;
}

bool PrinterChangeNotifier::StopWatching() {
  if (!state_) {
    return false;
  }
  state_->Stop();
  delete state_;
  state_ = NULL;
  return true;
}

bool PrinterChangeNotifier::GetCurrentPrinterInfo(
    PrinterBasicInfo* printer_info) {
  if (!state_) {
    return false;
  }
  DCHECK(printer_info);
  DWORD bytes_needed = 0;
  bool ret = false;
  GetPrinter(state_->printer_handle(), 2, NULL, 0, &bytes_needed);
  if (0 != bytes_needed) {
    scoped_ptr<BYTE> printer_info_buffer(new BYTE[bytes_needed]);
    if (GetPrinter(state_->printer_handle(), 2, printer_info_buffer.get(),
                   bytes_needed, &bytes_needed)) {
      PRINTER_INFO_2* printer_info_win =
          reinterpret_cast<PRINTER_INFO_2*>(printer_info_buffer.get());
      printer_info->printer_name = WideToUTF8(printer_info_win->pPrinterName);
      printer_info->printer_description =
          WideToUTF8(printer_info_win->pComment);
      printer_info->printer_status = printer_info_win->Status;
      ret = true;
    }
  }
  return ret;
}
}  // namespace cloud_print

