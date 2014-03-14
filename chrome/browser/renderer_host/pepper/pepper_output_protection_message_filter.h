// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_PEPPER_PEPPER_OUTPUT_PROTECTION_MESSAGE_FILTER_H_
#define CHROME_BROWSER_RENDERER_HOST_PEPPER_PEPPER_OUTPUT_PROTECTION_MESSAGE_FILTER_H_

#include "ppapi/c/pp_instance.h"
#include "ppapi/host/resource_message_filter.h"

namespace content {
class BrowserPpapiHost;
}  // namespace content

namespace ppapi {
namespace host {
struct HostMessageContext;
}  // namespace host
}  // namespace ppapi

namespace chrome {

class PepperOutputProtectionMessageFilter
    : public ppapi::host::ResourceMessageFilter {
 public:
  PepperOutputProtectionMessageFilter(content::BrowserPpapiHost* host,
                                      PP_Instance instance);

 private:
#if defined(OS_CHROMEOS)
  class Delegate;
#endif

  // ppapi::host::ResourceMessageFilter overrides.
  virtual scoped_refptr<base::TaskRunner> OverrideTaskRunnerForMessage(
      const IPC::Message& msg) OVERRIDE;
  virtual int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) OVERRIDE;

  virtual ~PepperOutputProtectionMessageFilter();

  int32_t OnQueryStatus(ppapi::host::HostMessageContext* context);
  int32_t OnEnableProtection(ppapi::host::HostMessageContext* context,
                             uint32_t desired_method_mask);

#if defined(OS_CHROMEOS)
  // Delegator. Should be deleted in UI thread.
  Delegate* delegate_;
#endif

  DISALLOW_COPY_AND_ASSIGN(PepperOutputProtectionMessageFilter);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_RENDERER_HOST_PEPPER_PEPPER_OUTPUT_PROTECTION_MESSAGE_FILTER_H_
