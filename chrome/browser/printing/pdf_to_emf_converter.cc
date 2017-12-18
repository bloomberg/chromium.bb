// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/pdf_to_emf_converter.h"

#include <stdint.h>
#include <windows.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/sequenced_task_runner.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/services/printing/public/interfaces/constants.mojom.h"
#include "chrome/services/printing/public/interfaces/pdf_to_emf_converter.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/common/service_manager_connection.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "printing/emf_win.h"
#include "printing/pdf_render_settings.h"
#include "services/service_manager/public/cpp/connector.h"

using content::BrowserThread;

namespace printing {

namespace {

void CloseFileOnBlockingTaskRunner(base::File temp_file) {
  base::AssertBlockingAllowed();
  temp_file.Close();
}

// Allows to delete temporary directory after all temporary files created inside
// are closed. Windows cannot delete directory with opened files. Directory is
// used to store PDF and metafiles. PDF should be gone by the time utility
// process exits. Metafiles should be gone when all LazyEmf destroyed.
class RefCountedTempDir
    : public base::RefCountedDeleteOnSequence<RefCountedTempDir> {
 public:
  RefCountedTempDir()
      : base::RefCountedDeleteOnSequence<RefCountedTempDir>(
            base::SequencedTaskRunnerHandle::Get()) {
    ignore_result(temp_dir_.CreateUniqueTempDir());
  }

  bool IsValid() const { return temp_dir_.IsValid(); }
  const base::FilePath& GetPath() const { return temp_dir_.GetPath(); }

 private:
  friend class base::RefCountedDeleteOnSequence<RefCountedTempDir>;
  friend class base::DeleteHelper<RefCountedTempDir>;

  ~RefCountedTempDir() {}

  base::ScopedTempDir temp_dir_;
  DISALLOW_COPY_AND_ASSIGN(RefCountedTempDir);
};

class TempFile {
 public:
  explicit TempFile(base::File file)
      : file_(std::move(file)),
        blocking_task_runner_(base::SequencedTaskRunnerHandle::Get()) {
    base::AssertBlockingAllowed();
  }
  ~TempFile() {
    blocking_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&CloseFileOnBlockingTaskRunner,
                                  base::Passed(std::move(file_))));
  }

  base::File& file() { return file_; }

 private:
  base::File file_;
  const scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(TempFile);
};

class PdfToEmfConverterClientImpl : public mojom::PdfToEmfConverterClient {
 public:
  explicit PdfToEmfConverterClientImpl(
      mojom::PdfToEmfConverterClientRequest request)
      : binding_(this, std::move(request)) {}

 private:
  // mojom::PdfToEmfConverterClient implementation.
  void PreCacheFontCharacters(
      const std::vector<uint8_t>& logfont_data,
      const base::string16& characters,
      PreCacheFontCharactersCallback callback) override {
    // TODO(scottmg): pdf/ppapi still require the renderer to be able to
    // precache GDI fonts (http://crbug.com/383227), even when using
    // DirectWrite. Eventually this shouldn't be added and should be moved to
    // FontCacheDispatcher too. http://crbug.com/356346.

    // First, comments from FontCacheDispatcher::OnPreCacheFont do apply here
    // too. Except that for True Type fonts, GetTextMetrics will not load the
    // font in memory. The only way windows seem to load properly, it is to
    // create a similar device (like the one in which we print), then do an
    // ExtTextOut, as we do in the printing thread, which is sandboxed.
    const LOGFONT* logfont =
        reinterpret_cast<const LOGFONT*>(&logfont_data.at(0));

    HDC hdc = CreateEnhMetaFile(nullptr, nullptr, nullptr, nullptr);
    HFONT font_handle = CreateFontIndirect(logfont);
    DCHECK(font_handle != nullptr);

    HGDIOBJ old_font = SelectObject(hdc, font_handle);
    DCHECK(old_font != nullptr);

    ExtTextOut(hdc, 0, 0, ETO_GLYPH_INDEX, 0, characters.c_str(),
               characters.length(), nullptr);

    SelectObject(hdc, old_font);
    DeleteObject(font_handle);

    HENHMETAFILE metafile = CloseEnhMetaFile(hdc);

    if (metafile)
      DeleteEnhMetaFile(metafile);

    std::move(callback).Run();
  }

