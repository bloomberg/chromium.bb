// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/renderer_overrides_handler.h"

#include <map>
#include <string>

#include "base/barrier_closure.h"
#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/strings/string16.h"
#include "base/thread_task_runner_handle.h"
#include "base/values.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/devtools/devtools_protocol_constants.h"
#include "content/browser/devtools/devtools_tracing_handler.h"
#include "content/browser/renderer_host/dip_util.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/cursors/webcursor.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/common/referrer.h"
#include "content/public/common/url_constants.h"
#include "ipc/ipc_sender.h"
#include "net/base/net_util.h"
#include "storage/browser/quota/quota_manager.h"
#include "third_party/WebKit/public/platform/WebCursorInfo.h"
#include "third_party/WebKit/public/platform/WebScreenInfo.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/size_conversions.h"
#include "ui/snapshot/snapshot.h"
#include "url/gurl.h"

using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebMouseEvent;

namespace content {

RendererOverridesHandler::RendererOverridesHandler()
    : weak_factory_(this) {
  RegisterCommandHandler(
      devtools::Network::canEmulateNetworkConditions::kName,
      base::Bind(
          &RendererOverridesHandler::CanEmulateNetworkConditions,
          base::Unretained(this)));
  RegisterCommandHandler(
      devtools::Network::clearBrowserCache::kName,
      base::Bind(
          &RendererOverridesHandler::ClearBrowserCache,
          base::Unretained(this)));
  RegisterCommandHandler(
      devtools::Network::clearBrowserCookies::kName,
      base::Bind(
          &RendererOverridesHandler::ClearBrowserCookies,
          base::Unretained(this)));
  RegisterCommandHandler(
      devtools::Page::queryUsageAndQuota::kName,
      base::Bind(
          &RendererOverridesHandler::PageQueryUsageAndQuota,
          base::Unretained(this)));
}

RendererOverridesHandler::~RendererOverridesHandler() {}

void RendererOverridesHandler::OnClientDetached() {
}

void RendererOverridesHandler::SetRenderViewHost(
    RenderViewHostImpl* host) {
  host_ = host;
}

void RendererOverridesHandler::ClearRenderViewHost() {
  host_ = NULL;
}

// Network agent handlers  ----------------------------------------------------

scoped_refptr<DevToolsProtocol::Response>
RendererOverridesHandler::CanEmulateNetworkConditions(
    scoped_refptr<DevToolsProtocol::Command> command) {
  base::DictionaryValue* result = new base::DictionaryValue();
  result->SetBoolean(devtools::kResult, false);
  return command->SuccessResponse(result);
}

scoped_refptr<DevToolsProtocol::Response>
RendererOverridesHandler::ClearBrowserCache(
    scoped_refptr<DevToolsProtocol::Command> command) {
  GetContentClient()->browser()->ClearCache(host_);
  return command->SuccessResponse(NULL);
}

scoped_refptr<DevToolsProtocol::Response>
RendererOverridesHandler::ClearBrowserCookies(
    scoped_refptr<DevToolsProtocol::Command> command) {
  GetContentClient()->browser()->ClearCookies(host_);
  return command->SuccessResponse(NULL);
}


// Quota and Usage ------------------------------------------

namespace {

typedef base::Callback<void(scoped_ptr<base::DictionaryValue>)>
    ResponseCallback;

void QueryUsageAndQuotaCompletedOnIOThread(
    scoped_ptr<base::DictionaryValue> quota,
    scoped_ptr<base::DictionaryValue> usage,
    ResponseCallback callback) {

  scoped_ptr<base::DictionaryValue> response_data(new base::DictionaryValue);
  response_data->Set(devtools::Page::queryUsageAndQuota::kResponseQuota,
                     quota.release());
  response_data->Set(devtools::Page::queryUsageAndQuota::kResponseUsage,
                     usage.release());

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(callback, base::Passed(&response_data)));
}

void DidGetHostUsage(
    base::ListValue* list,
    const std::string& client_id,
    const base::Closure& barrier,
    int64 value) {
  base::DictionaryValue* usage_item = new base::DictionaryValue;
  usage_item->SetString(devtools::Page::UsageItem::kParamId, client_id);
  usage_item->SetDouble(devtools::Page::UsageItem::kParamValue, value);
  list->Append(usage_item);
  barrier.Run();
}

void DidGetQuotaValue(base::DictionaryValue* dictionary,
                      const std::string& item_name,
                      const base::Closure& barrier,
                      storage::QuotaStatusCode status,
                      int64 value) {
  if (status == storage::kQuotaStatusOk)
    dictionary->SetDouble(item_name, value);
  barrier.Run();
}

void DidGetUsageAndQuotaForWebApps(base::DictionaryValue* quota,
                                   const std::string& item_name,
                                   const base::Closure& barrier,
                                   storage::QuotaStatusCode status,
                                   int64 used_bytes,
                                   int64 quota_in_bytes) {
  if (status == storage::kQuotaStatusOk)
    quota->SetDouble(item_name, quota_in_bytes);
  barrier.Run();
}

std::string GetStorageTypeName(storage::StorageType type) {
  switch (type) {
    case storage::kStorageTypeTemporary:
      return devtools::Page::Usage::kParamTemporary;
    case storage::kStorageTypePersistent:
      return devtools::Page::Usage::kParamPersistent;
    case storage::kStorageTypeSyncable:
      return devtools::Page::Usage::kParamSyncable;
    case storage::kStorageTypeQuotaNotManaged:
    case storage::kStorageTypeUnknown:
      NOTREACHED();
  }
  return "";
}

std::string GetQuotaClientName(storage::QuotaClient::ID id) {
  switch (id) {
    case storage::QuotaClient::kFileSystem:
      return devtools::Page::UsageItem::Id::kEnumFilesystem;
    case storage::QuotaClient::kDatabase:
      return devtools::Page::UsageItem::Id::kEnumDatabase;
    case storage::QuotaClient::kAppcache:
      return devtools::Page::UsageItem::Id::kEnumAppcache;
    case storage::QuotaClient::kIndexedDatabase:
      return devtools::Page::UsageItem::Id::kEnumIndexeddatabase;
    default:
      NOTREACHED();
  }
  return "";
}

void QueryUsageAndQuotaOnIOThread(
    scoped_refptr<storage::QuotaManager> quota_manager,
    const GURL& security_origin,
    const ResponseCallback& callback) {
  scoped_ptr<base::DictionaryValue> quota(new base::DictionaryValue);
  scoped_ptr<base::DictionaryValue> usage(new base::DictionaryValue);

  static storage::QuotaClient::ID kQuotaClients[] = {
      storage::QuotaClient::kFileSystem, storage::QuotaClient::kDatabase,
      storage::QuotaClient::kAppcache, storage::QuotaClient::kIndexedDatabase};

  static const size_t kStorageTypeCount = storage::kStorageTypeUnknown;
  std::map<storage::StorageType, base::ListValue*> storage_type_lists;

  for (size_t i = 0; i != kStorageTypeCount; i++) {
    const storage::StorageType type = static_cast<storage::StorageType>(i);
    if (type == storage::kStorageTypeQuotaNotManaged)
      continue;
    storage_type_lists[type] = new base::ListValue;
    usage->Set(GetStorageTypeName(type), storage_type_lists[type]);
  }

  const int kExpectedResults =
      2 + arraysize(kQuotaClients) * storage_type_lists.size();
  base::DictionaryValue* quota_raw_ptr = quota.get();

  // Takes ownership on usage and quota.
  base::Closure barrier = BarrierClosure(
      kExpectedResults,
      base::Bind(&QueryUsageAndQuotaCompletedOnIOThread,
                 base::Passed(&quota),
                 base::Passed(&usage),
                 callback));
  std::string host = net::GetHostOrSpecFromURL(security_origin);

  quota_manager->GetUsageAndQuotaForWebApps(
      security_origin,
      storage::kStorageTypeTemporary,
      base::Bind(&DidGetUsageAndQuotaForWebApps,
                 quota_raw_ptr,
                 std::string(devtools::Page::Quota::kParamTemporary),
                 barrier));

  quota_manager->GetPersistentHostQuota(
      host,
      base::Bind(&DidGetQuotaValue, quota_raw_ptr,
                 std::string(devtools::Page::Quota::kParamPersistent),
                 barrier));

  for (size_t i = 0; i != arraysize(kQuotaClients); i++) {
    std::map<storage::StorageType, base::ListValue*>::const_iterator iter;
    for (iter = storage_type_lists.begin();
         iter != storage_type_lists.end(); ++iter) {
      const storage::StorageType type = (*iter).first;
      if (!quota_manager->IsTrackingHostUsage(type, kQuotaClients[i])) {
        barrier.Run();
        continue;
      }
      quota_manager->GetHostUsage(
          host, type, kQuotaClients[i],
          base::Bind(&DidGetHostUsage, (*iter).second,
                     GetQuotaClientName(kQuotaClients[i]),
                     barrier));
    }
  }
}

} // namespace

