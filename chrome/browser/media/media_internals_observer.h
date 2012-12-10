// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_MEDIA_INTERNALS_OBSERVER_H_
#define CHROME_BROWSER_MEDIA_MEDIA_INTERNALS_OBSERVER_H_

#include "content/public/browser/media_request_state.h"
#include "content/public/common/media_stream_request.h"

// Used by MediaInternalsUI to receive callbacks on media events.
// Callbacks will be on the IO thread.
class MediaInternalsObserver {
 public:
  // Handle an information update consisting of a javascript function call.
  virtual void OnUpdate(const string16& javascript) {}

  // Handle an information update related to a media stream request.
  virtual void OnRequestUpdate(
      int render_process_id,
      int render_view_id,
      const content::MediaStreamDevice& device,
      const content::MediaRequestState state) {}

  virtual ~MediaInternalsObserver() {}
};

#endif  // CHROME_BROWSER_MEDIA_MEDIA_INTERNALS_OBSERVER_H_