  mojo::Binding<mojom::PdfToEmfConverterClient> binding_;
};

using ScopedTempFile = std::unique_ptr<TempFile>;

// Wrapper for Emf to keep only file handle in memory, and load actual data only
// on playback. Emf::InitFromFile() can play metafile directly from disk, but it
// can't open file handles. We need file handles to reliably delete temporary
// files, and to efficiently interact with utility process.
class LazyEmf : public MetafilePlayer {
 public:
  LazyEmf(const scoped_refptr<RefCountedTempDir>& temp_dir, ScopedTempFile file)
      : temp_dir_(temp_dir), file_(std::move(file)) {
    CHECK(file_);
  }
  ~LazyEmf() override { Close(); }

 protected:
  // MetafilePlayer:
  bool SafePlayback(HDC hdc) const override;

  void Close() const;
  bool LoadEmf(Emf* emf) const;

 private:
  mutable scoped_refptr<RefCountedTempDir> temp_dir_;
  mutable ScopedTempFile file_;  // Mutable because of consts in base class.

  bool GetDataAsVector(std::vector<char>* buffer) const override;
  bool SaveTo(base::File* file) const override;

  DISALLOW_COPY_AND_ASSIGN(LazyEmf);
};

// Postscript metafile subclass to override SafePlayback.
class PostScriptMetaFile : public LazyEmf {
 public:
  PostScriptMetaFile(const scoped_refptr<RefCountedTempDir>& temp_dir,
                     ScopedTempFile file)
      : LazyEmf(temp_dir, std::move(file)) {}
  ~PostScriptMetaFile() override;

 private:
  // MetafilePlayer:
  bool SafePlayback(HDC hdc) const override;

  DISALLOW_COPY_AND_ASSIGN(PostScriptMetaFile);
};

// Class for converting PDF to another format for printing (Emf, Postscript).
// Class uses UI thread and |blocking_task_runner_|.
// Internal workflow is following:
// 1. Create instance on the UI thread. (files_, settings_,)
// 2. Create pdf file on |blocking_task_runner_|.
// 3. Bind to printing service and start conversion on the UI thread (mojo
//    actually makes that happen transparently on the IO thread).
// 4. Printing service returns page count.
// 5. For each page:
//   1. Clients requests page with file handle to a temp file.
//   2. Utility converts the page, save it to the file and reply.
//
// All these steps work sequentially, so no data should be accessed
// simultaneously by several threads.
class PdfConverterImpl : public PdfConverter {
 public:
  PdfConverterImpl(const scoped_refptr<base::RefCountedMemory>& data,
                   const PdfRenderSettings& conversion_settings,
                   StartCallback start_callback);
  ~PdfConverterImpl() override;

 private:
  class GetPageCallbackData {
   public:
    GetPageCallbackData(int page_number, PdfConverter::GetPageCallback callback)
        : page_number_(page_number), callback_(callback) {}

    GetPageCallbackData(GetPageCallbackData&& other) {
      *this = std::move(other);
    }

    GetPageCallbackData& operator=(GetPageCallbackData&& rhs) {
      page_number_ = rhs.page_number_;
      callback_ = rhs.callback_;
      file_ = std::move(rhs.file_);
      return *this;
    }

    int page_number() const { return page_number_; }

    const PdfConverter::GetPageCallback& callback() const { return callback_; }

    ScopedTempFile TakeFile() { return std::move(file_); }

    void set_file(ScopedTempFile file) { file_ = std::move(file); }

   private:
    int page_number_;

    PdfConverter::GetPageCallback callback_;
    ScopedTempFile file_;

    DISALLOW_COPY_AND_ASSIGN(GetPageCallbackData);
  };

  void GetPage(int page_number,
               const PdfConverter::GetPageCallback& get_page_callback) override;

  void Stop();

  // Helper functions: must be overridden by subclasses
  // Create a metafileplayer subclass file from a temporary file.
  std::unique_ptr<MetafilePlayer> GetFileFromTemp(ScopedTempFile temp_file);

  void OnPageCount(mojom::PdfToEmfConverterPtr converter, uint32_t page_count);
  void OnPageDone(bool success, float scale_factor);

  void OnFailed(const std::string& error_message);
  void OnTempPdfReady(ScopedTempFile pdf);
  void OnTempFileReady(GetPageCallbackData* callback_data,
                       ScopedTempFile temp_file);

