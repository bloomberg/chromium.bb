// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_BACKGROUND_REQUEST_NOTIFIER_H_
#define COMPONENTS_OFFLINE_PAGES_BACKGROUND_REQUEST_NOTIFIER_H_

namespace offline_pages {

class SavePageRequest;

class RequestNotifier {
 public:
  // Status to return for failed notifications.
  // TODO(petewil): Can we find a better name for this enum?
  enum class SavePageStatus {
    SUCCESS,
    PRERENDER_FAILURE,
    PRERENDER_CANCELED,
    FOREGROUND_CANCELED,
    SAVE_FAILED,
    EXPIRED,
    RETRY_COUNT_EXCEEDED,
    START_COUNT_EXCEEDED,
    REMOVED,
  };

  virtual ~RequestNotifier() = default;

  // Notifies observers that |request| has been added.
  virtual void NotifyAdded(const SavePageRequest& request) = 0;

  // Notifies observers that |request| has been completed with |status|.
  virtual void NotifyCompleted(const SavePageRequest& request,
                               SavePageStatus status) = 0;

  // Notifies observers that |request| has been changed.
  virtual void NotifyChanged(const SavePageRequest& request) = 0;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_BACKGROUND_REQUEST_NOTIFIER_H_
