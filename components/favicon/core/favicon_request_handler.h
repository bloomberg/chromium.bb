// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CORE_FAVICON_REQUEST_HANDLER_H_
#define COMPONENTS_FAVICON_CORE_FAVICON_REQUEST_HANDLER_H_

#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/favicon_base/favicon_callback.h"
#include "components/favicon_base/favicon_request_metrics.h"
#include "url/gurl.h"

namespace favicon {

class FaviconService;

// Class for handling favicon requests by page url, forwarding them to local
// storage or sync accordingly.
class FaviconRequestHandler {
 public:
  // Callback that requests the synced bitmap for the page url given in the
  // the first argument, storing the result in the second argument. Returns
  // whether the request succeeded.
  // TODO(victorvianna): Make this return a pointer instead of a bool.
  using SyncedFaviconGetter =
      base::OnceCallback<bool(const GURL&,
                              scoped_refptr<base::RefCountedMemory>*)>;

  FaviconRequestHandler();

  ~FaviconRequestHandler();

  // Requests favicon bitmap at |page_url| of size |desired_size_in_pixel|.
  // Tries to fetch the icon from local storage and falls back to sync if it's
  // not found.
  void GetRawFaviconForPageURL(
      const GURL& page_url,
      int desired_size_in_pixel,
      const favicon_base::FaviconRawBitmapCallback& callback,
      FaviconRequestOrigin request_origin,
      FaviconService* favicon_service,
      SyncedFaviconGetter synced_favicon_getter,
      base::CancelableTaskTracker* tracker);

  // Requests favicon image at |page_url|. Tries to fetch the icon from local
  // storage and falls back to sync if it's not found.
  void GetFaviconImageForPageURL(
      const GURL& page_url,
      const favicon_base::FaviconImageCallback& callback,
      FaviconRequestOrigin request_origin,
      FaviconService* favicon_service,
      SyncedFaviconGetter synced_favicon_getter,
      base::CancelableTaskTracker* tracker);

 private:
  // Called after the first attempt to retrieve the icon bitmap from local
  // storage. If request succeeded, sends the result. Otherwise attempts to
  // retrieve from sync.
  void OnBitmapLocalDataAvailable(
      const GURL& page_url,
      const favicon_base::FaviconRawBitmapCallback& response_callback,
      FaviconRequestOrigin origin,
      SyncedFaviconGetter synced_favicon_getter,
      const favicon_base::FaviconRawBitmapResult& bitmap_result) const;

  // Called after the first attempt to retrieve the icon image from local
  // storage. If request succeeded, sends the result. Otherwise attempts to
  // retrieve from sync.
  void OnImageLocalDataAvailable(
      const GURL& page_url,
      const favicon_base::FaviconImageCallback& response_callback,
      FaviconRequestOrigin origin,
      SyncedFaviconGetter synced_favicon_getter,
      const favicon_base::FaviconImageResult& image_result) const;

  base::WeakPtrFactory<FaviconRequestHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FaviconRequestHandler);
};

}  // namespace favicon

#endif  // COMPONENTS_FAVICON_CORE_FAVICON_REQUEST_HANDLER_H_
