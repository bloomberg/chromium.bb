// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/pwg_raster_converter.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind_helpers.h"
#include "base/cancelable_callback.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/sequenced_task_runner.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/common/chrome_utility_printing_messages.h"
#include "chrome/common/printing/pdf_to_pwg_raster_converter.mojom.h"
#include "chrome/grit/generated_resources.h"
#include "components/cloud_devices/common/cloud_device_description.h"
#include "components/cloud_devices/common/printer_description.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/common/service_manager_connection.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "printing/pdf_render_settings.h"
#include "printing/pwg_raster_settings.h"
#include "printing/units.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace printing {

namespace {

using content::BrowserThread;

class FileHandlers {
 public:
  FileHandlers() {}

  ~FileHandlers() { base::AssertBlockingAllowed(); }

  void Init(base::RefCountedMemory* data);
  bool IsValid();

  base::FilePath GetPwgPath() const {
    return temp_dir_.GetPath().AppendASCII("output.pwg");
  }

  base::FilePath GetPdfPath() const {
    return temp_dir_.GetPath().AppendASCII("input.pdf");
  }

  base::PlatformFile GetPdfForProcess() {
    DCHECK(pdf_file_.IsValid());
    return pdf_file_.TakePlatformFile();
  }

  base::PlatformFile GetPwgForProcess() {
    DCHECK(pwg_file_.IsValid());
    return pwg_file_.TakePlatformFile();
  }

 private:
  base::ScopedTempDir temp_dir_;
  base::File pdf_file_;
  base::File pwg_file_;

  DISALLOW_COPY_AND_ASSIGN(FileHandlers);
};

void FileHandlers::Init(base::RefCountedMemory* data) {
  base::AssertBlockingAllowed();

  if (!temp_dir_.CreateUniqueTempDir())
    return;

  if (static_cast<int>(data->size()) !=
      base::WriteFile(GetPdfPath(), data->front_as<char>(), data->size())) {
    return;
  }

  // Reopen in read only mode.
  pdf_file_.Initialize(GetPdfPath(),
                       base::File::FLAG_OPEN | base::File::FLAG_READ);
  pwg_file_.Initialize(GetPwgPath(),
                       base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
}

bool FileHandlers::IsValid() {
  return pdf_file_.IsValid() && pwg_file_.IsValid();
}

// Converts PDF into PWG raster.
// Class uses UI thread and |blocking_task_runner_|.
// Internal workflow is following:
// 1. Create instance on the UI thread. (files_, settings_,)
// 2. Create file on |blocking_task_runner_|.
// 3. Connect to the printing utility service and start the conversion.
// 4. Run result callback on the UI thread.
// 5. Instance is destroyed from any thread that has the last reference.
// 6. FileHandlers destroyed on |blocking_task_runner_|.
//    This step posts |FileHandlers| to be destroyed on |blocking_task_runner_|.
// All these steps work sequentially, so no data should be accessed
// simultaneously by several threads.
class PWGRasterConverterHelper
    : public base::RefCountedThreadSafe<PWGRasterConverterHelper> {
 public:
  PWGRasterConverterHelper(const PdfRenderSettings& settings,
                           const PwgRasterSettings& bitmap_settings);

  void Convert(base::RefCountedMemory* data,
               const PWGRasterConverter::ResultCallback& callback);

 private:
  friend class base::RefCountedThreadSafe<PWGRasterConverterHelper>;

  ~PWGRasterConverterHelper();

  void RunCallback(bool success);

  void OnFilesReadyOnUIThread();

  PdfRenderSettings settings_;
  PwgRasterSettings bitmap_settings_;
  mojo::InterfacePtr<printing::mojom::PDFToPWGRasterConverter>
      pdf_to_pwg_raster_converter_ptr_;
  PWGRasterConverter::ResultCallback callback_;
  const scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  std::unique_ptr<FileHandlers, base::OnTaskRunnerDeleter> files_;

  DISALLOW_COPY_AND_ASSIGN(PWGRasterConverterHelper);
};

PWGRasterConverterHelper::PWGRasterConverterHelper(
    const PdfRenderSettings& settings,
    const PwgRasterSettings& bitmap_settings)
    : settings_(settings),
      bitmap_settings_(bitmap_settings),
      blocking_task_runner_(base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      files_(nullptr, base::OnTaskRunnerDeleter(blocking_task_runner_)) {}

PWGRasterConverterHelper::~PWGRasterConverterHelper() {}

void PWGRasterConverterHelper::Convert(
    base::RefCountedMemory* data,
    const PWGRasterConverter::ResultCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  callback_ = callback;
  CHECK(!files_);
  files_.reset(new FileHandlers());

  blocking_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&FileHandlers::Init, base::Unretained(files_.get()),
                     base::RetainedRef(data)),
      base::BindOnce(&PWGRasterConverterHelper::OnFilesReadyOnUIThread, this));
}

void PWGRasterConverterHelper::OnFilesReadyOnUIThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!files_->IsValid()) {
    RunCallback(false);
    return;
  }

  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(printing::mojom::kPdfToPwgRasterConverterServiceName,
                      &pdf_to_pwg_raster_converter_ptr_);

  pdf_to_pwg_raster_converter_ptr_.set_connection_error_handler(
      base::Bind(&PWGRasterConverterHelper::RunCallback, this, false));

  pdf_to_pwg_raster_converter_ptr_->Convert(
      mojo::WrapPlatformFile(files_->GetPdfForProcess()), settings_,
      bitmap_settings_, mojo::WrapPlatformFile(files_->GetPwgForProcess()),
      base::Bind(&PWGRasterConverterHelper::RunCallback, this));
}

