// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSER_PPAPI_HOST_H_
#define CONTENT_PUBLIC_BROWSER_BROWSER_PPAPI_HOST_H_

#include "base/process.h"
#include "content/common/content_export.h"

namespace ppapi {
namespace host {
class PpapiHost;
}
}

namespace content {

// Interface that allows components in the embedder app to talk to the
// PpapiHost in the browser process.
//
// There will be one of these objects in the browser per plugin process. It
// lives entirely on the I/O thread.
class CONTENT_EXPORT BrowserPpapiHost {
 public:
  // Returns the PpapiHost object.
  virtual ppapi::host::PpapiHost* GetPpapiHost() = 0;

  // Returns the handle to the plugin process.
  virtual base::ProcessHandle GetPluginProcessHandle() const = 0;

 protected:
  virtual ~BrowserPpapiHost() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSER_PPAPI_HOST_H_
