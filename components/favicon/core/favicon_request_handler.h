// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CORE_FAVICON_REQUEST_HANDLER_H_
#define COMPONENTS_FAVICON_CORE_FAVICON_REQUEST_HANDLER_H_

#include <map>

#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/favicon_base/favicon_callback.h"
#include "components/favicon_base/favicon_request_metrics.h"
#include "url/gurl.h"

namespace favicon {

class FaviconService;
class LargeIconService;

// Class for handling favicon requests by page url, forwarding them to local
// storage, sync or Google server accordingly.
// TODO(victorvianna): Refactor LargeIconService to avoid having to pass both
// it and FaviconService to the API.
// TODO(victorvianna): Use a more natural order for the parameters in the API.
// TODO(victorvianna): Remove |icon_url_for_uma| when we have access to the
// FaviconUrlMapper.
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
  // Tries to fetch the icon from local storage and falls back to sync, or to
  // Google favicon server if |favicon::kEnableHistoryFaviconsGoogleServerQuery|
  // is enabled. |can_send_history_data| indicates whether user settings allow
  // to query the favicon server using history data (in particular it must check
  // that history sync is enabled and no custom passphrase is set).
  // If a non-empty |icon_url_for_uma| (optional) is passed, it will be used to
  // record UMA about the grouping of requests to the favicon server.
  void GetRawFaviconForPageURL(const GURL& page_url,
                               int desired_size_in_pixel,
                               favicon_base::FaviconRawBitmapCallback callback,
                               FaviconRequestOrigin request_origin,
                               FaviconService* favicon_service,
                               LargeIconService* large_icon_service,
                               const GURL& icon_url_for_uma,
                               SyncedFaviconGetter synced_favicon_getter,
                               bool can_send_history_data,
                               base::CancelableTaskTracker* tracker);

  // Requests favicon image at |page_url|.
  // Tries to fetch the icon from local storage and falls back to sync, or to
  // Google favicon server if |favicon::kEnableHistoryFaviconsGoogleServerQuery|
  // is enabled. |can_send_history_data| indicates whether user settings allow
  // to query the favicon server using history data (in particular it must check
  // that history sync is enabled and no custom passphrase is set).
  // If a non-empty |icon_url_for_uma| (optional) is passed, it will be used to
  // record UMA about the grouping of requests to the favicon server.
  void GetFaviconImageForPageURL(const GURL& page_url,
                                 favicon_base::FaviconImageCallback callback,
                                 FaviconRequestOrigin request_origin,
                                 FaviconService* favicon_service,
                                 LargeIconService* large_icon_service,
                                 const GURL& icon_url_for_uma,
                                 SyncedFaviconGetter synced_favicon_getter,
                                 bool can_send_history_data,
                                 base::CancelableTaskTracker* tracker);

 private:
  static bool CanQueryGoogleServer(LargeIconService* large_icon_service,
                                   FaviconRequestOrigin origin,
                                   bool can_send_history_data);

  // Called after the first attempt to retrieve the icon bitmap from local
  // storage. If request succeeded, sends the result. Otherwise attempts to
  // retrieve from sync or the Google favicon server depending whether
  // |favicon::kEnableHistoryFaviconsGoogleServerQuery| is enabled.
  void OnBitmapLocalDataAvailable(
      const GURL& page_url,
      int desired_size_in_pixel,
      favicon_base::FaviconRawBitmapCallback response_callback,
      FaviconRequestOrigin origin,
      FaviconService* favicon_service,
      LargeIconService* large_icon_service,
      const GURL& icon_url_for_uma,
      SyncedFaviconGetter synced_favicon_getter,
      bool can_query_google_server,
      base::CancelableTaskTracker* tracker,
      const favicon_base::FaviconRawBitmapResult& bitmap_result);

  // Called after the first attempt to retrieve the icon image from local
  // storage. If request succeeded, sends the result. Otherwise attempts to
  // retrieve from sync or the Google favicon server depending whether
  // |favicon::kEnableHistoryFaviconsGoogleServerQuery| is enabled.
  void OnImageLocalDataAvailable(
      const GURL& page_url,
      favicon_base::FaviconImageCallback response_callback,
      FaviconRequestOrigin origin,
      FaviconService* favicon_service,
      LargeIconService* large_icon_service,
      const GURL& icon_url_for_uma,
      SyncedFaviconGetter synced_favicon_getter,
      bool can_query_google_server,
      base::CancelableTaskTracker* tracker,
      const favicon_base::FaviconImageResult& image_result);

  // Requests an icon from Google favicon server. Since requests work by
  // populating local storage, a |local_lookup_callback| will be needed in case
  // of success and an |empty_response_callback| in case of failure.
  void RequestFromGoogleServer(const GURL& page_url,
                               base::OnceClosure empty_response_callback,
                               base::OnceClosure local_lookup_callback,
                               LargeIconService* large_icon_service,
                               const GURL& icon_url_for_uma,
                               FaviconRequestOrigin origin);

  // Called once the request to the favicon server has finished. If the request
  // succeeded, |local_lookup_callback| is called to effectively retrieve the
  // icon, otherwise |empty_response_callback| is called. If |group_to_clear|
  // is non-empty, records the size of the associated group as UMA and clears
  // it, simulating the execution of its array of waiting callbacks.
  void OnGoogleServerDataAvailable(
      base::OnceClosure empty_response_callback,
      base::OnceClosure local_lookup_callback,
      FaviconRequestOrigin origin,
      const GURL& group_to_clear,
      favicon_base::GoogleFaviconServerRequestStatus status);

  // Map from a group identifier to the number of callbacks in that group which
  // would be waiting for execution. Used for recording metrics for the possible
  // benefit of grouping.
  std::map<GURL, int> group_callbacks_count_;

  base::WeakPtrFactory<FaviconRequestHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FaviconRequestHandler);
};

}  // namespace favicon

#endif  // COMPONENTS_FAVICON_CORE_FAVICON_REQUEST_HANDLER_H_
