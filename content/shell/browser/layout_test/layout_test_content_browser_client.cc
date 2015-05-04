// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/layout_test/layout_test_content_browser_client.h"

#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigator_connect_context.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/shell/browser/layout_test/layout_test_browser_context.h"
#include "content/shell/browser/layout_test/layout_test_message_filter.h"
#include "content/shell/browser/layout_test/layout_test_navigator_connect_service_factory.h"
#include "content/shell/browser/layout_test/layout_test_notification_manager.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/common/shell_messages.h"
#include "content/shell/renderer/layout_test/blink_test_helpers.h"

namespace content {
namespace {

LayoutTestContentBrowserClient* g_layout_test_browser_client;

}  // namespace

LayoutTestContentBrowserClient::LayoutTestContentBrowserClient() {
  DCHECK(!g_layout_test_browser_client);

  layout_test_notification_manager_.reset(new LayoutTestNotificationManager());

  g_layout_test_browser_client = this;
}

LayoutTestContentBrowserClient::~LayoutTestContentBrowserClient() {
  g_layout_test_browser_client = nullptr;
}

LayoutTestContentBrowserClient* LayoutTestContentBrowserClient::Get() {
  return g_layout_test_browser_client;
}

LayoutTestBrowserContext*
LayoutTestContentBrowserClient::GetLayoutTestBrowserContext() {
  return static_cast<LayoutTestBrowserContext*>(browser_context());
}

LayoutTestNotificationManager*
LayoutTestContentBrowserClient::GetLayoutTestNotificationManager() {
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

PlatformNotificationService*
LayoutTestContentBrowserClient::GetPlatformNotificationService() {
  return layout_test_notification_manager_.get();
}

void LayoutTestContentBrowserClient::GetAdditionalNavigatorConnectServices(
    const scoped_refptr<NavigatorConnectContext>& context) {
  context->AddFactory(
      make_scoped_ptr(new LayoutTestNavigatorConnectServiceFactory));
}

}  // namespace content
