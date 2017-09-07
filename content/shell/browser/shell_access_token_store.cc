// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_access_token_store.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/shell/browser/shell_browser_context.h"

namespace content {

ShellAccessTokenStore::ShellAccessTokenStore(
    content::ShellBrowserContext* shell_browser_context)
    : shell_browser_context_(shell_browser_context),
      system_request_context_(NULL) {
}

ShellAccessTokenStore::~ShellAccessTokenStore() {
}

void ShellAccessTokenStore::LoadAccessTokens(
    const LoadAccessTokensCallback& callback) {
  BrowserThread::PostTaskAndReply(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&ShellAccessTokenStore::GetRequestContextOnUIThread, this,
                     shell_browser_context_),
      base::BindOnce(&ShellAccessTokenStore::RespondOnOriginatingThread, this,
                     callback));
}

void ShellAccessTokenStore::GetRequestContextOnUIThread(
    content::ShellBrowserContext* shell_browser_context) {
  system_request_context_ =
      BrowserContext::GetDefaultStoragePartition(shell_browser_context)->
          GetURLRequestContext();
}

void ShellAccessTokenStore::RespondOnOriginatingThread(
    const LoadAccessTokensCallback& callback) {
  callback.Run(AccessTokenMap(), system_request_context_);
  system_request_context_ = NULL;
}

void ShellAccessTokenStore::SaveAccessToken(
    const GURL& server_url, const base::string16& access_token) {
}

}  // namespace content
