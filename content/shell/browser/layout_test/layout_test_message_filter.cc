// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/layout_test/layout_test_message_filter.h"

#include <stddef.h>

#include "base/files/file_util.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/permission_type.h"
#include "content/public/test/layouttest_support.h"
#include "content/shell/browser/layout_test/blink_test_controller.h"
#include "content/shell/browser/layout_test/layout_test_browser_context.h"
#include "content/shell/browser/layout_test/layout_test_content_browser_client.h"
#include "content/shell/browser/layout_test/layout_test_notification_manager.h"
#include "content/shell/browser/layout_test/layout_test_permission_manager.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/shell/browser/shell_network_delegate.h"
#include "content/shell/common/layout_test/layout_test_messages.h"
#include "net/base/net_errors.h"
#include "net/cookies/cookie_store.h"
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
  switch (message.type()) {
    case LayoutTestHostMsg_ClearAllDatabases::ID:
      *thread = BrowserThread::FILE;
      break;
    case LayoutTestHostMsg_SimulateWebNotificationClick::ID:
    case LayoutTestHostMsg_SimulateWebNotificationClose::ID:
    case LayoutTestHostMsg_SetPermission::ID:
    case LayoutTestHostMsg_ResetPermissions::ID:
    case LayoutTestHostMsg_LayoutTestRuntimeFlagsChanged::ID:
    case LayoutTestHostMsg_TestFinishedInSecondaryRenderer::ID:
      *thread = BrowserThread::UI;
      break;
  }
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
    IPC_MESSAGE_HANDLER(LayoutTestHostMsg_SimulateWebNotificationClick,
                        OnSimulateWebNotificationClick)
    IPC_MESSAGE_HANDLER(LayoutTestHostMsg_SimulateWebNotificationClose,
                        OnSimulateWebNotificationClose)
    IPC_MESSAGE_HANDLER(LayoutTestHostMsg_AcceptAllCookies, OnAcceptAllCookies)
    IPC_MESSAGE_HANDLER(LayoutTestHostMsg_DeleteAllCookies, OnDeleteAllCookies)
    IPC_MESSAGE_HANDLER(LayoutTestHostMsg_SetPermission, OnSetPermission)
    IPC_MESSAGE_HANDLER(LayoutTestHostMsg_ResetPermissions, OnResetPermissions)
    IPC_MESSAGE_HANDLER(LayoutTestHostMsg_LayoutTestRuntimeFlagsChanged,
                        OnLayoutTestRuntimeFlagsChanged)
    IPC_MESSAGE_HANDLER(LayoutTestHostMsg_TestFinishedInSecondaryRenderer,
                        OnTestFinishedInSecondaryRenderer)
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
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  database_tracker_->DeleteDataModifiedSince(
      base::Time(), net::CompletionCallback());
}

void LayoutTestMessageFilter::OnSetDatabaseQuota(int quota) {
  quota_manager_->SetTemporaryGlobalOverrideQuota(
      quota * storage::QuotaManager::kPerHostTemporaryPortion,
      storage::QuotaCallback());
}

void LayoutTestMessageFilter::OnSimulateWebNotificationClick(
    const std::string& title, int action_index) {
  LayoutTestNotificationManager* manager =
      LayoutTestContentBrowserClient::Get()->GetLayoutTestNotificationManager();
  if (manager)
    manager->SimulateClick(title, action_index);
}

void LayoutTestMessageFilter::OnSimulateWebNotificationClose(
    const std::string& title, bool by_user) {
  LayoutTestNotificationManager* manager =
      LayoutTestContentBrowserClient::Get()->GetLayoutTestNotificationManager();
  if (manager)
    manager->SimulateClose(title, by_user);
}

void LayoutTestMessageFilter::OnAcceptAllCookies(bool accept) {
  ShellNetworkDelegate::SetAcceptAllCookies(accept);
}

void LayoutTestMessageFilter::OnDeleteAllCookies() {
  request_context_getter_->GetURLRequestContext()->cookie_store()
      ->DeleteAllAsync(net::CookieStore::DeleteCallback());
}

void LayoutTestMessageFilter::OnSetPermission(
    const std::string& name,
    permissions::mojom::PermissionStatus status,
    const GURL& origin,
    const GURL& embedding_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  content::PermissionType type;
  if (name == "midi") {
    type = PermissionType::MIDI;
  } else if (name == "midi-sysex") {
    type = PermissionType::MIDI_SYSEX;
  } else if (name == "push-messaging") {
    type = PermissionType::PUSH_MESSAGING;
  } else if (name == "notifications") {
    type = PermissionType::NOTIFICATIONS;
  } else if (name == "geolocation") {
    type = PermissionType::GEOLOCATION;
  } else if (name == "protected-media-identifier") {
    type = PermissionType::PROTECTED_MEDIA_IDENTIFIER;
  } else if (name == "background-sync") {
    type = PermissionType::BACKGROUND_SYNC;
  } else {
    NOTREACHED();
    type = PermissionType::NOTIFICATIONS;
  }

  LayoutTestContentBrowserClient::Get()
      ->GetLayoutTestBrowserContext()
      ->GetLayoutTestPermissionManager()
      ->SetPermission(type, status, origin, embedding_origin);
}

void LayoutTestMessageFilter::OnResetPermissions() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  LayoutTestContentBrowserClient::Get()
      ->GetLayoutTestBrowserContext()
      ->GetLayoutTestPermissionManager()
      ->ResetPermissions();
}

void LayoutTestMessageFilter::OnLayoutTestRuntimeFlagsChanged(
    const base::DictionaryValue& changed_layout_test_runtime_flags) {
  BlinkTestController::Get()->OnLayoutTestRuntimeFlagsChanged(
      render_process_id_, changed_layout_test_runtime_flags);
}

void LayoutTestMessageFilter::OnTestFinishedInSecondaryRenderer() {
  BlinkTestController::Get()->OnTestFinishedInSecondaryRenderer();
}

}  // namespace content
