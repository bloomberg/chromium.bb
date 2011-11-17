// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_QUOTA_DISPATCHER_H_
#define CONTENT_COMMON_QUOTA_DISPATCHER_H_

#include <map>
#include <set>

#include "base/basictypes.h"
#include "base/id_map.h"
#include "ipc/ipc_channel.h"
#include "webkit/quota/quota_types.h"

class GURL;

namespace IPC {
class Message;
}

namespace WebKit {
class WebStorageQuotaCallbacks;
}

// Dispatches and sends quota related messages sent to/from a child
// process from/to the main browser process.  There is one instance
// per child process.  Messages are dispatched on the main child thread.
class QuotaDispatcher : public IPC::Channel::Listener {
 public:
  class Callback {
   public:
    virtual ~Callback() {}
    virtual void DidQueryStorageUsageAndQuota(int64 usage, int64 quota) = 0;
    virtual void DidGrantStorageQuota(int64 granted_quota) = 0;
    virtual void DidFail(quota::QuotaStatusCode status) = 0;
  };

  QuotaDispatcher();
  virtual ~QuotaDispatcher();

  // IPC::Channel::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

  void QueryStorageUsageAndQuota(const GURL& gurl,
                                 quota::StorageType type,
                                 Callback* callback);
  void RequestStorageQuota(int render_view_id,
                           const GURL& gurl,
                           quota::StorageType type,
                           int64 requested_size,
                           Callback* callback);

  // Creates a new Callback instance for WebStorageQuotaCallbacks.
  static Callback* CreateWebStorageQuotaCallbacksWrapper(
      WebKit::WebStorageQuotaCallbacks* callbacks);

 private:
  // Message handlers.
  void DidQueryStorageUsageAndQuota(int request_id,
                                    int64 current_usage,
                                    int64 current_quota);
  void DidGrantStorageQuota(int request_id,
                            int64 granted_quota);
  void DidFail(int request_id, quota::QuotaStatusCode error);

  IDMap<Callback, IDMapOwnPointer> pending_quota_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(QuotaDispatcher);
};

#endif  // CONTENT_COMMON_QUOTA_DISPATCHER_H_
