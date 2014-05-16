// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_message_filter.h"

#include "base/file_util.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/shell/browser/shell_network_delegate.h"
#include "content/shell/common/shell_messages.h"
#include "net/base/net_errors.h"
#include "net/cookies/cookie_monster.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "webkit/browser/database/database_tracker.h"
#include "webkit/browser/fileapi/isolated_context.h"
#include "webkit/browser/quota/quota_manager.h"

namespace content {

ShellMessageFilter::ShellMessageFilter(
    int render_process_id,
    webkit_database::DatabaseTracker* database_tracker,
    quota::QuotaManager* quota_manager,
    net::URLRequestContextGetter* request_context_getter)
    : BrowserMessageFilter(ShellMsgStart),
      render_process_id_(render_process_id),
      database_tracker_(database_tracker),
      quota_manager_(quota_manager),
      request_context_getter_(request_context_getter) {
}

ShellMessageFilter::~ShellMessageFilter() {
}

void ShellMessageFilter::OverrideThreadForMessage(const IPC::Message& message,
                                                  BrowserThread::ID* thread) {
  if (message.type() == ShellViewHostMsg_ClearAllDatabases::ID)
    *thread = BrowserThread::FILE;
}

bool ShellMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ShellMessageFilter, message)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_ReadFileToString, OnReadFileToString)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_RegisterIsolatedFileSystem,
                        OnRegisterIsolatedFileSystem)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_ClearAllDatabases, OnClearAllDatabases)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_SetDatabaseQuota, OnSetDatabaseQuota)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_AcceptAllCookies, OnAcceptAllCookies)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_DeleteAllCookies, OnDeleteAllCookies)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void ShellMessageFilter::OnReadFileToString(const base::FilePath& local_file,
                                            std::string* contents) {
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  base::ReadFileToString(local_file, contents);
}

void ShellMessageFilter::OnRegisterIsolatedFileSystem(
    const std::vector<base::FilePath>& absolute_filenames,
    std::string* filesystem_id) {
  fileapi::IsolatedContext::FileInfoSet files;
  ChildProcessSecurityPolicy* policy =
      ChildProcessSecurityPolicy::GetInstance();
  for (size_t i = 0; i < absolute_filenames.size(); ++i) {
    files.AddPath(absolute_filenames[i], NULL);
    if (!policy->CanReadFile(render_process_id_, absolute_filenames[i]))
      policy->GrantReadFile(render_process_id_, absolute_filenames[i]);
  }
  *filesystem_id =
      fileapi::IsolatedContext::GetInstance()->RegisterDraggedFileSystem(files);
  policy->GrantReadFileSystem(render_process_id_, *filesystem_id);
}

void ShellMessageFilter::OnClearAllDatabases() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  database_tracker_->DeleteDataModifiedSince(
      base::Time(), net::CompletionCallback());
}

void ShellMessageFilter::OnSetDatabaseQuota(int quota) {
  quota_manager_->SetTemporaryGlobalOverrideQuota(
      quota * quota::QuotaManager::kPerHostTemporaryPortion,
      quota::QuotaCallback());
}

void ShellMessageFilter::OnAcceptAllCookies(bool accept) {
  ShellNetworkDelegate::SetAcceptAllCookies(accept);
}

void ShellMessageFilter::OnDeleteAllCookies() {
  request_context_getter_->GetURLRequestContext()->cookie_store()
      ->GetCookieMonster()
      ->DeleteAllAsync(net::CookieMonster::DeleteCallback());
}

}  // namespace content
