// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/object_watcher.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_comptr.h"
#include "base/win/scoped_hdc.h"
#include "chrome/common/crash_keys.h"
#include "chrome/service/cloud_print/print_system_win.h"
#include "chrome/service/service_process.h"
#include "chrome/service/service_utility_process_host.h"
#include "grit/generated_resources.h"
#include "printing/backend/win_helper.h"
#include "printing/emf_win.h"
#include "printing/page_range.h"
#include "printing/printing_utils.h"
#include "ui/base/l10n/l10n_util.h"

namespace cloud_print {

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
      delete [] dm_;
    dm_ = NULL;
  }

  DEVMODE* dm_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DevMode);
};

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

HRESULT PrintTicketToDevMode(const std::string& printer_name,
                             const std::string& print_ticket,
                             DevMode* dev_mode) {
  DCHECK(dev_mode);
  printing::ScopedXPSInitializer xps_initializer;
  if (!xps_initializer.initialized()) {
    // TODO(sanjeevr): Handle legacy proxy case (with no prntvpt.dll)
    return E_FAIL;
  }

  base::win::ScopedComPtr<IStream> pt_stream;
  HRESULT hr = StreamFromPrintTicket(print_ticket, pt_stream.Receive());
  if (FAILED(hr))
    return hr;

  HPTPROVIDER provider = NULL;
  hr = printing::XPSModule::OpenProvider(UTF8ToWide(printer_name), 1,
                                         &provider);
  if (SUCCEEDED(hr)) {
    ULONG size = 0;
    DEVMODE* dm = NULL;
    // Use kPTJobScope, because kPTDocumentScope breaks duplex.
    hr = printing::XPSModule::ConvertPrintTicketToDevMode(provider,
                                                          pt_stream,
                                                          kUserDefaultDevmode,
                                                          kPTJobScope,
                                                          &size,
                                                          &dm,
                                                          NULL);
    if (SUCCEEDED(hr)) {
      dev_mode->Allocate(size);
      memcpy(dev_mode->dm_, dm, size);
      printing::XPSModule::ReleaseMemory(dm);
    }
    printing::XPSModule::CloseProvider(provider);
  }
  return hr;
}

class JobSpoolerWin : public PrintSystem::JobSpooler {
 public:
  JobSpoolerWin() : core_(new Core) {}

  // PrintSystem::JobSpooler implementation.
  virtual bool Spool(const std::string& print_ticket,
                     const base::FilePath& print_data_file_path,
                     const std::string& print_data_mime_type,
                     const std::string& printer_name,
                     const std::string& job_title,
                     const std::vector<std::string>& tags,
                     JobSpooler::Delegate* delegate) OVERRIDE {
    // TODO(gene): add tags handling.
    scoped_refptr<printing::PrintBackend> print_backend(
        printing::PrintBackend::CreateInstance(NULL));
    crash_keys::ScopedPrinterInfo crash_key(
        print_backend->GetPrinterDriverInfo(printer_name));
    return core_->Spool(print_ticket, print_data_file_path,
                        print_data_mime_type, printer_name, job_title,
                        delegate);
  }

 protected:
  virtual ~JobSpoolerWin() {}

