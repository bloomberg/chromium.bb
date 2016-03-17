// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/native_desktop_media_list.h"

#include "base/hash.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/media/desktop_media_list_observer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "media/base/video_util.h"
#include "third_party/libyuv/include/libyuv/scale_argb.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/screen_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/window_capturer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/snapshot/snapshot.h"

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

gfx::ImageSkia ScaleDesktopFrame(scoped_ptr<webrtc::DesktopFrame> frame,
                                 gfx::Size size) {
  gfx::Rect scaled_rect = media::ComputeLetterboxRegion(
      gfx::Rect(0, 0, size.width(), size.height()),
      gfx::Size(frame->size().width(), frame->size().height()));

  SkBitmap result;
  result.allocN32Pixels(scaled_rect.width(), scaled_rect.height(), true);
  result.lockPixels();

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

  result.unlockPixels();

  return gfx::ImageSkia::CreateFrom1xBitmap(result);
}

#if defined(USE_AURA)

NativeDesktopMediaList::NativeAuraIdMap GetBrowserNativeAuraIdMap() {
  NativeDesktopMediaList::NativeAuraIdMap id_map;
  for (auto* browser : *BrowserList::GetInstance()) {
    aura::Window* aura_window = browser->window()->GetNativeWindow();
    if (!aura_window)
      continue;
    aura::WindowTreeHost* host = aura_window->GetHost();
    if (!host)
      continue;
    gfx::AcceleratedWidget widget = host->GetAcceleratedWidget();
#if defined(OS_WIN)
    DesktopMediaID::Id native_id = reinterpret_cast<DesktopMediaID::Id>(widget);
#else
    DesktopMediaID::Id native_id = widget;
#endif
    DesktopMediaID media_id = DesktopMediaID::RegisterAuraWindow(
        DesktopMediaID::TYPE_WINDOW, aura_window);
    id_map[native_id] = media_id.aura_id;
  }

  return id_map;
}

#endif  // defined(USE_AURA)

}  // namespace

class NativeDesktopMediaList::Worker
    : public webrtc::DesktopCapturer::Callback {
 public:
  Worker(base::WeakPtr<NativeDesktopMediaList> media_list,
         scoped_ptr<webrtc::ScreenCapturer> screen_capturer,
         scoped_ptr<webrtc::WindowCapturer> window_capturer);
  ~Worker() override;

  void Refresh(const gfx::Size& thumbnail_size,
               const DesktopMediaID::Id& view_dialog_id,
               const NativeAuraIdMap& native_aura_id_map);

 private:
  typedef std::map<DesktopMediaID, uint32_t> ImageHashesMap;

  // webrtc::DesktopCapturer::Callback interface.
  void OnCaptureCompleted(webrtc::DesktopFrame* frame) override;

  base::WeakPtr<NativeDesktopMediaList> media_list_;

  scoped_ptr<webrtc::ScreenCapturer> screen_capturer_;
  scoped_ptr<webrtc::WindowCapturer> window_capturer_;

  scoped_ptr<webrtc::DesktopFrame> current_frame_;

  ImageHashesMap image_hashes_;

  DISALLOW_COPY_AND_ASSIGN(Worker);
};

