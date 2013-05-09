// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/pepper/pepper_extensions_common_message_filter.h"

#include "base/values.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "content/public/browser/browser_thread.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace chrome {

// static
PepperExtensionsCommonMessageFilter*
PepperExtensionsCommonMessageFilter::Create(content::BrowserPpapiHost* host,
                                            PP_Instance instance) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  int render_process_id = 0;
  int render_view_id = 0;
  if (!host->GetRenderViewIDsForInstance(instance, &render_process_id,
                                         &render_view_id)) {
    return NULL;
  }

  base::FilePath profile_directory = host->GetProfileDataDirectory();
  GURL document_url = host->GetDocumentURLForInstance(instance);

  return new PepperExtensionsCommonMessageFilter(render_process_id,
                                                 render_view_id,
                                                 profile_directory,
                                                 document_url);
}

PepperExtensionsCommonMessageFilter::PepperExtensionsCommonMessageFilter(
    int render_process_id,
    int render_view_id,
    const base::FilePath& profile_directory,
    const GURL& document_url)
    : render_process_id_(render_process_id),
      render_view_id_(render_view_id),
      profile_directory_(profile_directory),
      document_url_(document_url) {
}

PepperExtensionsCommonMessageFilter::~PepperExtensionsCommonMessageFilter() {
}

scoped_refptr<base::TaskRunner>
PepperExtensionsCommonMessageFilter::OverrideTaskRunnerForMessage(
    const IPC::Message& msg) {
  return content::BrowserThread::GetMessageLoopProxyForThread(
      content::BrowserThread::UI);
}

int32_t PepperExtensionsCommonMessageFilter::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  IPC_BEGIN_MESSAGE_MAP(PepperExtensionsCommonMessageFilter, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_ExtensionsCommon_Post,
                                      OnPost)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_ExtensionsCommon_Call,
                                      OnCall)
  IPC_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

int32_t PepperExtensionsCommonMessageFilter::OnPost(
    ppapi::host::HostMessageContext* context,
    const std::string& request_name,
    base::ListValue& args) {
  // TODO(yzshen): Implement it.
  return PP_ERROR_FAILED;
}

int32_t PepperExtensionsCommonMessageFilter::OnCall(
    ppapi::host::HostMessageContext* context,
    const std::string& request_name,
    base::ListValue& args) {
  // TODO(yzshen): Implement it.
  return PP_ERROR_FAILED;
}

}  // namespace chrome
