// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/native_desktop_media_list.h"

#include "base/hash.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_scheduler/post_task.h"
#include "chrome/browser/media/webrtc/desktop_media_list_observer.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "media/base/video_util.h"
#include "third_party/libyuv/include/libyuv/scale_argb.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/snapshot/snapshot.h"

#if defined(OS_WIN)
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_win.h"
#endif  // defined(OS_WIN)

#if defined(USE_X11) && !defined(OS_CHROMEOS)
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_x11.h"
#endif  // defined(USE_X11) && !defined(OS_CHROMEOS)

#if defined(USE_AURA)
#include "ui/snapshot/snapshot_aura.h"
#endif

using content::BrowserThread;
using content::DesktopMediaID;

namespace {

// Update the list every second.
const int kDefaultUpdatePeriod = 1000;

// Returns a hash of a DesktopFrame content to detect when image for a desktop
// media source has changed.
uint32_t GetFrameHash(webrtc::DesktopFrame* frame) {
  int data_size = frame->stride() * frame->size().height();
  return base::Hash(reinterpret_cast<char*>(frame->data()), data_size);
}

gfx::ImageSkia ScaleDesktopFrame(std::unique_ptr<webrtc::DesktopFrame> frame,
                                 gfx::Size size) {
  gfx::Rect scaled_rect = media::ComputeLetterboxRegion(
      gfx::Rect(0, 0, size.width(), size.height()),
      gfx::Size(frame->size().width(), frame->size().height()));

  SkBitmap result;
  result.allocN32Pixels(scaled_rect.width(), scaled_rect.height(), true);

  uint8_t* pixels_data = reinterpret_cast<uint8_t*>(result.getPixels());
  libyuv::ARGBScale(frame->data(), frame->stride(),
                    frame->size().width(), frame->size().height(),
                    pixels_data, result.rowBytes(),
                    scaled_rect.width(), scaled_rect.height(),
                    libyuv::kFilterBilinear);

  // Set alpha channel values to 255 for all pixels.
  // TODO(sergeyu): Fix screen/window capturers to capture alpha channel and
  // remove this code. Currently screen/window capturers (at least some
  // implementations) only capture R, G and B channels and set Alpha to 0.
  // crbug.com/264424
  for (int y = 0; y < result.height(); ++y) {
    for (int x = 0; x < result.width(); ++x) {
      pixels_data[result.rowBytes() * y + x * result.bytesPerPixel() + 3] =
          0xff;
    }
  }

  return gfx::ImageSkia::CreateFrom1xBitmap(result);
}

}  // namespace

class NativeDesktopMediaList::Worker
    : public webrtc::DesktopCapturer::Callback {
 public:
  Worker(base::WeakPtr<NativeDesktopMediaList> media_list,
         std::unique_ptr<webrtc::DesktopCapturer> screen_capturer,
         std::unique_ptr<webrtc::DesktopCapturer> window_capturer);
  ~Worker() override;

  void Refresh(const DesktopMediaID::Id& view_dialog_id);

  void RefreshThumbnails(const std::vector<DesktopMediaID>& native_ids,
                         const gfx::Size& thumbnail_size);

 private:
  typedef std::map<DesktopMediaID, uint32_t> ImageHashesMap;

  // webrtc::DesktopCapturer::Callback interface.
  void OnCaptureResult(webrtc::DesktopCapturer::Result result,
                       std::unique_ptr<webrtc::DesktopFrame> frame) override;

  base::WeakPtr<NativeDesktopMediaList> media_list_;

  std::unique_ptr<webrtc::DesktopCapturer> screen_capturer_;
  std::unique_ptr<webrtc::DesktopCapturer> window_capturer_;

  std::unique_ptr<webrtc::DesktopFrame> current_frame_;

  ImageHashesMap image_hashes_;

  DISALLOW_COPY_AND_ASSIGN(Worker);
};

NativeDesktopMediaList::Worker::Worker(
    base::WeakPtr<NativeDesktopMediaList> media_list,
    std::unique_ptr<webrtc::DesktopCapturer> screen_capturer,
    std::unique_ptr<webrtc::DesktopCapturer> window_capturer)
    : media_list_(media_list),
      screen_capturer_(std::move(screen_capturer)),
      window_capturer_(std::move(window_capturer)) {
  if (screen_capturer_)
    screen_capturer_->Start(this);
  if (window_capturer_)
    window_capturer_->Start(this);
}

NativeDesktopMediaList::Worker::~Worker() {}