NativeDesktopMediaList::Worker::Worker(
    base::WeakPtr<NativeDesktopMediaList> media_list,
    scoped_ptr<webrtc::ScreenCapturer> screen_capturer,
    scoped_ptr<webrtc::WindowCapturer> window_capturer)
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
    const gfx::Size& thumbnail_size,
    const DesktopMediaID::Id& view_dialog_id,
    const NativeAuraIdMap& native_aura_id_map) {
  std::vector<SourceDescription> sources;
  std::vector<DesktopMediaID> aura_media_ids;

  if (screen_capturer_) {
    webrtc::ScreenCapturer::ScreenList screens;
    if (screen_capturer_->GetScreenList(&screens)) {
      bool mutiple_screens = screens.size() > 1;
      base::string16 title;
      for (size_t i = 0; i < screens.size(); ++i) {
        if (mutiple_screens) {
          title = l10n_util::GetStringFUTF16Int(
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
    webrtc::WindowCapturer::WindowList windows;
    if (window_capturer_->GetWindowList(&windows)) {
      for (webrtc::WindowCapturer::WindowList::iterator it = windows.begin();
           it != windows.end(); ++it) {
        // Skip the picker dialog window.
        if (it->id != view_dialog_id) {
          DesktopMediaID media_id(DesktopMediaID::TYPE_WINDOW, it->id);
#if defined(USE_AURA)
          // Associate aura id with native id.
          auto aura_id = native_aura_id_map.find(media_id.id);
          if (aura_id != native_aura_id_map.end()) {
            media_id.aura_id = aura_id->second;
            aura_media_ids.push_back(media_id);
          }
#endif
          sources.push_back(
              SourceDescription(media_id, base::UTF8ToUTF16(it->title)));
        }
      }
    }
  }

  // Update list of windows before updating thumbnails.
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&NativeDesktopMediaList::UpdateSourcesList,
                                     media_list_, sources));

  ImageHashesMap new_image_hashes;

  // Get a thumbnail for each source.
  for (size_t i = 0; i < sources.size(); ++i) {
    SourceDescription& source = sources[i];

    switch (source.id.type) {
      case DesktopMediaID::TYPE_SCREEN:
        if (!screen_capturer_->SelectScreen(source.id.id))
          continue;
        screen_capturer_->Capture(webrtc::DesktopRegion());
        break;

      case DesktopMediaID::TYPE_WINDOW:
#if defined(USE_AURA)
        // Aura window thumbmail capture is skipped here. It will be done
        // asynchronously in the UI thread.
        if (source.id.aura_id > DesktopMediaID::kNullId)
          continue;
#endif
        if (!window_capturer_->SelectWindow(source.id.id))
          continue;
        window_capturer_->Capture(webrtc::DesktopRegion());
        break;

      default:
        NOTREACHED();
    }

    // Expect that DesktopCapturer to always captures frames synchronously.
    // |current_frame_| may be NULL if capture failed (e.g. because window has
    // been closed).
    if (current_frame_) {
      uint32_t frame_hash = GetFrameHash(current_frame_.get());
      new_image_hashes[source.id] = frame_hash;

      // Scale the image only if it has changed.
      ImageHashesMap::iterator it = image_hashes_.find(source.id);
      if (it == image_hashes_.end() || it->second != frame_hash) {
        gfx::ImageSkia thumbnail =
            ScaleDesktopFrame(std::move(current_frame_), thumbnail_size);
        BrowserThread::PostTask(
            BrowserThread::UI, FROM_HERE,
            base::Bind(&NativeDesktopMediaList::OnSourceThumbnailCaptured,
                       media_list_, i, thumbnail));
      }
    }
  }

  image_hashes_.swap(new_image_hashes);

  // Aura thumbnail captures have to be done in UI thread. After they are done,
  // a refresh will be scheduled.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &NativeDesktopMediaList::FinishRefreshOnUiThreadAndScheduleNext,
          media_list_, aura_media_ids));
}

void NativeDesktopMediaList::Worker::OnCaptureCompleted(
    webrtc::DesktopFrame* frame) {
  current_frame_.reset(frame);
}

NativeDesktopMediaList::NativeDesktopMediaList(
    scoped_ptr<webrtc::ScreenCapturer> screen_capturer,
    scoped_ptr<webrtc::WindowCapturer> window_capturer)
    : DesktopMediaListBase(
          base::TimeDelta::FromMilliseconds(kDefaultUpdatePeriod)),
      weak_factory_(this) {
  base::SequencedWorkerPool* worker_pool = BrowserThread::GetBlockingPool();
  capture_task_runner_ = worker_pool->GetSequencedTaskRunner(
      worker_pool->GetSequenceToken());

  worker_.reset(new Worker(weak_factory_.GetWeakPtr(),
                           std::move(screen_capturer),
                           std::move(window_capturer)));
}

NativeDesktopMediaList::~NativeDesktopMediaList() {
  capture_task_runner_->DeleteSoon(FROM_HERE, worker_.release());
}

void NativeDesktopMediaList::Refresh() {
  NativeAuraIdMap native_aura_id_map;
#if defined(USE_AURA)
  native_aura_id_map = GetBrowserNativeAuraIdMap();
  pending_aura_capture_requests_ = 0;
  new_aura_thumbnail_hashes_.clear();
#endif

  capture_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&Worker::Refresh, base::Unretained(worker_.get()),
                 thumbnail_size_, view_dialog_id_.id, native_aura_id_map));
}

void NativeDesktopMediaList::OnSourceThumbnailCaptured(
    int index,
    const gfx::ImageSkia& image) {
  UpdateSourceThumbnail(GetSource(index).id, image);
}

void NativeDesktopMediaList::FinishRefreshOnUiThreadAndScheduleNext(
    const std::vector<DesktopMediaID>& aura_ids) {
  // Schedule a refresh here when there is no aura thumbanil capture or schedule
  // a refresh in OnAuraThumbnailCaptured() after all aura thumbnails are
  // captured.
  if (aura_ids.size() == 0) {
    ScheduleNextRefresh();
    return;
  }

#if defined(USE_AURA)
  DCHECK_EQ(pending_aura_capture_requests_, 0);
  for (const auto& aura_id : aura_ids) {
    CaptureAuraWindowThumbnail(aura_id);
  }
#endif
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
  ui::GrabWindowSnapshotAndScaleAsync(
      window, window_rect, scaled_rect.size(), BrowserThread::GetBlockingPool(),
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
    ScheduleNextRefresh();
  }
}

#endif  // defined(USE_AURA)
