// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_REGISTRATION_ID_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_REGISTRATION_ID_H_

#include <stdint.h>
#include <string>

#include "base/macros.h"
#include "content/common/content_export.h"
#include "url/origin.h"

namespace content {

// The Background Fetch registration id corresponds to the information required
// to uniquely identify a Background Fetch registration in scope of a profile.
class CONTENT_EXPORT BackgroundFetchRegistrationId {
 public:
  BackgroundFetchRegistrationId();
  BackgroundFetchRegistrationId(int64_t service_worker_registration_id,
                                const url::Origin& origin,
                                const std::string& tag);
  BackgroundFetchRegistrationId(BackgroundFetchRegistrationId&& other);
  ~BackgroundFetchRegistrationId();

  BackgroundFetchRegistrationId& operator=(
      const BackgroundFetchRegistrationId& other);

  // Returns whether the |other| registration id are identical or different.
  bool operator==(const BackgroundFetchRegistrationId& other) const;
  bool operator!=(const BackgroundFetchRegistrationId& other) const;

  // Enables this type to be used in an std::map and std::set.
  // TODO(peter): Delete this when we switch away from using maps.
  bool operator<(const BackgroundFetchRegistrationId& other) const;

  // Returns whether this registration id refers to valid data.
  bool is_null() const;

  int64_t service_worker_registration_id() const {
    return service_worker_registration_id_;
  }
  const url::Origin& origin() const { return origin_; }
  const std::string& tag() const { return tag_; }

 private:
  int64_t service_worker_registration_id_;
  url::Origin origin_;
  std::string tag_;

  DISALLOW_COPY(BackgroundFetchRegistrationId);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_REGISTRATION_ID_H_
