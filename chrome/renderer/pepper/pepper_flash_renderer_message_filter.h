// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PEPPER_PEPPER_FLASH_RENDERER_MESSAGE_FILTER_H_
#define CHROME_RENDERER_PEPPER_PEPPER_FLASH_RENDERER_MESSAGE_FILTER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/host/instance_message_filter.h"

namespace content {
class RendererPpapiHost;
}

namespace chrome {

// Implements the backend for Flash-specific messages from a plugin process.
class PepperFlashRendererMessageFilter
    : public ppapi::host::InstanceMessageFilter {
 public:
  // This class is designed to be heap-allocated. It will attach itself to the
  // given host and delete itself when the host is destroyed.
  explicit PepperFlashRendererMessageFilter(content::RendererPpapiHost* host);
  virtual ~PepperFlashRendererMessageFilter();

  // InstanceMessageFilter:
  virtual bool OnInstanceMessageReceived(const IPC::Message& msg) OVERRIDE;

 private:
  content::RendererPpapiHost* host_;

  DISALLOW_COPY_AND_ASSIGN(PepperFlashRendererMessageFilter);
};

}  // namespace chrome

#endif  // CHROME_RENDERER_PEPPER_PEPPER_FLASH_RENDERER_MESSAGE_FILTER_H_
