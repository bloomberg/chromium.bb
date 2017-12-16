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
#include "chrome/services/printing/public/interfaces/constants.mojom.h"
#include "chrome/services/printing/public/interfaces/pdf_to_pwg_raster_converter.mojom.h"
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
class PwgRasterConverterHelper
    : public base::RefCountedThreadSafe<PwgRasterConverterHelper> {
 public:
  PwgRasterConverterHelper(const PdfRenderSettings& settings,
                           const PwgRasterSettings& bitmap_settings);

  void Convert(base::RefCountedMemory* data,
               PwgRasterConverter::ResultCallback callback);

 private:
  friend class base::RefCountedThreadSafe<PwgRasterConverterHelper>;

  ~PwgRasterConverterHelper();

  void RunCallback(bool success);

  void OnFilesReadyOnUIThread();

  PdfRenderSettings settings_;
  PwgRasterSettings bitmap_settings_;
  mojo::InterfacePtr<printing::mojom::PdfToPwgRasterConverter>
      pdf_to_pwg_raster_converter_ptr_;
  PwgRasterConverter::ResultCallback callback_;
  const scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  std::unique_ptr<FileHandlers, base::OnTaskRunnerDeleter> files_;

  DISALLOW_COPY_AND_ASSIGN(PwgRasterConverterHelper);
};

PwgRasterConverterHelper::PwgRasterConverterHelper(
    const PdfRenderSettings& settings,
    const PwgRasterSettings& bitmap_settings)
    : settings_(settings),
      bitmap_settings_(bitmap_settings),
      blocking_task_runner_(base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      files_(nullptr, base::OnTaskRunnerDeleter(blocking_task_runner_)) {}

PwgRasterConverterHelper::~PwgRasterConverterHelper() {}

void PwgRasterConverterHelper::Convert(
    base::RefCountedMemory* data,
    PwgRasterConverter::ResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  callback_ = std::move(callback);
  CHECK(!files_);
  files_.reset(new FileHandlers());

  blocking_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&FileHandlers::Init, base::Unretained(files_.get()),
                     base::RetainedRef(data)),
      base::BindOnce(&PwgRasterConverterHelper::OnFilesReadyOnUIThread, this));
}

void PwgRasterConverterHelper::OnFilesReadyOnUIThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!files_->IsValid()) {
    RunCallback(false);
    return;
  }

  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(printing::mojom::kChromePrintingServiceName,
                      &pdf_to_pwg_raster_converter_ptr_);

  pdf_to_pwg_raster_converter_ptr_.set_connection_error_handler(
      base::Bind(&PwgRasterConverterHelper::RunCallback, this, false));

  pdf_to_pwg_raster_converter_ptr_->Convert(
      mojo::WrapPlatformFile(files_->GetPdfForProcess()), settings_,
      bitmap_settings_, mojo::WrapPlatformFile(files_->GetPwgForProcess()),
      base::Bind(&PwgRasterConverterHelper::RunCallback, this));
}

void PwgRasterConverterHelper::RunCallback(bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (callback_)
    std::move(callback_).Run(success, files_->GetPwgPath());
}

class PwgRasterConverterImpl : public PwgRasterConverter {
 public:
  PwgRasterConverterImpl();
  ~PwgRasterConverterImpl() override;

  void Start(base::RefCountedMemory* data,
             const PdfRenderSettings& conversion_settings,
             const PwgRasterSettings& bitmap_settings,
             ResultCallback callback) override;

 private:
  // TODO (rbpotter): Once CancelableOnceCallback is added, remove this and
  // change callback_ to a CancelableOnceCallback.
  void RunCallback(bool success, const base::FilePath& temp_file);

  scoped_refptr<PwgRasterConverterHelper> utility_client_;
  ResultCallback callback_;
  base::WeakPtrFactory<PwgRasterConverterImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PwgRasterConverterImpl);
};

PwgRasterConverterImpl::PwgRasterConverterImpl() : weak_ptr_factory_(this) {}

PwgRasterConverterImpl::~PwgRasterConverterImpl() {}

void PwgRasterConverterImpl::Start(base::RefCountedMemory* data,
                                   const PdfRenderSettings& conversion_settings,
                                   const PwgRasterSettings& bitmap_settings,
                                   ResultCallback callback) {
  // Bind callback here and pass a wrapper to the utility client to avoid
  // calling callback if PwgRasterConverterImpl is destroyed.
  callback_ = std::move(callback);
  utility_client_ = base::MakeRefCounted<PwgRasterConverterHelper>(
      conversion_settings, bitmap_settings);
  utility_client_->Convert(data,
                           base::BindOnce(&PwgRasterConverterImpl::RunCallback,
                                          weak_ptr_factory_.GetWeakPtr()));
}

void PwgRasterConverterImpl::RunCallback(bool success,
                                         const base::FilePath& temp_file) {
  std::move(callback_).Run(success, temp_file);
}

}  // namespace

// static
std::unique_ptr<PwgRasterConverter> PwgRasterConverter::CreateDefault() {
  return base::MakeUnique<PwgRasterConverterImpl>();
}

// static
PdfRenderSettings PwgRasterConverter::GetConversionSettings(
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
PwgRasterSettings PwgRasterConverter::GetBitmapSettings(
    const cloud_devices::CloudDeviceDescription& printer_capabilities,
    const cloud_devices::CloudDeviceDescription& ticket) {
  cloud_devices::printer::DuplexTicketItem duplex_item;
  cloud_devices::printer::DuplexType duplex_value =
      cloud_devices::printer::NO_DUPLEX;
  if (duplex_item.LoadFrom(ticket))
    duplex_value = duplex_item.value();

  // This assumes |ticket| contains a color ticket item. In case it does not, or
  // the color is invalid, |color_value| will default to AUTO_COLOR, which works
  // just fine. With AUTO_COLOR, it may be possible to better determine the
  // value for |use_color| based on |printer_capabilities|, rather than just
  // defaulting to the safe value of true. Parsing |printer_capabilities|
  // requires work, which this method is avoiding on purpose.
  cloud_devices::printer::Color color_value;
  cloud_devices::printer::ColorTicketItem color_item;
  if (color_item.LoadFrom(ticket) && color_item.IsValid())
    color_value = color_item.value();
  DCHECK(color_value.IsValid());
  bool use_color;
  switch (color_value.type) {
    case cloud_devices::printer::STANDARD_MONOCHROME:
    case cloud_devices::printer::CUSTOM_MONOCHROME:
      use_color = false;
      break;

    case cloud_devices::printer::STANDARD_COLOR:
    case cloud_devices::printer::CUSTOM_COLOR:
    case cloud_devices::printer::AUTO_COLOR:
      use_color = true;
      break;

    default:
      NOTREACHED();
      use_color = true;  // Still need to initialize |color| or MSVC will warn.
      break;
  }

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

  // No need to check for SRGB_8 support in |types|. CDD spec says:
  // "any printer that doesn't support SGRAY_8 must be able to perform
  // conversion from RGB to grayscale... "
  const auto& types = raster_capability.value().document_types_supported;
  result.use_color =
      use_color || !base::ContainsValue(types, cloud_devices::printer::SGRAY_8);

  return result;
}

}  // namespace printing