 private:
  // We use a Core class because we want a separate RefCountedThreadSafe
  // implementation for ServiceUtilityProcessHost::Client.
  class Core : public ServiceUtilityProcessHost::Client,
               public base::win::ObjectWatcher::Delegate {
   public:
    Core()
        : last_page_printed_(-1),
          job_id_(-1),
          delegate_(NULL),
          saved_dc_(0) {
    }

    ~Core() {}

    bool Spool(const std::string& print_ticket,
               const base::FilePath& print_data_file_path,
               const std::string& print_data_mime_type,
               const std::string& printer_name,
               const std::string& job_title,
               JobSpooler::Delegate* delegate) {
      scoped_refptr<printing::PrintBackend> print_backend(
          printing::PrintBackend::CreateInstance(NULL));
      crash_keys::ScopedPrinterInfo crash_key(
          print_backend->GetPrinterDriverInfo(printer_name));
      if (delegate_) {
        // We are already in the process of printing.
        NOTREACHED();
        return false;
      }
      last_page_printed_ = -1;
      // We only support PDF and XPS documents for now.
      if (print_data_mime_type == "application/pdf") {
        DevMode pt_dev_mode;
        HRESULT hr = PrintTicketToDevMode(printer_name, print_ticket,
                                          &pt_dev_mode);
        if (FAILED(hr)) {
          NOTREACHED();
          return false;
        }

        HDC dc = CreateDC(L"WINSPOOL", UTF8ToWide(printer_name).c_str(),
                          NULL, pt_dev_mode.dm_);
        if (!dc) {
          NOTREACHED();
          return false;
        }
        hr = E_FAIL;
        DOCINFO di = {0};
        di.cbSize = sizeof(DOCINFO);
        string16 doc_name = UTF8ToUTF16(job_title);
        DCHECK(printing::SimplifyDocumentTitle(doc_name) == doc_name);
        di.lpszDocName = doc_name.c_str();
        job_id_ = StartDoc(dc, &di);
        if (job_id_ <= 0)
          return false;

        printer_dc_.Set(dc);
        saved_dc_ = SaveDC(printer_dc_.Get());
        print_data_file_path_ = print_data_file_path;
        delegate_ = delegate;
        RenderNextPDFPages();
      } else if (print_data_mime_type == "application/vnd.ms-xpsdocument") {
        bool ret = PrintXPSDocument(printer_name,
                                    job_title,
                                    print_data_file_path,
                                    print_ticket);
        if (ret)
          delegate_ = delegate;
        return ret;
      } else {
        NOTREACHED();
        return false;
      }
      return true;
    }

    void PreparePageDCForPrinting(HDC, double scale_factor) {
      SetGraphicsMode(printer_dc_.Get(), GM_ADVANCED);
      // Setup the matrix to translate and scale to the right place. Take in
      // account the scale factor.
      // Note that the printing output is relative to printable area of
      // the page. That is 0,0 is offset by PHYSICALOFFSETX/Y from the page.
      int offset_x = ::GetDeviceCaps(printer_dc_.Get(), PHYSICALOFFSETX);
      int offset_y = ::GetDeviceCaps(printer_dc_.Get(), PHYSICALOFFSETY);
      XFORM xform = {0};
      xform.eDx = static_cast<float>(-offset_x);
      xform.eDy = static_cast<float>(-offset_y);
      xform.eM11 = xform.eM22 = 1.0 / scale_factor;
      SetWorldTransform(printer_dc_.Get(), &xform);
    }

    // ServiceUtilityProcessHost::Client implementation.
    virtual void OnRenderPDFPagesToMetafileSucceeded(
        const printing::Emf& metafile,
        int highest_rendered_page_number,
        double scale_factor) OVERRIDE {
      PreparePageDCForPrinting(printer_dc_.Get(), scale_factor);
      metafile.SafePlayback(printer_dc_.Get());
      bool done_printing = (highest_rendered_page_number !=
          last_page_printed_ + kPageCountPerBatch);
      last_page_printed_ = highest_rendered_page_number;
      if (done_printing)
        PrintJobDone();
      else
        RenderNextPDFPages();
    }

    // base::win::ObjectWatcher::Delegate implementation.
    virtual void OnObjectSignaled(HANDLE object) OVERRIDE {
      DCHECK(xps_print_job_);
      DCHECK(object == job_progress_event_.Get());
      ResetEvent(job_progress_event_.Get());
      if (!delegate_)
        return;
      XPS_JOB_STATUS job_status = {0};
      xps_print_job_->GetJobStatus(&job_status);
      if ((job_status.completion == XPS_JOB_CANCELLED) ||
          (job_status.completion == XPS_JOB_FAILED)) {
        delegate_->OnJobSpoolFailed();
      } else if (job_status.jobId ||
                  (job_status.completion == XPS_JOB_COMPLETED)) {
        // Note: In the case of the XPS document being printed to the
        // Microsoft XPS Document Writer, it seems to skip spooling the job
        // and goes to the completed state without ever assigning a job id.
        delegate_->OnJobSpoolSucceeded(job_status.jobId);
      } else {
        job_progress_watcher_.StopWatching();
        job_progress_watcher_.StartWatching(job_progress_event_.Get(), this);
      }
    }

    virtual void OnRenderPDFPagesToMetafileFailed() OVERRIDE {
      PrintJobDone();
    }

    virtual void OnChildDied() OVERRIDE {
      PrintJobDone();
    }

   private:
    // Helper class to allow PrintXPSDocument() to have multiple exits.
    class PrintJobCanceler {
     public:
      explicit PrintJobCanceler(
          base::win::ScopedComPtr<IXpsPrintJob>* job_ptr)
          : job_ptr_(job_ptr) {
      }
      ~PrintJobCanceler() {
        if (job_ptr_ && *job_ptr_) {
          (*job_ptr_)->Cancel();
          job_ptr_->Release();
        }
      }

      void reset() { job_ptr_ = NULL; }

     private:
      base::win::ScopedComPtr<IXpsPrintJob>* job_ptr_;

      DISALLOW_COPY_AND_ASSIGN(PrintJobCanceler);
    };

    void PrintJobDone() {
      // If there is no delegate, then there is nothing pending to process.
      if (!delegate_)
        return;
      RestoreDC(printer_dc_.Get(), saved_dc_);
      EndDoc(printer_dc_.Get());
      if (-1 == last_page_printed_) {
        delegate_->OnJobSpoolFailed();
      } else {
        delegate_->OnJobSpoolSucceeded(job_id_);
      }
      delegate_ = NULL;
    }

    void RenderNextPDFPages() {
      printing::PageRange range;
      // Render 10 pages at a time.
      range.from = last_page_printed_ + 1;
      range.to = last_page_printed_ + kPageCountPerBatch;
      std::vector<printing::PageRange> page_ranges;
      page_ranges.push_back(range);

      int printer_dpi = ::GetDeviceCaps(printer_dc_.Get(), LOGPIXELSX);
      int dc_width = GetDeviceCaps(printer_dc_.Get(), PHYSICALWIDTH);
      int dc_height = GetDeviceCaps(printer_dc_.Get(), PHYSICALHEIGHT);
      gfx::Rect render_area(0, 0, dc_width, dc_height);
      g_service_process->io_thread()->message_loop_proxy()->PostTask(
          FROM_HERE,
          base::Bind(&JobSpoolerWin::Core::RenderPDFPagesInSandbox, this,
                      print_data_file_path_, render_area, printer_dpi,
                      page_ranges, base::MessageLoopProxy::current()));
    }

    // Called on the service process IO thread.
    void RenderPDFPagesInSandbox(
        const base::FilePath& pdf_path, const gfx::Rect& render_area,
        int render_dpi, const std::vector<printing::PageRange>& page_ranges,
        const scoped_refptr<base::MessageLoopProxy>&
            client_message_loop_proxy) {
      DCHECK(g_service_process->io_thread()->message_loop_proxy()->
          BelongsToCurrentThread());
      scoped_ptr<ServiceUtilityProcessHost> utility_host(
          new ServiceUtilityProcessHost(this, client_message_loop_proxy));
      // TODO(gene): For now we disabling autorotation for CloudPrinting.
      // Landscape/Portrait setting is passed in the print ticket and
      // server is generating portrait PDF always.
      // We should enable autorotation once server will be able to generate
      // PDF that matches paper size and orientation.
      if (utility_host->StartRenderPDFPagesToMetafile(
              pdf_path,
              printing::PdfRenderSettings(render_area, render_dpi, false),
              page_ranges)) {
        // The object will self-destruct when the child process dies.
        utility_host.release();
      }
    }

    bool PrintXPSDocument(const std::string& printer_name,
                          const std::string& job_title,
                          const base::FilePath& print_data_file_path,
                          const std::string& print_ticket) {
      if (!printing::XPSPrintModule::Init())
        return false;

      job_progress_event_.Set(CreateEvent(NULL, TRUE, FALSE, NULL));
      if (!job_progress_event_.Get())
        return false;

      PrintJobCanceler job_canceler(&xps_print_job_);
      base::win::ScopedComPtr<IXpsPrintJobStream> doc_stream;
      base::win::ScopedComPtr<IXpsPrintJobStream> print_ticket_stream;
      if (FAILED(printing::XPSPrintModule::StartXpsPrintJob(
              UTF8ToWide(printer_name).c_str(), UTF8ToWide(job_title).c_str(),
              NULL, job_progress_event_.Get(), NULL, NULL, NULL,
              xps_print_job_.Receive(), doc_stream.Receive(),
              print_ticket_stream.Receive())))
        return false;

      ULONG print_bytes_written = 0;
      if (FAILED(print_ticket_stream->Write(print_ticket.c_str(),
                                            print_ticket.length(),
                                            &print_bytes_written)))
        return false;
      DCHECK_EQ(print_ticket.length(), print_bytes_written);
      if (FAILED(print_ticket_stream->Close()))
        return false;

      std::string document_data;
      base::ReadFileToString(print_data_file_path, &document_data);
      ULONG doc_bytes_written = 0;
      if (FAILED(doc_stream->Write(document_data.c_str(),
                                    document_data.length(),
                                    &doc_bytes_written)))
        return false;
      DCHECK_EQ(document_data.length(), doc_bytes_written);
      if (FAILED(doc_stream->Close()))
        return false;

      job_progress_watcher_.StartWatching(job_progress_event_.Get(), this);
      job_canceler.reset();
      return true;
    }

    // Some Cairo-generated PDFs from Chrome OS result in huge metafiles.
    // So the PageCountPerBatch is set to 1 for now.
    // TODO(sanjeevr): Figure out a smarter way to determine the pages per
    // batch. Filed a bug to track this at
    // http://code.google.com/p/chromium/issues/detail?id=57350.
    static const int kPageCountPerBatch = 1;

    int last_page_printed_;
    PlatformJobId job_id_;
    PrintSystem::JobSpooler::Delegate* delegate_;
    int saved_dc_;
    base::win::ScopedCreateDC printer_dc_;
    base::FilePath print_data_file_path_;
    base::win::ScopedHandle job_progress_event_;
    base::win::ObjectWatcher job_progress_watcher_;
    base::win::ScopedComPtr<IXpsPrintJob> xps_print_job_;

    DISALLOW_COPY_AND_ASSIGN(Core);
  };
  scoped_refptr<Core> core_;

