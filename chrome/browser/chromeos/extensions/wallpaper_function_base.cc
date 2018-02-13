// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/wallpaper_function_base.h"

#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/synchronization/cancellation_flag.h"
#include "base/task_scheduler/lazy_task_runner.h"
#include "base/task_scheduler/task_traits.h"
#include "chrome/browser/image_decoder.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/login/login_state.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;

namespace wallpaper_api_util {
namespace {

// Keeps in sync (same order) with WallpaperLayout enum in header file.
const char* const kWallpaperLayoutArrays[] = {
  "CENTER",
  "CENTER_CROPPED",
  "STRETCH",
  "TILE"
};

const int kWallpaperLayoutCount = arraysize(kWallpaperLayoutArrays);

base::LazySequencedTaskRunner g_blocking_task_runner =
    LAZY_SEQUENCED_TASK_RUNNER_INITIALIZER(
        base::TaskTraits(base::MayBlock(),
                         base::TaskPriority::USER_BLOCKING,
                         base::TaskShutdownBehavior::BLOCK_SHUTDOWN));
base::LazySequencedTaskRunner g_non_blocking_task_runner =
    LAZY_SEQUENCED_TASK_RUNNER_INITIALIZER(
        base::TaskTraits(base::MayBlock(),
                         base::TaskPriority::USER_VISIBLE,
                         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN));

}  // namespace

const char kCancelWallpaperMessage[] = "Set wallpaper was canceled.";

wallpaper::WallpaperLayout GetLayoutEnum(const std::string& layout) {
  for (int i = 0; i < kWallpaperLayoutCount; i++) {
    if (layout.compare(kWallpaperLayoutArrays[i]) == 0)
      return static_cast<wallpaper::WallpaperLayout>(i);
  }
  // Default to use CENTER layout.
  return wallpaper::WALLPAPER_LAYOUT_CENTER;
}

void RecordCustomWallpaperLayout(const wallpaper::WallpaperLayout& layout) {
  UMA_HISTOGRAM_ENUMERATION("Ash.Wallpaper.CustomLayout", layout,
                            wallpaper::NUM_WALLPAPER_LAYOUT);
}

}  // namespace wallpaper_api_util

class WallpaperFunctionBase::UnsafeWallpaperDecoder
    : public ImageDecoder::ImageRequest {
 public:
  explicit UnsafeWallpaperDecoder(scoped_refptr<WallpaperFunctionBase> function)
      : function_(function) {}

  void Start(const std::vector<char>& image_data) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    // This function can only be called after user login. It is fine to use
    // unsafe image decoder here. Before user login, a robust jpeg decoder will
    // be used.
    CHECK(chromeos::LoginState::Get()->IsUserLoggedIn());
    std::string image_data_str(image_data.begin(), image_data.end());
    ImageDecoder::StartWithOptions(this, image_data_str,
                                   ImageDecoder::DEFAULT_CODEC, true);
  }

  void Cancel() {
    cancel_flag_.Set();
  }

  void OnImageDecoded(const SkBitmap& decoded_image) override {
    // Make the SkBitmap immutable as we won't modify it. This is important
    // because otherwise it gets duplicated during painting, wasting memory.
    SkBitmap immutable(decoded_image);
    immutable.setImmutable();
    gfx::ImageSkia final_image = gfx::ImageSkia::CreateFrom1xBitmap(immutable);
    final_image.MakeThreadSafe();
    if (cancel_flag_.IsSet()) {
      function_->OnCancel();
      delete this;
      return;
    }
    function_->OnWallpaperDecoded(final_image);
    delete this;
  }

  void OnDecodeImageFailed() override {
    function_->OnFailure(
        l10n_util::GetStringUTF8(IDS_WALLPAPER_MANAGER_INVALID_WALLPAPER));
    delete this;
  }

 private:
  scoped_refptr<WallpaperFunctionBase> function_;
  base::CancellationFlag cancel_flag_;

  DISALLOW_COPY_AND_ASSIGN(UnsafeWallpaperDecoder);
};

WallpaperFunctionBase::UnsafeWallpaperDecoder*
    WallpaperFunctionBase::unsafe_wallpaper_decoder_;

WallpaperFunctionBase::WallpaperFunctionBase() {
}

WallpaperFunctionBase::~WallpaperFunctionBase() {
}

base::SequencedTaskRunner* WallpaperFunctionBase::GetBlockingTaskRunner() {
  return wallpaper_api_util::g_blocking_task_runner.Get().get();
}

base::SequencedTaskRunner* WallpaperFunctionBase::GetNonBlockingTaskRunner() {
  return wallpaper_api_util::g_non_blocking_task_runner.Get().get();
}

void WallpaperFunctionBase::AssertCalledOnWallpaperSequence(
    base::SequencedTaskRunner* task_runner) {
#if DCHECK_IS_ON()
  DCHECK(task_runner->RunsTasksInCurrentSequence());
#endif
}

void WallpaperFunctionBase::StartDecode(const std::vector<char>& data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (unsafe_wallpaper_decoder_)
    unsafe_wallpaper_decoder_->Cancel();
  unsafe_wallpaper_decoder_ = new UnsafeWallpaperDecoder(this);
  unsafe_wallpaper_decoder_->Start(data);
}

void WallpaperFunctionBase::OnCancel() {
  unsafe_wallpaper_decoder_ = NULL;
  SetError(wallpaper_api_util::kCancelWallpaperMessage);
  SendResponse(false);
}

void WallpaperFunctionBase::OnFailure(const std::string& error) {
  unsafe_wallpaper_decoder_ = NULL;
  SetError(error);
  SendResponse(false);
}
