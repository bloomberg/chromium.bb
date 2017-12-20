// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_PEPPER_PEPPER_OUTPUT_PROTECTION_MESSAGE_FILTER_H_
#define CHROME_BROWSER_RENDERER_HOST_PEPPER_PEPPER_OUTPUT_PROTECTION_MESSAGE_FILTER_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/browser_thread.h"
#include "build/build_config.h"
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

class OutputProtectionProxy;

class PepperOutputProtectionMessageFilter
    : public ppapi::host::ResourceMessageFilter {
 public:
  PepperOutputProtectionMessageFilter(content::BrowserPpapiHost* host,
                                      PP_Instance instance);

 private:
  // Result handlers for calls made on the UI thread, which hand off the result
  // to the IPC filter on the IO thread.
  static void OnQueryStatusComplete(
      base::WeakPtr<PepperOutputProtectionMessageFilter> filter,
      ppapi::host::ReplyMessageContext reply_context,
      bool success,
      uint32_t link_mask,
      uint32_t protection_mask);
  static void OnEnableProtectionComplete(
      base::WeakPtr<PepperOutputProtectionMessageFilter> filter,
      ppapi::host::ReplyMessageContext reply_context,
      bool success);

  // ppapi::host::ResourceMessageFilter overrides.
  scoped_refptr<base::TaskRunner> OverrideTaskRunnerForMessage(
      const IPC::Message& msg) override;
  int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) override;

  ~PepperOutputProtectionMessageFilter() override;

  int32_t OnQueryStatus(ppapi::host::HostMessageContext* context);
  int32_t OnEnableProtection(ppapi::host::HostMessageContext* context,
                             uint32_t desired_method_mask);

  // Sends IPC replies for operations. Called on the IO thread.
  void OnQueryStatusCompleteOnIOThread(
      ppapi::host::ReplyMessageContext reply_context,
      bool success,
      uint32_t link_mask,
      uint32_t protection_mask);
  void OnEnableProtectionCompleteOnIOThread(
      ppapi::host::ReplyMessageContext reply_context,
      bool success);

  std::unique_ptr<OutputProtectionProxy,
                  content::BrowserThread::DeleteOnUIThread>
      proxy_;

  base::WeakPtrFactory<PepperOutputProtectionMessageFilter> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PepperOutputProtectionMessageFilter);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_PEPPER_PEPPER_OUTPUT_PROTECTION_MESSAGE_FILTER_H_