void NativeDesktopMediaList::Worker::Refresh(
    const DesktopMediaID::Id& view_dialog_id) {
  std::vector<SourceDescription> sources;

  if (screen_capturer_) {
    webrtc::DesktopCapturer::SourceList screens;
    if (screen_capturer_->GetSourceList(&screens)) {
      bool mutiple_screens = screens.size() > 1;
      base::string16 title;
      for (size_t i = 0; i < screens.size(); ++i) {
        if (mutiple_screens) {
          // Just in case 'Screen' is inflected depending on the screen number,
          // use plural formatter.
          title = l10n_util::GetPluralStringFUTF16(
              IDS_DESKTOP_MEDIA_PICKER_MULTIPLE_SCREEN_NAME,
              static_cast<int>(i + 1));
        } else {
          title = l10n_util::GetStringUTF16(
              IDS_DESKTOP_MEDIA_PICKER_SINGLE_SCREEN_NAME);
        }
        sources.push_back(SourceDescription(DesktopMediaID(
            DesktopMediaID::TYPE_SCREEN, screens[i].id), title));
      }
    }
  }

  if (window_capturer_) {
    webrtc::DesktopCapturer::SourceList windows;
    if (window_capturer_->GetSourceList(&windows)) {
      for (auto it = windows.begin(); it != windows.end(); ++it) {
        // Skip the picker dialog window.
        if (it->id == view_dialog_id)
          continue;

        DesktopMediaID media_id(DesktopMediaID::TYPE_WINDOW, it->id);
        sources.push_back(
            SourceDescription(media_id, base::UTF8ToUTF16(it->title)));
      }
    }
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&NativeDesktopMediaList::RefreshForAuraWindows,
                     media_list_, sources));
}

void NativeDesktopMediaList::Worker::RefreshThumbnails(
    const std::vector<DesktopMediaID>& native_ids,
    const gfx::Size& thumbnail_size) {
  ImageHashesMap new_image_hashes;

  // Get a thumbnail for each native source.
  for (const auto& id : native_ids) {
    switch (id.type) {
      case DesktopMediaID::TYPE_SCREEN:
        if (!screen_capturer_->SelectSource(id.id))
          continue;
        screen_capturer_->CaptureFrame();
        break;

      case DesktopMediaID::TYPE_WINDOW:
        if (!window_capturer_->SelectSource(id.id))
          continue;
        window_capturer_->CaptureFrame();
        break;

      default:
        NOTREACHED();
    }

    // Expect that DesktopCapturer to always captures frames synchronously.
    // |current_frame_| may be NULL if capture failed (e.g. because window has
    // been closed).
    if (current_frame_) {
      uint32_t frame_hash = GetFrameHash(current_frame_.get());
      new_image_hashes[id] = frame_hash;

      // Scale the image only if it has changed.
      ImageHashesMap::iterator it = image_hashes_.find(id);
      if (it == image_hashes_.end() || it->second != frame_hash) {
        gfx::ImageSkia thumbnail =
            ScaleDesktopFrame(std::move(current_frame_), thumbnail_size);
        BrowserThread::PostTask(
            BrowserThread::UI, FROM_HERE,
            base::BindOnce(&NativeDesktopMediaList::UpdateSourceThumbnail,
                           media_list_, id, thumbnail));
      }
    }
  }

  image_hashes_.swap(new_image_hashes);

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&NativeDesktopMediaList::UpdateNativeThumbnailsFinished,
                     media_list_));
}

void NativeDesktopMediaList::Worker::OnCaptureResult(
    webrtc::DesktopCapturer::Result result,
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  current_frame_ = std::move(frame);
}

NativeDesktopMediaList::NativeDesktopMediaList(
    std::unique_ptr<webrtc::DesktopCapturer> screen_capturer,
    std::unique_ptr<webrtc::DesktopCapturer> window_capturer)
    : DesktopMediaListBase(
          base::TimeDelta::FromMilliseconds(kDefaultUpdatePeriod)),
      weak_factory_(this) {
  capture_task_runner_ = base::CreateSequencedTaskRunnerWithTraits(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE});

  worker_.reset(new Worker(weak_factory_.GetWeakPtr(),
                           std::move(screen_capturer),
                           std::move(window_capturer)));
}

NativeDesktopMediaList::~NativeDesktopMediaList() {
  capture_task_runner_->DeleteSoon(FROM_HERE, worker_.release());
}

void NativeDesktopMediaList::Refresh() {
#if defined(USE_AURA)
  DCHECK_EQ(pending_aura_capture_requests_, 0);
  DCHECK(!pending_native_thumbnail_capture_);
  new_aura_thumbnail_hashes_.clear();
#endif

  capture_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Worker::Refresh, base::Unretained(worker_.get()),
                     view_dialog_id_.id));
}

