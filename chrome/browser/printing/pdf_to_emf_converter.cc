// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/pdf_to_emf_converter.h"

#include "base/bind_helpers.h"
#include "base/cancelable_callback.h"
#include "base/file_util.h"
#include "base/files/file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "chrome/common/chrome_utility_messages.h"
#include "chrome/common/chrome_utility_printing_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/utility_process_host.h"
#include "content/public/browser/utility_process_host_client.h"
#include "printing/page_range.h"
#include "printing/pdf_render_settings.h"

namespace printing {

namespace {

using content::BrowserThread;

class FileHandlers {
 public:
  FileHandlers() {}

  ~FileHandlers() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  }

  void Init(base::RefCountedMemory* data);
  bool IsValid();

  base::FilePath GetEmfPath() const {
    return temp_dir_.path().AppendASCII("output.emf");
  }

  base::FilePath GetPdfPath() const {
    return temp_dir_.path().AppendASCII("input.pdf");
  }

  IPC::PlatformFileForTransit GetPdfForProcess(base::ProcessHandle process) {
    DCHECK(pdf_file_.IsValid());
    IPC::PlatformFileForTransit transit =
        IPC::TakeFileHandleForProcess(pdf_file_.Pass(), process);
    return transit;
  }

  const base::FilePath& GetBasePath() const {
    return temp_dir_.path();
  }

 private:
  base::ScopedTempDir temp_dir_;
  base::File pdf_file_;
};

void FileHandlers::Init(base::RefCountedMemory* data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  if (!temp_dir_.CreateUniqueTempDir()) {
    return;
  }

  if (static_cast<int>(data->size()) !=
      base::WriteFile(GetPdfPath(), data->front_as<char>(), data->size())) {
    return;
  }

  // Reopen in read only mode.
  pdf_file_.Initialize(GetPdfPath(),
                       base::File::FLAG_OPEN | base::File::FLAG_READ);
}

bool FileHandlers::IsValid() {
  return pdf_file_.IsValid();
}

// Converts PDF into EMF.
// Class uses 3 threads: UI, IO and FILE.
// Internal workflow is following:
// 1. Create instance on the UI thread. (files_, settings_,)
// 2. Create file on the FILE thread.
// 3. Start utility process and start conversion on the IO thread.
// 4. Run result callback on the UI thread.
// 5. Instance is destroyed from any thread that has the last reference.
// 6. FileHandlers destroyed on the FILE thread.
//    This step posts |FileHandlers| to be destroyed on the FILE thread.
// All these steps work sequentially, so no data should be accessed
// simultaneously by several threads.
class PdfToEmfUtilityProcessHostClient
    : public content::UtilityProcessHostClient {
 public:
  explicit PdfToEmfUtilityProcessHostClient(
      const printing::PdfRenderSettings& settings);

  void Convert(base::RefCountedMemory* data,
               const PdfToEmfConverter::ResultCallback& callback);

  // UtilityProcessHostClient implementation.
  virtual void OnProcessCrashed(int exit_code) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  virtual ~PdfToEmfUtilityProcessHostClient();

  // Message handlers.
  void OnProcessStarted();
  void OnSucceeded(const std::vector<printing::PageRange>& page_ranges,
                   double scale_factor);
  void OnFailed();

  void RunCallback(const std::vector<printing::PageRange>& page_ranges,
                   double scale_factor);

  void StartProcessOnIOThread();

  void RunCallbackOnUIThread(
      const std::vector<printing::PageRange>& page_ranges,
      double scale_factor);
  void OnFilesReadyOnUIThread();

  scoped_ptr<FileHandlers, BrowserThread::DeleteOnFileThread> files_;
  printing::PdfRenderSettings settings_;
  PdfToEmfConverter::ResultCallback callback_;
  base::WeakPtr<content::UtilityProcessHost> utility_process_host_;

  DISALLOW_COPY_AND_ASSIGN(PdfToEmfUtilityProcessHostClient);
};

PdfToEmfUtilityProcessHostClient::PdfToEmfUtilityProcessHostClient(
    const printing::PdfRenderSettings& settings)
    : settings_(settings) {}

PdfToEmfUtilityProcessHostClient::~PdfToEmfUtilityProcessHostClient() {
}

void PdfToEmfUtilityProcessHostClient::Convert(
    base::RefCountedMemory* data,
    const PdfToEmfConverter::ResultCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  callback_ = callback;
  CHECK(!files_);
  files_.reset(new FileHandlers());
  BrowserThread::PostTaskAndReply(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&FileHandlers::Init,
                 base::Unretained(files_.get()),
                 make_scoped_refptr(data)),
      base::Bind(&PdfToEmfUtilityProcessHostClient::OnFilesReadyOnUIThread,
                 this));
}

void PdfToEmfUtilityProcessHostClient::OnProcessCrashed(int exit_code) {
  OnFailed();
}

