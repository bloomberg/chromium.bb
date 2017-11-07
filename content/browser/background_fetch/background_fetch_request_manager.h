// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_REQUEST_MANAGER_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_REQUEST_MANAGER_H_

#include <string>

#include "base/callback_forward.h"
#include "base/memory/scoped_refptr.h"

namespace content {

class BackgroundFetchRegistrationId;
class BackgroundFetchRequestInfo;

// Interface for manager requests that are part of a Background Fetch.
// Implementations maintain a queue of requests for each given
// |BackgroundFetchRegistrationId| that may be backed by a database.
class BackgroundFetchRequestManager {
 public:
  using NextRequestCallback =
      base::OnceCallback<void(scoped_refptr<BackgroundFetchRequestInfo>)>;
  using MarkedCompleteCallback =
      base::OnceCallback<void(bool /* has_pending_or_active_requests */)>;

  virtual ~BackgroundFetchRequestManager() {}

  // Removes the next request, if any, from the pending requests queue, and
  // invokes the |callback| with that request, else a null request.
  virtual void PopNextRequest(
      const BackgroundFetchRegistrationId& registration_id,
      NextRequestCallback callback) = 0;

  // Marks that the |request|, part of the Background Fetch identified by
  // |registration_id|, has been started as |download_guid|.
  virtual void MarkRequestAsStarted(
      const BackgroundFetchRegistrationId& registration_id,
      BackgroundFetchRequestInfo* request,
      const std::string& download_guid) = 0;

  // Marks that the |request|, part of the Background Fetch identified by
  // |registration_id|, has completed.
  virtual void MarkRequestAsComplete(
      const BackgroundFetchRegistrationId& registration_id,
      BackgroundFetchRequestInfo* request,
      MarkedCompleteCallback callback) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_REQUEST_MANAGER_H_
