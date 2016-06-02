// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/arc/arc_app_icon.h"

#include <algorithm>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/task_runner_util.h"
#include "chrome/browser/image_decoder.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_source.h"

namespace {

bool disable_safe_decoding = false;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ArcAppIcon::ReadResult

struct ArcAppIcon::ReadResult {
  enum class Status {
    OK,
    FAIL,
    REQUEST_TO_INSTALL,
  };

  // Used for fail results or for the requests to install an icon.
  ReadResult(Status status, ui::ScaleFactor scale_factor)
      : status(status), scale_factor(scale_factor) {
    DCHECK(status == Status::FAIL || status == Status::REQUEST_TO_INSTALL);
  }

  // Used for successful results.
  ReadResult(ui::ScaleFactor scale_factor,
             const std::string& unsafe_icon_data)
      : status(Status::OK),
        scale_factor(scale_factor),
        unsafe_icon_data(unsafe_icon_data) {
  }

  Status status;
  ui::ScaleFactor scale_factor;
  std::string unsafe_icon_data;
};

////////////////////////////////////////////////////////////////////////////////
// ArcAppIcon::Source

class ArcAppIcon::Source : public gfx::ImageSkiaSource {
 public:
  Source(const base::WeakPtr<ArcAppIcon>& host, int resource_size_in_dip);
  ~Source() override;

 private:
  // gfx::ImageSkiaSource overrides:
  gfx::ImageSkiaRep GetImageForScale(float scale) override;

  // Used to load images asynchronously. NULLed out when the ArcAppIcon is
  // destroyed.
  base::WeakPtr<ArcAppIcon> host_;

  const int resource_size_in_dip_;

  DISALLOW_COPY_AND_ASSIGN(Source);
};

ArcAppIcon::Source::Source(const base::WeakPtr<ArcAppIcon>& host,
                           int resource_size_in_dip)
    : host_(host),
      resource_size_in_dip_(resource_size_in_dip) {
}

ArcAppIcon::Source::~Source() {
}

gfx::ImageSkiaRep ArcAppIcon::Source::GetImageForScale(float scale) {
  if (host_)
    host_->LoadForScaleFactor(ui::GetSupportedScaleFactor(scale));

  // Host loads icon asynchronously, so use default icon so far.
  const gfx::ImageSkia* default_image = ResourceBundle::GetSharedInstance().
      GetImageSkiaNamed(IDR_APP_DEFAULT_ICON);
  CHECK(default_image);
  return gfx::ImageSkiaOperations::CreateResizedImage(
      *default_image,
      skia::ImageOperations::RESIZE_BEST,
      gfx::Size(resource_size_in_dip_, resource_size_in_dip_)).
          GetRepresentation(scale);
}

class ArcAppIcon::DecodeRequest : public ImageDecoder::ImageRequest {
 public:
  DecodeRequest(const base::WeakPtr<ArcAppIcon>& host,
                int dimension,
                ui::ScaleFactor scale_factor);
  ~DecodeRequest() override;

  // ImageDecoder::ImageRequest
  void OnImageDecoded(const SkBitmap& bitmap) override;
  void OnDecodeImageFailed() override;
 private:
  base::WeakPtr<ArcAppIcon> host_;
  int dimension_;
  ui::ScaleFactor scale_factor_;

  DISALLOW_COPY_AND_ASSIGN(DecodeRequest);
};

////////////////////////////////////////////////////////////////////////////////
// ArcAppIcon::DecodeRequest

ArcAppIcon::DecodeRequest::DecodeRequest(const base::WeakPtr<ArcAppIcon>& host,
                                         int dimension,
                                         ui::ScaleFactor scale_factor)
    : host_(host),
      dimension_(dimension),
      scale_factor_(scale_factor) {
}

ArcAppIcon::DecodeRequest::~DecodeRequest() {
}

void ArcAppIcon::DecodeRequest::OnImageDecoded(const SkBitmap& bitmap) {
  DCHECK(!bitmap.isNull() && !bitmap.empty());

  if (!host_)
    return;

  int expected_dim = static_cast<int>(
      ui::GetScaleForScaleFactor(scale_factor_) * dimension_ + 0.5f);
  if (bitmap.width() != expected_dim || bitmap.height() != expected_dim) {
    VLOG(2) << "Decoded ARC icon has unexpected dimension "
            << bitmap.width() << "x" << bitmap.height() << ". Expected "
            << expected_dim << "x" << ".";
    host_->DiscardDecodeRequest(this);
    return;
  }

  gfx::ImageSkia image_skia;
  image_skia.AddRepresentation(gfx::ImageSkiaRep(
      bitmap,
      ui::GetScaleForScaleFactor(scale_factor_)));

  host_->Update(&image_skia);
  host_->DiscardDecodeRequest(this);
}

void ArcAppIcon::DecodeRequest::OnDecodeImageFailed() {
  VLOG(2) << "Failed to decode ARC icon.";

  if (!host_)
    return;

  host_->DiscardDecodeRequest(this);
}

////////////////////////////////////////////////////////////////////////////////
// ArcAppIcon

// static
void ArcAppIcon::DisableSafeDecodingForTesting() {
  disable_safe_decoding = true;
}

ArcAppIcon::ArcAppIcon(content::BrowserContext* context,
                       const std::string& app_id,
                       int resource_size_in_dip,
                       Observer* observer)
    : context_(context),
      app_id_(app_id),
      resource_size_in_dip_(resource_size_in_dip),
      observer_(observer),
      weak_ptr_factory_(this) {
  CHECK(observer_ != nullptr);
  source_ = new Source(weak_ptr_factory_.GetWeakPtr(), resource_size_in_dip);
  gfx::Size resource_size(resource_size_in_dip, resource_size_in_dip);
  image_skia_ = gfx::ImageSkia(source_, resource_size);
}

ArcAppIcon::~ArcAppIcon() {
}

void ArcAppIcon::LoadForScaleFactor(ui::ScaleFactor scale_factor) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(context_);
  DCHECK(prefs);

