// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_NATIVE_DESKTOP_MEDIA_LIST_H_
#define CHROME_BROWSER_MEDIA_NATIVE_DESKTOP_MEDIA_LIST_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/media/desktop_media_list_base.h"
#include "content/public/browser/desktop_media_id.h"

namespace webrtc {
class ScreenCapturer;
class WindowCapturer;
}

// Implementation of DesktopMediaList that shows native screens and
// native windows.
class NativeDesktopMediaList : public DesktopMediaListBase {
 public:
  // Caller may pass NULL for either of the arguments in case when only some
  // types of sources the model should be populated with (e.g. it will only
  // contain windows, if |screen_capturer| is NULL).
  NativeDesktopMediaList(
      scoped_ptr<webrtc::ScreenCapturer> screen_capturer,
      scoped_ptr<webrtc::WindowCapturer> window_capturer);
  ~NativeDesktopMediaList() override;

 private:
  class Worker;
  friend class Worker;

  // Refresh() posts a task for the |worker_| to update list of windows and get
  // thumbnails. Then |worker_| first posts tasks for
  // UpdateObserverSourcesList() with fresh list of sources, then follows with
  // OnSourceThumbnail() for each changed thumbnail and then calls
  // DelayLaunchNextRefersh() at the end.
  void Refresh() override;
  void OnSourceThumbnail(int index, const gfx::ImageSkia& image);

  // Task runner used for the |worker_|.
  scoped_refptr<base::SequencedTaskRunner> capture_task_runner_;

  // An object that does all the work of getting list of sources on a background
  // thread (see |capture_task_runner_|). Destroyed on |capture_task_runner_|
  // after the model is destroyed.
  scoped_ptr<Worker> worker_;

  base::WeakPtrFactory<NativeDesktopMediaList> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NativeDesktopMediaList);
};

#endif  // CHROME_BROWSER_MEDIA_NATIVE_DESKTOP_MEDIA_LIST_H_
