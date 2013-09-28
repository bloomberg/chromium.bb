// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef CHROME_BROWSER_RENDERER_HOST_PEPPER_PEPPER_OUTPUT_PROTECTION_MESSAGE_FILTER_H_
#define CHROME_BROWSER_RENDERER_HOST_PEPPER_PEPPER_OUTPUT_PROTECTION_MESSAGE_FILTER_H_

#include "content/public/browser/browser_thread.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/resource_message_filter.h"

#if defined(OS_CHROMEOS)
#include "chromeos/display/output_configurator.h"
#endif

namespace ppapi {
namespace host {
class PpapiHost;
}
}

namespace chrome {

class PepperOutputProtectionMessageFilter
    : public ppapi::host::ResourceMessageFilter {
 public:
  PepperOutputProtectionMessageFilter();

 private:
  // ppapi::host::ResourceMessageFilter overrides.
  virtual scoped_refptr<base::TaskRunner> OverrideTaskRunnerForMessage(
      const IPC::Message& msg) OVERRIDE;
  virtual int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) OVERRIDE;

  virtual ~PepperOutputProtectionMessageFilter();

#if defined(OS_CHROMEOS)
  chromeos::OutputConfigurator::OutputProtectionClientId GetClientId();
#endif
  int32_t OnQueryStatus(ppapi::host::HostMessageContext* context);
  int32_t OnEnableProtection(ppapi::host::HostMessageContext* context,
                             uint32_t desired_method_mask);

#if defined(OS_CHROMEOS)
  chromeos::OutputConfigurator::OutputProtectionClientId client_id_;
#endif

  DISALLOW_COPY_AND_ASSIGN(PepperOutputProtectionMessageFilter);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_RENDERER_HOST_PEPPER_PEPPER_OUTPUT_PROTECTION_MESSAGE_FILTER_H_
