// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_QUOTA_MANAGER_BRIDGE_H_
#define ANDROID_WEBVIEW_BROWSER_AW_QUOTA_MANAGER_BRIDGE_H_

#include "base/memory/ref_counted.h"

namespace android_webview {

// Empty base class so this can be refcounted by AwBrowserContext.
class AwQuotaManagerBridge :
    public base::RefCountedThreadSafe<AwQuotaManagerBridge> {
 protected:
  friend class base::RefCountedThreadSafe<AwQuotaManagerBridge>;
  AwQuotaManagerBridge();
  virtual ~AwQuotaManagerBridge();
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_QUOTA_MANAGER_BRIDGE_H_
