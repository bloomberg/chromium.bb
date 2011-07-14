// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_MEDIA_INTERNALS_OBSERVER_H_
#define CHROME_BROWSER_MEDIA_MEDIA_INTERNALS_OBSERVER_H_
#pragma once

// Used by MediaInternalsUI to receive callbacks on media events.
// Callbacks will be on the IO thread.
class MediaInternalsObserver {
 public:
  // Handle an information update consisting of a javascript function call.
  virtual void OnUpdate(const string16& javascript) = 0;
  virtual ~MediaInternalsObserver() {}
};

#endif  // CHROME_BROWSER_MEDIA_MEDIA_INTERNALS_OBSERVER_H_