  DISALLOW_COPY_AND_ASSIGN(JobSpoolerWin);
};

// A helper class to handle the response from the utility process to the
// request to fetch printer capabilities and defaults.
class PrinterCapsHandler : public ServiceUtilityProcessHost::Client {
 public:
  PrinterCapsHandler(
      const std::string& printer_name,
      const PrintSystem::PrinterCapsAndDefaultsCallback& callback)
          : printer_name_(printer_name), callback_(callback) {
  }

  // ServiceUtilityProcessHost::Client implementation.
  virtual void OnChildDied() OVERRIDE {
    OnGetPrinterCapsAndDefaultsFailed(printer_name_);
  }

  virtual void OnGetPrinterCapsAndDefaultsSucceeded(
      const std::string& printer_name,
      const printing::PrinterCapsAndDefaults& caps_and_defaults) OVERRIDE {
    callback_.Run(true, printer_name, caps_and_defaults);
    callback_.Reset();
    Release();
  }

  virtual void OnGetPrinterCapsAndDefaultsFailed(
      const std::string& printer_name) OVERRIDE {
    printing::PrinterCapsAndDefaults caps_and_defaults;
    callback_.Run(false, printer_name, caps_and_defaults);
    callback_.Reset();
    Release();
  }

  void Start() {
    g_service_process->io_thread()->message_loop_proxy()->PostTask(
        FROM_HERE,
        base::Bind(&PrinterCapsHandler::GetPrinterCapsAndDefaultsImpl, this,
                    base::MessageLoopProxy::current()));
  }

