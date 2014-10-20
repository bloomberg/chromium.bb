// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/layout_test/layout_test_content_browser_client.h"

#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/shell/browser/layout_test/layout_test_message_filter.h"
#include "content/shell/browser/layout_test/layout_test_notification_manager.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/common/shell_messages.h"
#include "content/shell/common/webkit_test_helpers.h"

namespace content {
namespace {

LayoutTestContentBrowserClient* g_layout_test_browser_client;

void RequestDesktopNotificationPermissionOnIO(
    const GURL& source_origin,
    RenderFrameHost* render_frame_host,
    const base::Callback<void(blink::WebNotificationPermission)>& callback) {
  LayoutTestNotificationManager* manager =
      LayoutTestContentBrowserClient::Get()->GetLayoutTestNotificationManager();
  if (manager)
    manager->RequestPermission(source_origin, callback);
  else
    callback.Run(blink::WebNotificationPermissionAllowed);
}

}  // namespace

LayoutTestContentBrowserClient::LayoutTestContentBrowserClient() {
  DCHECK(!g_layout_test_browser_client);
  g_layout_test_browser_client = this;
}

LayoutTestContentBrowserClient::~LayoutTestContentBrowserClient() {
  g_layout_test_browser_client = nullptr;
}

LayoutTestContentBrowserClient* LayoutTestContentBrowserClient::Get() {
  return g_layout_test_browser_client;
}

LayoutTestNotificationManager*
LayoutTestContentBrowserClient::GetLayoutTestNotificationManager() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!layout_test_notification_manager_) {
    layout_test_notification_manager_.reset(
        new LayoutTestNotificationManager());
  }

  return layout_test_notification_manager_.get();
}

void LayoutTestContentBrowserClient::RenderProcessWillLaunch(
    RenderProcessHost* host) {
  ShellContentBrowserClient::RenderProcessWillLaunch(host);

  StoragePartition* partition =
      BrowserContext::GetDefaultStoragePartition(browser_context());
  host->AddFilter(new LayoutTestMessageFilter(
      host->GetID(),
      partition->GetDatabaseTracker(),
      partition->GetQuotaManager(),
      partition->GetURLRequestContext()));

  host->Send(new ShellViewMsg_SetWebKitSourceDir(GetWebKitRootDirFilePath()));
}

void LayoutTestContentBrowserClient::RequestDesktopNotificationPermission(
    const GURL& source_origin,
    RenderFrameHost* render_frame_host,
    const base::Callback<void(blink::WebNotificationPermission)>& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(&RequestDesktopNotificationPermissionOnIO,
                                     source_origin,
                                     render_frame_host,
                                     callback));
}

blink::WebNotificationPermission
LayoutTestContentBrowserClient::CheckDesktopNotificationPermission(
    const GURL& source_url,
    ResourceContext* context,
    int render_process_id) {
  LayoutTestNotificationManager* manager = GetLayoutTestNotificationManager();
  if (manager)
    return manager->CheckPermission(source_url);

  return blink::WebNotificationPermissionAllowed;
}

}  // namespace content