void PWGRasterConverterHelper::RunCallback(bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (callback_)
    std::move(callback_).Run(success, files_->GetPwgPath());
}

class PWGRasterConverterImpl : public PWGRasterConverter {
 public:
  PWGRasterConverterImpl();
  ~PWGRasterConverterImpl() override;

  void Start(base::RefCountedMemory* data,
             const PdfRenderSettings& conversion_settings,
             const PwgRasterSettings& bitmap_settings,
             const ResultCallback& callback) override;

 private:
  scoped_refptr<PWGRasterConverterHelper> utility_client_;
  base::CancelableCallback<ResultCallback::RunType> callback_;

  DISALLOW_COPY_AND_ASSIGN(PWGRasterConverterImpl);
};

PWGRasterConverterImpl::PWGRasterConverterImpl() {
}

PWGRasterConverterImpl::~PWGRasterConverterImpl() {
}

void PWGRasterConverterImpl::Start(base::RefCountedMemory* data,
                                   const PdfRenderSettings& conversion_settings,
                                   const PwgRasterSettings& bitmap_settings,
                                   const ResultCallback& callback) {
  // Rebind cancelable callback to avoid calling callback if
  // PWGRasterConverterImpl is destroyed.
  callback_.Reset(callback);
  utility_client_ = base::MakeRefCounted<PWGRasterConverterHelper>(
      conversion_settings, bitmap_settings);
  utility_client_->Convert(data, callback_.callback());
}

}  // namespace

// static
std::unique_ptr<PWGRasterConverter> PWGRasterConverter::CreateDefault() {
  return base::MakeUnique<PWGRasterConverterImpl>();
}

// static
PdfRenderSettings PWGRasterConverter::GetConversionSettings(
    const cloud_devices::CloudDeviceDescription& printer_capabilities,
    const gfx::Size& page_size) {
  int dpi = kDefaultPdfDpi;
  cloud_devices::printer::DpiCapability dpis;
  if (dpis.LoadFrom(printer_capabilities))
    dpi = std::max(dpis.GetDefault().horizontal, dpis.GetDefault().vertical);

  const double scale = static_cast<double>(dpi) / kPointsPerInch;

  // Make vertical rectangle to optimize streaming to printer. Fix orientation
  // by autorotate.
  gfx::Rect area(std::min(page_size.width(), page_size.height()) * scale,
                 std::max(page_size.width(), page_size.height()) * scale);
  return PdfRenderSettings(area, gfx::Point(0, 0), dpi, /*autorotate=*/true,
                           PdfRenderSettings::Mode::NORMAL);
}

// static
PwgRasterSettings PWGRasterConverter::GetBitmapSettings(
    const cloud_devices::CloudDeviceDescription& printer_capabilities,
    const cloud_devices::CloudDeviceDescription& ticket) {
  cloud_devices::printer::DuplexTicketItem duplex_item;
  cloud_devices::printer::DuplexType duplex_value =
      cloud_devices::printer::NO_DUPLEX;
  if (duplex_item.LoadFrom(ticket))
    duplex_value = duplex_item.value();

  cloud_devices::printer::PwgRasterConfigCapability raster_capability;
  // If the raster capability fails to load, |raster_capability| will contain
  // the default value.
  raster_capability.LoadFrom(printer_capabilities);
  cloud_devices::printer::DocumentSheetBack document_sheet_back =
      raster_capability.value().document_sheet_back;

  PwgRasterSettings result;
  result.odd_page_transform = TRANSFORM_NORMAL;
  switch (duplex_value) {
    case cloud_devices::printer::NO_DUPLEX:
      break;
    case cloud_devices::printer::LONG_EDGE:
      if (document_sheet_back == cloud_devices::printer::ROTATED)
        result.odd_page_transform = TRANSFORM_ROTATE_180;
      else if (document_sheet_back == cloud_devices::printer::FLIPPED)
        result.odd_page_transform = TRANSFORM_FLIP_VERTICAL;
      break;
    case cloud_devices::printer::SHORT_EDGE:
      if (document_sheet_back == cloud_devices::printer::MANUAL_TUMBLE)
        result.odd_page_transform = TRANSFORM_ROTATE_180;
      else if (document_sheet_back == cloud_devices::printer::FLIPPED)
        result.odd_page_transform = TRANSFORM_FLIP_HORIZONTAL;
      break;
  }

  result.rotate_all_pages = raster_capability.value().rotate_all_pages;
  result.reverse_page_order = raster_capability.value().reverse_order_streaming;
  return result;
}

}  // namespace printing