  // Additional message handler needed for Pdf to Emf
  void OnPreCacheFontCharacters(const LOGFONT& log_font,
                                const base::string16& characters);

  scoped_refptr<RefCountedTempDir> temp_dir_;

  PdfRenderSettings settings_;

  // Document loaded callback.
  PdfConverter::StartCallback start_callback_;

  // Queue of callbacks for GetPage() requests. Utility process should reply
  // with PageDone in the same order as requests were received.
  // Use containers that keeps element pointers valid after push() and pop().
  using GetPageCallbacks = base::queue<GetPageCallbackData>;
  GetPageCallbacks get_page_callbacks_;

  const scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  std::unique_ptr<PdfToEmfConverterClientImpl>
      pdf_to_emf_converter_client_impl_;

  mojom::PdfToEmfConverterPtr pdf_to_emf_converter_;

  mojom::PdfToEmfConverterFactoryPtr pdf_to_emf_converter_factory_;

  base::WeakPtrFactory<PdfConverterImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PdfConverterImpl);
};

std::unique_ptr<MetafilePlayer> PdfConverterImpl::GetFileFromTemp(
    ScopedTempFile temp_file) {
  if (settings_.mode == PdfRenderSettings::Mode::POSTSCRIPT_LEVEL2 ||
      settings_.mode == PdfRenderSettings::Mode::POSTSCRIPT_LEVEL3 ||
      settings_.mode == PdfRenderSettings::Mode::TEXTONLY) {
    return std::make_unique<PostScriptMetaFile>(temp_dir_,
                                                std::move(temp_file));
  }
  return std::make_unique<LazyEmf>(temp_dir_, std::move(temp_file));
}

ScopedTempFile CreateTempFile(scoped_refptr<RefCountedTempDir>* temp_dir) {
  if (!temp_dir->get())
    *temp_dir = base::MakeRefCounted<RefCountedTempDir>();
  ScopedTempFile file;
  if (!(*temp_dir)->IsValid())
    return file;
  base::FilePath path;
  if (!base::CreateTemporaryFileInDir((*temp_dir)->GetPath(), &path)) {
    PLOG(ERROR) << "Failed to create file in "
                << (*temp_dir)->GetPath().value();
    return file;
  }
  file = std::make_unique<TempFile>(base::File(
      path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE |
                base::File::FLAG_READ | base::File::FLAG_DELETE_ON_CLOSE |
                base::File::FLAG_TEMPORARY));
  if (!file->file().IsValid()) {
    PLOG(ERROR) << "Failed to create " << path.value();
    file.reset();
  }
  return file;
}

ScopedTempFile CreateTempPdfFile(
    const scoped_refptr<base::RefCountedMemory>& data,
    scoped_refptr<RefCountedTempDir>* temp_dir) {
  ScopedTempFile pdf_file = CreateTempFile(temp_dir);
  if (!pdf_file || static_cast<int>(data->size()) !=
                       pdf_file->file().WriteAtCurrentPos(
                           data->front_as<char>(), data->size())) {
    pdf_file.reset();
    return pdf_file;
  }
  pdf_file->file().Seek(base::File::FROM_BEGIN, 0);
  return pdf_file;
}

bool LazyEmf::SafePlayback(HDC hdc) const {
  Emf emf;
  bool result = LoadEmf(&emf) && emf.SafePlayback(hdc);
  // TODO(thestig): Fix destruction of metafiles. For some reasons
  // instances of Emf are not deleted. https://crbug.com/260806
  // It's known that the Emf going to be played just once to a printer. So just
  // release |file_| here.
  Close();
  return result;
}

bool LazyEmf::GetDataAsVector(std::vector<char>* buffer) const {
  NOTREACHED();
  return false;
}

bool LazyEmf::SaveTo(base::File* file) const {
  Emf emf;
  return LoadEmf(&emf) && emf.SaveTo(file);
}

void LazyEmf::Close() const {
  file_.reset();
  temp_dir_ = nullptr;
}

bool LazyEmf::LoadEmf(Emf* emf) const {
  file_->file().Seek(base::File::FROM_BEGIN, 0);
  int64_t size = file_->file().GetLength();
  if (size <= 0)
    return false;
  std::vector<char> data(size);
  if (file_->file().ReadAtCurrentPos(data.data(), data.size()) != size)
    return false;
  return emf->InitFromData(data.data(), data.size());
}

