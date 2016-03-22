// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/db_message_filter.h"

#include "content/common/database_messages.h"
#include "storage/common/database/database_identifier.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebDatabase.h"

using blink::WebSecurityOrigin;
using blink::WebString;

namespace content {

namespace {

// TODO(jsbell): Pass url::Origin over IPC instead of database identifier/GURL.
// https://crbug.com/591482
WebSecurityOrigin OriginFromIdentifier(const std::string& identifier) {
  return WebSecurityOrigin::create(
      storage::GetOriginFromIdentifier(identifier));
}

}  // namespace

DBMessageFilter::DBMessageFilter() {
}

bool DBMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DBMessageFilter, message)
    IPC_MESSAGE_HANDLER(DatabaseMsg_UpdateSize, OnDatabaseUpdateSize)
    IPC_MESSAGE_HANDLER(DatabaseMsg_UpdateSpaceAvailable,
                        OnDatabaseUpdateSpaceAvailable)
    IPC_MESSAGE_HANDLER(DatabaseMsg_ResetSpaceAvailable,
                        OnDatabaseResetSpaceAvailable)
    IPC_MESSAGE_HANDLER(DatabaseMsg_CloseImmediately,
                        OnDatabaseCloseImmediately)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void DBMessageFilter::OnDatabaseUpdateSize(const std::string& origin_identifier,
                                           const base::string16& database_name,
                                           int64_t database_size) {
  blink::WebDatabase::updateDatabaseSize(
      OriginFromIdentifier(origin_identifier), database_name, database_size);
}

void DBMessageFilter::OnDatabaseUpdateSpaceAvailable(
    const std::string& origin_identifier,
    int64_t space_available) {
  blink::WebDatabase::updateSpaceAvailable(
      OriginFromIdentifier(origin_identifier), space_available);
}

void DBMessageFilter::OnDatabaseResetSpaceAvailable(
    const std::string& origin_identifier) {
  blink::WebDatabase::resetSpaceAvailable(
      OriginFromIdentifier(origin_identifier));
}

void DBMessageFilter::OnDatabaseCloseImmediately(
    const std::string& origin_identifier,
    const base::string16& database_name) {
  blink::WebDatabase::closeDatabaseImmediately(
      OriginFromIdentifier(origin_identifier), database_name);
}

}  // namespace content
