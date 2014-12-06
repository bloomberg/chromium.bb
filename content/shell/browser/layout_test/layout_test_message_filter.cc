// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/layout_test/layout_test_message_filter.h"

#include "base/files/file_util.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/shell/browser/layout_test/layout_test_browser_context.h"
#include "content/shell/browser/layout_test/layout_test_content_browser_client.h"
#include "content/shell/browser/layout_test/layout_test_notification_manager.h"
#include "content/shell/browser/layout_test/layout_test_push_messaging_service.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/shell/browser/shell_network_delegate.h"
#include "content/shell/common/layout_test/layout_test_messages.h"
#include "net/base/net_errors.h"
#include "net/cookies/cookie_monster.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "storage/browser/database/database_tracker.h"
#include "storage/browser/fileapi/isolated_context.h"
#include "storage/browser/quota/quota_manager.h"

namespace content {

LayoutTestMessageFilter::LayoutTestMessageFilter(
    int render_process_id,
    storage::DatabaseTracker* database_tracker,
    storage::QuotaManager* quota_manager,
    net::URLRequestContextGetter* request_context_getter)
    : BrowserMessageFilter(LayoutTestMsgStart),
      render_process_id_(render_process_id),
      database_tracker_(database_tracker),
      quota_manager_(quota_manager),
      request_context_getter_(request_context_getter) {
}

LayoutTestMessageFilter::~LayoutTestMessageFilter() {
}

void LayoutTestMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message, BrowserThread::ID* thread) {
  if (message.type() == LayoutTestHostMsg_ClearAllDatabases::ID)
    *thread = BrowserThread::FILE;
  if (message.type() == LayoutTestHostMsg_SimulateWebNotificationClick::ID ||
      message.type() == LayoutTestHostMsg_SetPushMessagingPermission::ID ||
      message.type() == LayoutTestHostMsg_ClearPushMessagingPermissions::ID)
    *thread = BrowserThread::UI;
}

bool LayoutTestMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(LayoutTestMessageFilter, message)
    IPC_MESSAGE_HANDLER(LayoutTestHostMsg_ReadFileToString, OnReadFileToString)
    IPC_MESSAGE_HANDLER(LayoutTestHostMsg_RegisterIsolatedFileSystem,
                        OnRegisterIsolatedFileSystem)
    IPC_MESSAGE_HANDLER(LayoutTestHostMsg_ClearAllDatabases,
                        OnClearAllDatabases)
    IPC_MESSAGE_HANDLER(LayoutTestHostMsg_SetDatabaseQuota, OnSetDatabaseQuota)
    IPC_MESSAGE_HANDLER(LayoutTestHostMsg_GrantWebNotificationPermission,
                        OnGrantWebNotificationPermission)
    IPC_MESSAGE_HANDLER(LayoutTestHostMsg_ClearWebNotificationPermissions,
                        OnClearWebNotificationPermissions)
    IPC_MESSAGE_HANDLER(LayoutTestHostMsg_SimulateWebNotificationClick,
                        OnSimulateWebNotificationClick)
    IPC_MESSAGE_HANDLER(LayoutTestHostMsg_SetPushMessagingPermission,
                        OnSetPushMessagingPermission)
    IPC_MESSAGE_HANDLER(LayoutTestHostMsg_ClearPushMessagingPermissions,
                        OnClearPushMessagingPermissions)
    IPC_MESSAGE_HANDLER(LayoutTestHostMsg_AcceptAllCookies, OnAcceptAllCookies)
    IPC_MESSAGE_HANDLER(LayoutTestHostMsg_DeleteAllCookies, OnDeleteAllCookies)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void LayoutTestMessageFilter::OnReadFileToString(
    const base::FilePath& local_file, std::string* contents) {
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  base::ReadFileToString(local_file, contents);
}

void LayoutTestMessageFilter::OnRegisterIsolatedFileSystem(
    const std::vector<base::FilePath>& absolute_filenames,
    std::string* filesystem_id) {
  storage::IsolatedContext::FileInfoSet files;
  ChildProcessSecurityPolicy* policy =
      ChildProcessSecurityPolicy::GetInstance();
  for (size_t i = 0; i < absolute_filenames.size(); ++i) {
    files.AddPath(absolute_filenames[i], NULL);
    if (!policy->CanReadFile(render_process_id_, absolute_filenames[i]))
      policy->GrantReadFile(render_process_id_, absolute_filenames[i]);
  }
  *filesystem_id =
      storage::IsolatedContext::GetInstance()->RegisterDraggedFileSystem(files);
  policy->GrantReadFileSystem(render_process_id_, *filesystem_id);
}

void LayoutTestMessageFilter::OnClearAllDatabases() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  database_tracker_->DeleteDataModifiedSince(
      base::Time(), net::CompletionCallback());
}

void LayoutTestMessageFilter::OnSetDatabaseQuota(int quota) {
  quota_manager_->SetTemporaryGlobalOverrideQuota(
      quota * storage::QuotaManager::kPerHostTemporaryPortion,
      storage::QuotaCallback());
}

void LayoutTestMessageFilter::OnGrantWebNotificationPermission(
    const GURL& origin, bool permission_granted) {
  LayoutTestNotificationManager* manager =
      LayoutTestContentBrowserClient::Get()->GetLayoutTestNotificationManager();
  if (manager) {
    manager->SetPermission(origin, permission_granted ?
        blink::WebNotificationPermissionAllowed :
        blink::WebNotificationPermissionDenied);
  }
}

void LayoutTestMessageFilter::OnClearWebNotificationPermissions() {
  LayoutTestNotificationManager* manager =
      LayoutTestContentBrowserClient::Get()->GetLayoutTestNotificationManager();
  if (manager)
    manager->ClearPermissions();
}

void LayoutTestMessageFilter::OnSimulateWebNotificationClick(
    const std::string& title) {
  LayoutTestNotificationManager* manager =
      LayoutTestContentBrowserClient::Get()->GetLayoutTestNotificationManager();
  if (manager)
    manager->SimulateClick(title);
}

void LayoutTestMessageFilter::OnSetPushMessagingPermission(const GURL& origin,
                                                           bool allowed) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  LayoutTestContentBrowserClient::Get()
      ->GetLayoutTestBrowserContext()
      ->GetLayoutTestPushMessagingService()
      ->SetPermission(origin, allowed);
}

void LayoutTestMessageFilter::OnClearPushMessagingPermissions() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  LayoutTestContentBrowserClient::Get()
      ->GetLayoutTestBrowserContext()
      ->GetLayoutTestPushMessagingService()
      ->ClearPermissions();
}

void LayoutTestMessageFilter::OnAcceptAllCookies(bool accept) {
  ShellNetworkDelegate::SetAcceptAllCookies(accept);
}

void LayoutTestMessageFilter::OnDeleteAllCookies() {
  request_context_getter_->GetURLRequestContext()->cookie_store()
      ->GetCookieMonster()
      ->DeleteAllAsync(net::CookieMonster::DeleteCallback());
}

}  // namespace content