scoped_refptr<DevToolsProtocol::Response>
RendererOverridesHandler::PageQueryUsageAndQuota(
    scoped_refptr<DevToolsProtocol::Command> command) {
  base::DictionaryValue* params = command->params();
  std::string security_origin;
  if (!params || !params->GetString(
      devtools::Page::queryUsageAndQuota::kParamSecurityOrigin,
      &security_origin)) {
    return command->InvalidParamResponse(
        devtools::Page::queryUsageAndQuota::kParamSecurityOrigin);
  }

  ResponseCallback callback = base::Bind(
      &RendererOverridesHandler::PageQueryUsageAndQuotaCompleted,
      weak_factory_.GetWeakPtr(),
      command);

  if (!host_)
    return command->InternalErrorResponse("Could not connect to view");

  scoped_refptr<storage::QuotaManager> quota_manager =
      host_->GetProcess()->GetStoragePartition()->GetQuotaManager();

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(
          &QueryUsageAndQuotaOnIOThread,
          quota_manager,
          GURL(security_origin),
          callback));

  return command->AsyncResponsePromise();
}

void RendererOverridesHandler::PageQueryUsageAndQuotaCompleted(
    scoped_refptr<DevToolsProtocol::Command> command,
    scoped_ptr<base::DictionaryValue> response_data) {
  SendAsyncResponse(command->SuccessResponse(response_data.release()));
}

}  // namespace content