 private:
    // Called on the service process IO thread.
  void GetPrinterCapsAndDefaultsImpl(
      const scoped_refptr<base::MessageLoopProxy>&
          client_message_loop_proxy) {
    DCHECK(g_service_process->io_thread()->message_loop_proxy()->
        BelongsToCurrentThread());
    scoped_ptr<ServiceUtilityProcessHost> utility_host(
        new ServiceUtilityProcessHost(this, client_message_loop_proxy));
    if (utility_host->StartGetPrinterCapsAndDefaults(printer_name_)) {
      // The object will self-destruct when the child process dies.
      utility_host.release();
    } else {
      client_message_loop_proxy->PostTask(
          FROM_HERE,
          base::Bind(&PrinterCapsHandler::OnGetPrinterCapsAndDefaultsFailed,
                      this, printer_name_));
    }
  }

  std::string printer_name_;
  PrintSystem::PrinterCapsAndDefaultsCallback callback_;
};

class PrintSystemWinXPS : public PrintSystemWin {
 public:
  PrintSystemWinXPS();
  virtual ~PrintSystemWinXPS();

  // PrintSystem implementation.
  virtual PrintSystemResult Init() OVERRIDE;
  virtual void GetPrinterCapsAndDefaults(
      const std::string& printer_name,
      const PrinterCapsAndDefaultsCallback& callback) OVERRIDE;
  virtual bool PrintSystemWinXPS::ValidatePrintTicket(
      const std::string& printer_name,
      const std::string& print_ticket_data) OVERRIDE;