PostScriptMetaFile::~PostScriptMetaFile() {
}

bool PostScriptMetaFile::SafePlayback(HDC hdc) const {
  // TODO(thestig): Fix destruction of metafiles. For some reasons
  // instances of Emf are not deleted. https://crbug.com/260806
  // It's known that the Emf going to be played just once to a printer. So just
  // release |file_| before returning.
  Emf emf;
  if (!LoadEmf(&emf)) {
    Close();
    return false;
  }

  {
    // Ensure enumerator destruction before calling Close() below.
    Emf::Enumerator emf_enum(emf, nullptr, nullptr);
    for (const Emf::Record& record : emf_enum) {
      auto* emf_record = record.record();
      if (emf_record->iType != EMR_GDICOMMENT)
        continue;

      const EMRGDICOMMENT* comment =
          reinterpret_cast<const EMRGDICOMMENT*>(emf_record);
      const char* data = reinterpret_cast<const char*>(comment->Data);
      const uint16_t* ptr = reinterpret_cast<const uint16_t*>(data);
      int ret = ExtEscape(hdc, PASSTHROUGH, 2 + *ptr, data, 0, nullptr);
      DCHECK_EQ(*ptr, ret);
    }
  }
  Close();
  return true;
}

PdfConverterImpl::PdfConverterImpl(
    const scoped_refptr<base::RefCountedMemory>& data,
    const PdfRenderSettings& settings,
    StartCallback start_callback)
    : settings_(settings),
      start_callback_(std::move(start_callback)),
      blocking_task_runner_(base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(start_callback_);

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CreateTempPdfFile, data, &temp_dir_),
      base::BindOnce(&PdfConverterImpl::OnTempPdfReady,
                     weak_ptr_factory_.GetWeakPtr()));
}

PdfConverterImpl::~PdfConverterImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void PdfConverterImpl::OnTempPdfReady(ScopedTempFile pdf) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!pdf)
    return OnFailed(std::string("Failed to create temporary PDF file."));

  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(printing::mojom::kChromePrintingServiceName,
                      &pdf_to_emf_converter_factory_);
  pdf_to_emf_converter_factory_.set_connection_error_handler(base::BindOnce(
      &PdfConverterImpl::OnFailed, weak_ptr_factory_.GetWeakPtr(),
      std::string("Connection to PdfToEmfConverterFactory error.")));

  mojom::PdfToEmfConverterClientPtr pdf_to_emf_converter_client_ptr;
  pdf_to_emf_converter_client_impl_ =
      std::make_unique<PdfToEmfConverterClientImpl>(
          mojo::MakeRequest(&pdf_to_emf_converter_client_ptr));

  pdf_to_emf_converter_factory_->CreateConverter(
      mojo::WrapPlatformFile(pdf->file().TakePlatformFile()), settings_,
      std::move(pdf_to_emf_converter_client_ptr),
      base::BindOnce(&PdfConverterImpl::OnPageCount,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PdfConverterImpl::OnPageCount(mojom::PdfToEmfConverterPtr converter,
                                   uint32_t page_count) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!pdf_to_emf_converter_.is_bound());
  pdf_to_emf_converter_ = std::move(converter);
  pdf_to_emf_converter_.set_connection_error_handler(base::BindOnce(
      &PdfConverterImpl::OnFailed, weak_ptr_factory_.GetWeakPtr(),
      std::string("Connection to PdfToEmfConverter error.")));
  std::move(start_callback_).Run(page_count);
}

void PdfConverterImpl::GetPage(
    int page_number,
    const PdfConverter::GetPageCallback& get_page_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Store callback before any OnFailed() call to make it called on failure.
  get_page_callbacks_.push(GetPageCallbackData(page_number, get_page_callback));

  if (!pdf_to_emf_converter_)
    return OnFailed(std::string("No PdfToEmfConverter."));

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CreateTempFile, &temp_dir_),
      base::BindOnce(&PdfConverterImpl::OnTempFileReady,
                     weak_ptr_factory_.GetWeakPtr(),
                     &get_page_callbacks_.back()));
}

