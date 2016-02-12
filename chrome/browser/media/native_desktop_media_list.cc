// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/native_desktop_media_list.h"

#include "base/hash.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/media/desktop_media_list_observer.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "media/base/video_util.h"
#include "third_party/libyuv/include/libyuv/scale_argb.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/screen_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/window_capturer.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;
using content::DesktopMediaID;

namespace {

// Update the list every second.
const int kDefaultUpdatePeriod = 1000;

// Returns a hash of a DesktopFrame content to detect when image for a desktop
// media source has changed.
uint32_t GetFrameHash(webrtc::DesktopFrame* frame) {
  int data_size = frame->stride() * frame->size().height();
  return base::SuperFastHash(reinterpret_cast<char*>(frame->data()), data_size);
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

}  // namespace

class NativeDesktopMediaList::Worker
    : public webrtc::DesktopCapturer::Callback {
 public:
  Worker(base::WeakPtr<NativeDesktopMediaList> media_list,
         scoped_ptr<webrtc::ScreenCapturer> screen_capturer,
         scoped_ptr<webrtc::WindowCapturer> window_capturer);
  ~Worker() override;

  void Refresh(const gfx::Size& thumbnail_size,
               content::DesktopMediaID::Id view_dialog_id);

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
    content::DesktopMediaID::Id view_dialog_id) {
  std::vector<SourceDescription> sources;

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
          sources.push_back(SourceDescription(
              DesktopMediaID(DesktopMediaID::TYPE_WINDOW, it->id),
              base::UTF8ToUTF16(it->title)));
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
            base::Bind(&NativeDesktopMediaList::OnSourceThumbnail, media_list_,
                       i, thumbnail));
      }
    }
  }

  image_hashes_.swap(new_image_hashes);

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&NativeDesktopMediaList::ScheduleNextRefresh, media_list_));
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
  capture_task_runner_->PostTask(
      FROM_HERE, base::Bind(&Worker::Refresh, base::Unretained(worker_.get()),
                            thumbnail_size_, view_dialog_id_.id));
}

void NativeDesktopMediaList::OnSourceThumbnail(int index,
                                               const gfx::ImageSkia& image) {
  UpdateSourceThumbnail(GetSource(index).id, image);
}
