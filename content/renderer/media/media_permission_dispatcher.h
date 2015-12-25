// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Base class for MediaPermission implementations in content. This class allows
// multiple pending PermissionService calls by managing callbacks for its
// subclasses. This class is not thread safe and should only be used on one
// thread.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_PERMISSION_DISPATCHER_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_PERMISSION_DISPATCHER_H_

#include <stdint.h>

#include <map>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "media/base/media_permission.h"

namespace content {

class CONTENT_EXPORT MediaPermissionDispatcher : public media::MediaPermission {
 public:
  MediaPermissionDispatcher();
  ~MediaPermissionDispatcher() override;

 protected:
  // Register callbacks for PermissionService calls.
  uint32_t RegisterCallback(const PermissionStatusCB& permission_status_cb);

  // Deliver the permission status |granted| to the callback identified by
  // |request_id|.
  void DeliverResult(uint32_t request_id, bool granted);

  base::ThreadChecker& thread_checker() { return thread_checker_; }

 private:
  // Map of request IDs and pending PermissionStatusCBs.
  typedef std::map<uint32_t, PermissionStatusCB> RequestMap;

  base::ThreadChecker thread_checker_;

  uint32_t next_request_id_;
  RequestMap requests_;

  DISALLOW_COPY_AND_ASSIGN(MediaPermissionDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_PERMISSION_DISPATCHER_H_