void PdfConverterImpl::OnTempFileReady(GetPageCallbackData* callback_data,
                                       ScopedTempFile temp_file) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(pdf_to_emf_converter_.is_bound());

  if (!pdf_to_emf_converter_ || !temp_file)
    return OnFailed(std::string("Error connecting to printing service."));

  // We need to dup the file as mojo::WrapPlatformFile takes ownership of the
  // passed file.
  base::File temp_file_copy = temp_file->file().Duplicate();
  pdf_to_emf_converter_->ConvertPage(
      callback_data->page_number(),
      mojo::WrapPlatformFile(temp_file_copy.TakePlatformFile()),
      base::BindOnce(&PdfConverterImpl::OnPageDone,
                     weak_ptr_factory_.GetWeakPtr()));
  callback_data->set_file(std::move(temp_file));
}

void PdfConverterImpl::OnPageDone(bool success, float scale_factor) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (get_page_callbacks_.empty())
    return OnFailed(std::string("No get_page callbacks."));
  GetPageCallbackData& data = get_page_callbacks_.front();
  std::unique_ptr<MetafilePlayer> file;

  if (success) {
    ScopedTempFile temp_file = data.TakeFile();
    if (!temp_file)  // Unexpected message from printing service.
      return OnFailed("No temp file.");
    file = GetFileFromTemp(std::move(temp_file));
  }

  base::WeakPtr<PdfConverterImpl> weak_this = weak_ptr_factory_.GetWeakPtr();
  data.callback().Run(data.page_number(), scale_factor, std::move(file));
  // WARNING: the callback might have deleted |this|!
  if (!weak_this)
    return;
  get_page_callbacks_.pop();
}

void PdfConverterImpl::Stop() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Disconnect interface ptrs so that the printing service process stop.
  pdf_to_emf_converter_factory_.reset();
  pdf_to_emf_converter_.reset();
}

void PdfConverterImpl::OnFailed(const std::string& error_message) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  LOG(ERROR) << "Failed to convert PDF: " << error_message;
  base::WeakPtr<PdfConverterImpl> weak_this = weak_ptr_factory_.GetWeakPtr();
  if (!start_callback_.is_null()) {
    OnPageCount(mojom::PdfToEmfConverterPtr(), 0);
    if (!weak_this)
      return;  // Protect against the |start_callback_| deleting |this|.
  }

  while (!get_page_callbacks_.empty()) {
    OnPageDone(false, 0.0f);
    if (!weak_this) {
      // OnPageDone invokes the GetPageCallback which might end up deleting
      // this.
      return;
    }
  }

  Stop();
}

void PdfConverterImpl::OnPreCacheFontCharacters(const LOGFONT& font,
                                                const base::string16& str) {
  // TODO(scottmg): pdf/ppapi still require the renderer to be able to precache
  // GDI fonts (http://crbug.com/383227), even when using DirectWrite.
  // Eventually this shouldn't be added and should be moved to
  // FontCacheDispatcher too. http://crbug.com/356346.

  // First, comments from FontCacheDispatcher::OnPreCacheFont do apply here too.
  // Except that for True Type fonts,
  // GetTextMetrics will not load the font in memory.
  // The only way windows seem to load properly, it is to create a similar
  // device (like the one in which we print), then do an ExtTextOut,
  // as we do in the printing thread, which is sandboxed.
  HDC hdc = CreateEnhMetaFile(nullptr, nullptr, nullptr, nullptr);
  HFONT font_handle = CreateFontIndirect(&font);
  DCHECK(font_handle != nullptr);

  HGDIOBJ old_font = SelectObject(hdc, font_handle);
  DCHECK(old_font != nullptr);

  ExtTextOut(hdc, 0, 0, ETO_GLYPH_INDEX, 0, str.c_str(), str.length(), nullptr);

  SelectObject(hdc, old_font);
  DeleteObject(font_handle);

  HENHMETAFILE metafile = CloseEnhMetaFile(hdc);

  if (metafile)
    DeleteEnhMetaFile(metafile);
}

}  // namespace

PdfConverter::~PdfConverter() = default;

// static
std::unique_ptr<PdfConverter> PdfConverter::StartPdfConverter(
    const scoped_refptr<base::RefCountedMemory>& data,
    const PdfRenderSettings& conversion_settings,
    StartCallback start_callback) {
  return std::make_unique<PdfConverterImpl>(data, conversion_settings,
                                            std::move(start_callback));
}

}  // namespace printing
