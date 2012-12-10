// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/pepper/pepper_broker_host.h"

#include <string>

#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/content_settings.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_message_macros.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/resource_message_filter.h"
#include "ppapi/proxy/ppapi_messages.h"

using content::BrowserPpapiHost;
using content::BrowserThread;
using content::RenderProcessHost;

namespace chrome {

namespace {

// This filter handles messages for the PepperBrokerHost on the UI thread.
class BrokerMessageFilter : public ppapi::host::ResourceMessageFilter {
 public:
  BrokerMessageFilter(int render_process_id, GURL document_url);

 protected:
  // ppapi::host::ResourceMessageFilter override.
  virtual scoped_refptr<base::TaskRunner> OverrideTaskRunnerForMessage(
      const IPC::Message& message) OVERRIDE;

  // ppapi::host::ResourceMessageHandler override.
  virtual int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) OVERRIDE;

 private:
  virtual ~BrokerMessageFilter();

  int32_t OnIsAllowed(ppapi::host::HostMessageContext* context);

  int render_process_id_;
  GURL document_url_;
};

BrokerMessageFilter::BrokerMessageFilter(
    int render_process_id,
    GURL document_url)
    : render_process_id_(render_process_id),
      document_url_(document_url) {
}

BrokerMessageFilter::~BrokerMessageFilter() {
}

scoped_refptr<base::TaskRunner>
BrokerMessageFilter::OverrideTaskRunnerForMessage(
    const IPC::Message& message) {
  return BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI);
}

int32_t BrokerMessageFilter::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  IPC_BEGIN_MESSAGE_MAP(BrokerMessageFilter, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_Broker_IsAllowed,
                                        OnIsAllowed)
  IPC_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

int32_t BrokerMessageFilter::OnIsAllowed(
    ppapi::host::HostMessageContext* context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!document_url_.is_valid())
    return PP_ERROR_FAILED;
  RenderProcessHost* render_process_host =
      RenderProcessHost::FromID(render_process_id_);
  if (!render_process_host)
    return PP_ERROR_FAILED;
  Profile* profile =
      Profile::FromBrowserContext(render_process_host->GetBrowserContext());
  HostContentSettingsMap* content_settings =
      profile->GetHostContentSettingsMap();
  ContentSetting setting =
      content_settings->GetContentSetting(document_url_, document_url_,
                                          CONTENT_SETTINGS_TYPE_PPAPI_BROKER,
                                          std::string());
  if (setting == CONTENT_SETTING_ALLOW)
    return PP_OK;
  return PP_ERROR_FAILED;
}

}  // namespace

PepperBrokerHost::PepperBrokerHost(BrowserPpapiHost* host,
                                   PP_Instance instance,
                                   PP_Resource resource)
    : ResourceHost(host->GetPpapiHost(), instance, resource) {
  int render_process_id, unused;
  host->GetRenderViewIDsForInstance(instance, &render_process_id, &unused);
  const GURL& document_url = host->GetDocumentURLForInstance(instance);
  AddFilter(make_scoped_refptr(new BrokerMessageFilter(render_process_id,
                                                       document_url)));
}

PepperBrokerHost::~PepperBrokerHost() {
}

}  // namespace chrome