void NativeDesktopMediaList::RefreshForAuraWindows(
    std::vector<SourceDescription> sources) {
#if defined(USE_AURA)
  // Associate aura id with native id.
  for (auto& source : sources) {
    if (source.id.type != DesktopMediaID::TYPE_WINDOW)
      continue;

    aura::Window* aura_window = NULL;
#if defined(OS_WIN)
    aura_window = views::DesktopWindowTreeHostWin::GetContentWindowForHWND(
        reinterpret_cast<HWND>(source.id.id));
#elif defined(USE_X11) && !defined(OS_CHROMEOS)
    aura_window =
        views::DesktopWindowTreeHostX11::GetContentWindowForXID(source.id.id);
#endif  // defined(USE_X11) && !defined(OS_CHROMEOS)
    if (aura_window) {
      DesktopMediaID aura_id = DesktopMediaID::RegisterAuraWindow(
          DesktopMediaID::TYPE_WINDOW, aura_window);
      source.id.aura_id = aura_id.aura_id;
    }
  }
#endif  // defined(USE_AURA)

  UpdateSourcesList(sources);

  // OnAuraThumbnailCaptured() and UpdateNativeThumbnailsFinished() are
  // guaranteed to be excuted after RefreshForAuraWindows() and
  // CaptureAuraWindowThumbnail() in the browser UI thread.
  // Therefore pending_aura_capture_requests_ will be set the number of aura
  // windows to be captured and pending_native_thumbnail_capture_ will be set
  // true if native thumbnail capture is needed before OnAuraThumbnailCaptured()
  // or UpdateNativeThumbnailsFinished() are called.
  std::vector<DesktopMediaID> native_ids;
  for (const auto& source : sources) {
#if defined(USE_AURA)
    if (source.id.aura_id > DesktopMediaID::kNullId) {
      CaptureAuraWindowThumbnail(source.id);
      continue;
    }
#endif  // defined(USE_AURA)
    native_ids.push_back(source.id);
  }

  if (native_ids.size() > 0) {
#if defined(USE_AURA)
    pending_native_thumbnail_capture_ = true;
#endif
    capture_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Worker::RefreshThumbnails,
                                  base::Unretained(worker_.get()), native_ids,
                                  thumbnail_size_));
  }
}

void NativeDesktopMediaList::UpdateNativeThumbnailsFinished() {
#if defined(USE_AURA)
  DCHECK(pending_native_thumbnail_capture_);
  pending_native_thumbnail_capture_ = false;
  // Schedule next refresh if native thumbnail captures finished after aura
  // thumbnail captures.
  if (pending_aura_capture_requests_ == 0)
    ScheduleNextRefresh();
#else
  ScheduleNextRefresh();
#endif  // defined(USE_AURA)
}

#if defined(USE_AURA)

void NativeDesktopMediaList::CaptureAuraWindowThumbnail(
    const DesktopMediaID& id) {
  gfx::NativeWindow window = DesktopMediaID::GetAuraWindowById(id);
  if (!window)
    return;

  gfx::Rect window_rect(window->bounds().width(), window->bounds().height());
  gfx::Rect scaled_rect = media::ComputeLetterboxRegion(
      gfx::Rect(thumbnail_size_), window_rect.size());

  pending_aura_capture_requests_++;
  ui::GrabWindowSnapshotAndScaleAsyncAura(
      window, window_rect, scaled_rect.size(),
      base::CreateTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE}),
      base::Bind(&NativeDesktopMediaList::OnAuraThumbnailCaptured,
                 weak_factory_.GetWeakPtr(), id));
}

void NativeDesktopMediaList::OnAuraThumbnailCaptured(const DesktopMediaID& id,
                                                     const gfx::Image& image) {
  if (!image.IsEmpty()) {
    // Only new or changed thumbnail need update.
    new_aura_thumbnail_hashes_[id] = GetImageHash(image);
    if (!previous_aura_thumbnail_hashes_.count(id) ||
        previous_aura_thumbnail_hashes_[id] != new_aura_thumbnail_hashes_[id]) {
      UpdateSourceThumbnail(id, image.AsImageSkia());
    }
  }

  // After all aura windows are processed, schedule next refresh;
  pending_aura_capture_requests_--;
  DCHECK_GE(pending_aura_capture_requests_, 0);
  if (pending_aura_capture_requests_ == 0) {
    previous_aura_thumbnail_hashes_ = std::move(new_aura_thumbnail_hashes_);
    // Schedule next refresh if aura thumbnail captures finished after native
    // thumbnail captures.
    if (!pending_native_thumbnail_capture_)
      ScheduleNextRefresh();
  }
}

#endif  // defined(USE_AURA)
