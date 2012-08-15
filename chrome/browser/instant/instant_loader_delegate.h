// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTANT_INSTANT_LOADER_DELEGATE_H_
#define CHROME_BROWSER_INSTANT_INSTANT_LOADER_DELEGATE_H_

#include <vector>

#include "chrome/common/instant_types.h"

class InstantLoader;

// InstantLoader calls these methods on its delegate (InstantController)
// to notify it of interesting events happening on the Instant preview.
class InstantLoaderDelegate {
 public:
  // Invoked when the loader has suggested text.
  virtual void SetSuggestions(
      InstantLoader* loader,
      const std::vector<InstantSuggestion>& suggestions) = 0;

  // Commit the preview.
  virtual void CommitInstantLoader(InstantLoader* loader) = 0;

  // Notification that the first page load (of the Instant URL) completed.
  virtual void InstantLoaderPreviewLoaded(InstantLoader* loader) = 0;

  // Notification when the loader has determined whether or not the page
  // supports the Instant API.
  virtual void InstantSupportDetermined(InstantLoader* loader,
                                        bool supports_instant) = 0;

  // Notification that the loader swapped a different TabContents into the
  // preview, usually because a prerendered page was navigated to.
  virtual void SwappedTabContents(InstantLoader* loader) = 0;

  // Notification that the preview gained focus, usually due to the user
  // clicking on it.
  virtual void InstantLoaderContentsFocused(InstantLoader* loader) = 0;

 protected:
  virtual ~InstantLoaderDelegate() {}
};

#endif  // CHROME_BROWSER_INSTANT_INSTANT_LOADER_DELEGATE_H_
