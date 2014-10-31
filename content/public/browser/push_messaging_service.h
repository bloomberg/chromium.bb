// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PUSH_MESSAGING_SERVICE_H_
#define CONTENT_PUBLIC_BROWSER_PUSH_MESSAGING_SERVICE_H_

#include <string>

#include "base/callback.h"
#include "content/common/content_export.h"
#include "content/public/common/push_messaging_status.h"
#include "third_party/WebKit/public/platform/WebPushPermissionStatus.h"
#include "url/gurl.h"

namespace content {

// A push service-agnostic interface that the Push API uses for talking to
// push messaging services like GCM.
class CONTENT_EXPORT PushMessagingService {
 public:
  typedef base::Callback<void(const GURL& /* endpoint */,
                              const std::string& /* registration_id */,
                              PushRegistrationStatus /* status */)>
      RegisterCallback;

  virtual ~PushMessagingService() {}

  // Register the given |sender_id| with GCM.
  virtual void Register(const GURL& origin,
                        int64 service_worker_registration_id,
                        const std::string& sender_id,
                        int renderer_id,
                        int render_frame_id,
                        bool user_gesture,
                        const RegisterCallback& callback) = 0;

  // Check whether the requester has permission to register for Push
  // Messages
  virtual blink::WebPushPermissionStatus GetPermissionStatus(
      const GURL& requesting_origin,
      int renderer_id,
      int render_frame_id) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PUSH_MESSAGING_SERVICE_H_