  base::FilePath path = prefs->GetIconPath(app_id_, scale_factor);
  if (path.empty())
    return;

  base::PostTaskAndReplyWithResult(content::BrowserThread::GetBlockingPool(),
                                   FROM_HERE,
                                   base::Bind(&ArcAppIcon::ReadOnFileThread,
                                              scale_factor,
                                              path),
                                   base::Bind(&ArcAppIcon::OnIconRead,
                                              weak_ptr_factory_.GetWeakPtr()));
}

void ArcAppIcon::RequestIcon(ui::ScaleFactor scale_factor) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(context_);
  DCHECK(prefs);

  // ArcAppListPrefs notifies ArcAppModelBuilder via Observer when icon is ready
  // and ArcAppModelBuilder refreshes the icon of the corresponding item by
  // calling LoadScaleFactor.
  prefs->RequestIcon(app_id_, scale_factor);
}

// static
std::unique_ptr<ArcAppIcon::ReadResult> ArcAppIcon::ReadOnFileThread(
    ui::ScaleFactor scale_factor,
    const base::FilePath& path) {
  DCHECK(content::BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());
  DCHECK(!path.empty());

  if (!base::PathExists(path))
    return base::WrapUnique(new ArcAppIcon::ReadResult(
        ArcAppIcon::ReadResult::Status::REQUEST_TO_INSTALL, scale_factor));

  // Read the file from disk.
  std::string unsafe_icon_data;
  if (!base::ReadFileToString(path, &unsafe_icon_data)) {
    VLOG(2) << "Failed to read an ARC icon from file " << path.MaybeAsASCII();
    return base::WrapUnique(new ArcAppIcon::ReadResult(
        ArcAppIcon::ReadResult::Status::FAIL, scale_factor));
  }

  return base::WrapUnique(
      new ArcAppIcon::ReadResult(scale_factor, unsafe_icon_data));
}

void ArcAppIcon::OnIconRead(
    std::unique_ptr<ArcAppIcon::ReadResult> read_result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  switch (read_result->status) {
  case ReadResult::Status::OK:
    decode_requests_.push_back(
        new DecodeRequest(weak_ptr_factory_.GetWeakPtr(),
                          resource_size_in_dip_,
                          read_result->scale_factor));
    if (disable_safe_decoding) {
      SkBitmap bitmap;
      if (!read_result->unsafe_icon_data.empty() &&
          gfx::PNGCodec::Decode(
              reinterpret_cast<const unsigned char*>(
                  &read_result->unsafe_icon_data.front()),
              read_result->unsafe_icon_data.length(),
              &bitmap)) {
        decode_requests_.back()->OnImageDecoded(bitmap);
      } else {
        decode_requests_.back()->OnDecodeImageFailed();
     }
    } else {
      ImageDecoder::Start(decode_requests_.back(),
                          read_result->unsafe_icon_data);
    }
    break;
  case ReadResult::Status::FAIL:
    break;
  case ReadResult::Status::REQUEST_TO_INSTALL:
    RequestIcon(read_result->scale_factor);
    break;
  default:
    NOTREACHED();
  }
}

void ArcAppIcon::Update(const gfx::ImageSkia* image) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(image && !image->isNull());

  std::vector<gfx::ImageSkiaRep> reps = image->image_reps();
  for (const auto& image_rep : reps) {
    if (ui::IsSupportedScale(image_rep.scale())) {
      image_skia_.RemoveRepresentation(image_rep.scale());
      image_skia_.AddRepresentation(image_rep);
    }
  }

  image_ = gfx::Image(image_skia_);

  observer_->OnIconUpdated(this);
}

void ArcAppIcon::DiscardDecodeRequest(DecodeRequest* request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ScopedVector<DecodeRequest>::iterator it = std::find(decode_requests_.begin(),
                                                       decode_requests_.end(),
                                                       request);
  CHECK(it != decode_requests_.end());
  decode_requests_.erase(it);
}