  virtual PrintSystem::JobSpooler* CreateJobSpooler() OVERRIDE;
  virtual std::string GetSupportedMimeTypes() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(PrintSystemWinXPS);
};

PrintSystemWinXPS::PrintSystemWinXPS() {
}

PrintSystemWinXPS::~PrintSystemWinXPS() {
}

PrintSystem::PrintSystemResult PrintSystemWinXPS::Init() {
  if (!printing::XPSModule::Init()) {
    std::string message = l10n_util::GetStringFUTF8(
        IDS_CLOUD_PRINT_XPS_UNAVAILABLE,
        l10n_util::GetStringUTF16(IDS_GOOGLE_CLOUD_PRINT));
    return PrintSystemResult(false, message);
  }
  return PrintSystemResult(true, std::string());
}

void PrintSystemWinXPS::GetPrinterCapsAndDefaults(
    const std::string& printer_name,
    const PrinterCapsAndDefaultsCallback& callback) {
  // Launch as child process to retrieve the capabilities and defaults because
  // this involves invoking a printer driver DLL and crashes have been known to
  // occur.
  PrinterCapsHandler* handler =
      new PrinterCapsHandler(printer_name, callback);
  handler->AddRef();
  handler->Start();
}

bool PrintSystemWinXPS::ValidatePrintTicket(
    const std::string& printer_name,
    const std::string& print_ticket_data) {
  crash_keys::ScopedPrinterInfo crash_key(GetPrinterDriverInfo(printer_name));
  printing::ScopedXPSInitializer xps_initializer;
  if (!xps_initializer.initialized()) {
    // TODO(sanjeevr): Handle legacy proxy case (with no prntvpt.dll)
    return false;
  }
  bool ret = false;
  HPTPROVIDER provider = NULL;
  printing::XPSModule::OpenProvider(UTF8ToWide(printer_name.c_str()),
                                    1,
                                    &provider);
  if (provider) {
    base::win::ScopedComPtr<IStream> print_ticket_stream;
    CreateStreamOnHGlobal(NULL, TRUE, print_ticket_stream.Receive());
    ULONG bytes_written = 0;
    print_ticket_stream->Write(print_ticket_data.c_str(),
                               print_ticket_data.length(),
                               &bytes_written);
    DCHECK(bytes_written == print_ticket_data.length());
    LARGE_INTEGER pos = {0};
    ULARGE_INTEGER new_pos = {0};
    print_ticket_stream->Seek(pos, STREAM_SEEK_SET, &new_pos);
    base::win::ScopedBstr error;
    base::win::ScopedComPtr<IStream> result_ticket_stream;
    CreateStreamOnHGlobal(NULL, TRUE, result_ticket_stream.Receive());
    ret = SUCCEEDED(printing::XPSModule::MergeAndValidatePrintTicket(
        provider,
        print_ticket_stream.get(),
        NULL,
        kPTJobScope,
        result_ticket_stream.get(),
        error.Receive()));
    printing::XPSModule::CloseProvider(provider);
  }
  return ret;
}

PrintSystem::JobSpooler* PrintSystemWinXPS::CreateJobSpooler() {
  return new JobSpoolerWin();
}

std::string PrintSystemWinXPS::GetSupportedMimeTypes() {
  if (printing::XPSPrintModule::Init())
    return "application/vnd.ms-xpsdocument,application/pdf";
  return "application/pdf";
}

}  // namespace

scoped_refptr<PrintSystem> PrintSystem::CreateInstance(
    const base::DictionaryValue* print_system_settings) {
  return new PrintSystemWinXPS;
}

}  // namespace cloud_print