bool PdfToEmfUtilityProcessHostClient::OnMessageReceived(
  const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PdfToEmfUtilityProcessHostClient, message)
    IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_ProcessStarted, OnProcessStarted)
    IPC_MESSAGE_HANDLER(
        ChromeUtilityHostMsg_RenderPDFPagesToMetafiles_Succeeded, OnSucceeded)
    IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_RenderPDFPagesToMetafile_Failed,
                        OnFailed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PdfToEmfUtilityProcessHostClient::OnProcessStarted() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!utility_process_host_) {
    RunCallbackOnUIThread(std::vector<printing::PageRange>(), 0.0);
    return;
  }

  base::ProcessHandle process = utility_process_host_->GetData().handle;
  utility_process_host_->Send(
      new ChromeUtilityMsg_RenderPDFPagesToMetafiles(
          files_->GetPdfForProcess(process),
          files_->GetEmfPath(),
          settings_,
          std::vector<printing::PageRange>()));
  utility_process_host_.reset();
}

void PdfToEmfUtilityProcessHostClient::OnSucceeded(
    const std::vector<printing::PageRange>& page_ranges,
    double scale_factor) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  RunCallback(page_ranges, scale_factor);
}

void PdfToEmfUtilityProcessHostClient::OnFailed() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  RunCallback(std::vector<printing::PageRange>(), 0.0);
}

void PdfToEmfUtilityProcessHostClient::OnFilesReadyOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!files_->IsValid()) {
    RunCallbackOnUIThread(std::vector<printing::PageRange>(), 0.0);
    return;
  }
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&PdfToEmfUtilityProcessHostClient::StartProcessOnIOThread,
                 this));
}

void PdfToEmfUtilityProcessHostClient::StartProcessOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  utility_process_host_ =
      content::UtilityProcessHost::Create(
          this,
          base::MessageLoop::current()->message_loop_proxy())->AsWeakPtr();
  // NOTE: This process _must_ be sandboxed, otherwise the pdf dll will load
  // gdiplus.dll, change how rendering happens, and not be able to correctly
  // generate when sent to a metafile DC.
  utility_process_host_->SetExposedDir(files_->GetBasePath());
  utility_process_host_->Send(new ChromeUtilityMsg_StartupPing);
}

void PdfToEmfUtilityProcessHostClient::RunCallback(
    const std::vector<printing::PageRange>& page_ranges,
    double scale_factor) {
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&PdfToEmfUtilityProcessHostClient::RunCallbackOnUIThread,
                 this,
                 page_ranges,
                 scale_factor));
}

void PdfToEmfUtilityProcessHostClient::RunCallbackOnUIThread(
    const std::vector<printing::PageRange>& page_ranges,
    double scale_factor) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::vector<base::FilePath> page_filenames;
  std::vector<printing::PageRange>::const_iterator iter;
  for (iter = page_ranges.begin(); iter != page_ranges.end(); ++iter) {
    for (int page_number = iter->from; page_number <= iter->to; ++page_number) {
      page_filenames.push_back(files_->GetEmfPath().InsertBeforeExtensionASCII(
          base::StringPrintf(".%d", page_number)));
    }
  }
  if (!callback_.is_null()) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(callback_, scale_factor, page_filenames));
    callback_.Reset();
  }
}

class PdfToEmfConverterImpl : public PdfToEmfConverter {
 public:
  PdfToEmfConverterImpl();

  virtual ~PdfToEmfConverterImpl();

  virtual void Start(base::RefCountedMemory* data,
                     const printing::PdfRenderSettings& conversion_settings,
                     const ResultCallback& callback) OVERRIDE;

 private:
  scoped_refptr<PdfToEmfUtilityProcessHostClient> utility_client_;
  base::CancelableCallback<ResultCallback::RunType> callback_;

  DISALLOW_COPY_AND_ASSIGN(PdfToEmfConverterImpl);
};

PdfToEmfConverterImpl::PdfToEmfConverterImpl() {
}

PdfToEmfConverterImpl::~PdfToEmfConverterImpl() {
}

void PdfToEmfConverterImpl::Start(
    base::RefCountedMemory* data,
    const printing::PdfRenderSettings& conversion_settings,
    const ResultCallback& callback) {
  // Rebind cancelable callback to avoid calling callback if
  // PdfToEmfConverterImpl is destroyed.
  callback_.Reset(callback);
  utility_client_ = new PdfToEmfUtilityProcessHostClient(conversion_settings);
  utility_client_->Convert(data, callback_.callback());
}

}  // namespace

// static
scoped_ptr<PdfToEmfConverter> PdfToEmfConverter::CreateDefault() {
  return scoped_ptr<PdfToEmfConverter>(new PdfToEmfConverterImpl());
}

}  // namespace printing
