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
#include "third_party/WebKit/Source/WebKit/chromium/public/WebStorageQuotaError.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebStorageQuotaType.h"

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
  QuotaDispatcher();
  virtual ~QuotaDispatcher();

  // IPC::Channel::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

  void QueryStorageUsageAndQuota(const GURL& gurl,
                                 WebKit::WebStorageQuotaType type,
                                 WebKit::WebStorageQuotaCallbacks* callbacks);
  void RequestStorageQuota(const GURL& gurl,
                           WebKit::WebStorageQuotaType type,
                           unsigned long long requested_size,
                           WebKit::WebStorageQuotaCallbacks* callbacks);

 private:
  // Message handlers.
  void DidQueryStorageUsageAndQuota(int request_id,
                                    int64 current_usage,
                                    int64 current_quota);
  void DidGrantStorageQuota(int request_id,
                            int64 granted_quota);
  void DidFail(int request_id,
               WebKit::WebStorageQuotaError error);

  IDMap<WebKit::WebStorageQuotaCallbacks> pending_quota_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(QuotaDispatcher);
};

#endif  // CONTENT_COMMON_QUOTA_DISPATCHER_H_
