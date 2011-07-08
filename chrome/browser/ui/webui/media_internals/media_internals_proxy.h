// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MEDIA_INTERNALS_MEDIA_INTERNALS_PROXY_H_
#define CHROME_BROWSER_UI_WEBUI_MEDIA_INTERNALS_MEDIA_INTERNALS_PROXY_H_
#pragma once

#include "base/string16.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/media/media_internals_observer.h"

class IOThread;
class MediaInternalsMessageHandler;

// This class is a proxy between MediaInternals (on the IO thread) and
// MediaInternalsMessageHandler (on the UI thread).
// It is ref_counted to ensure that it completes all pending Tasks on both
// threads before destruction.
class MediaInternalsProxy
    : public MediaInternalsObserver,
      public base::RefCountedThreadSafe<MediaInternalsProxy> {
 public:
  MediaInternalsProxy();

  // Register a Handler and start receiving callbacks from MediaInternals.
  void Attach(MediaInternalsMessageHandler* handler);

  // Unregister the same and stop receiving callbacks.
  void Detach();

  // Have MediaInternals send all the data it has.
  void GetEverything();

  // MediaInternalsObserver implementation. Called on the IO thread.
  virtual void OnUpdate(const string16& update);

 private:
  friend class base::RefCountedThreadSafe<MediaInternalsProxy>;
  virtual ~MediaInternalsProxy();

  void ObserveMediaInternalsOnIOThread();
  void StopObservingMediaInternalsOnIOThread();
  void GetEverythingOnIOThread();
  void UpdateUIOnUIThread(const string16& update);

  MediaInternalsMessageHandler* handler_;
  IOThread* io_thread_;

  DISALLOW_COPY_AND_ASSIGN(MediaInternalsProxy);
};

#endif  // CHROME_BROWSER_UI_WEBUI_MEDIA_INTERNALS_MEDIA_INTERNALS_PROXY_H_
