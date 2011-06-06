// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_QUOTA_PERMISSION_CONTEXT_H_
#define CONTENT_BROWSER_QUOTA_PERMISSION_CONTEXT_H_

#include "base/callback_old.h"
#include "base/memory/ref_counted.h"
#include "webkit/quota/quota_types.h"

class GURL;

class QuotaPermissionContext
    : public base::RefCountedThreadSafe<QuotaPermissionContext> {
 public:
  enum Response {
    kResponseUnknown,
    kResponseAllow,
    kResponseDisallow,
    kResponseCancelled,
  };
  typedef Callback1<Response>::Type PermissionCallback;

  virtual void RequestQuotaPermission(
      const GURL& origin_url,
      quota::StorageType type,
      int64 new_quota,
      int render_process_id,
      int render_view_id,
      PermissionCallback* callback) = 0;

 protected:
  friend class base::RefCountedThreadSafe<QuotaPermissionContext>;
  virtual ~QuotaPermissionContext() {}
};

#endif  // CONTENT_BROWSER_QUOTA_PERMISSION_CONTEXT_H_
