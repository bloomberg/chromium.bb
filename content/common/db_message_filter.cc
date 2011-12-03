// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/db_message_filter.h"

#include "content/common/database_messages.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDatabase.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"

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

void DBMessageFilter::OnDatabaseUpdateSize(const string16& origin_identifier,
                                           const string16& database_name,
                                           int64 database_size) {
  WebKit::WebDatabase::updateDatabaseSize(
      origin_identifier, database_name, database_size);
}

void DBMessageFilter::OnDatabaseUpdateSpaceAvailable(
    const string16& origin_identifier,
    int64 space_available) {
  WebKit::WebDatabase::updateSpaceAvailable(
      origin_identifier, space_available);
}

void DBMessageFilter::OnDatabaseResetSpaceAvailable(
    const string16& origin_identifier) {
  WebKit::WebDatabase::resetSpaceAvailable(origin_identifier);
}

void DBMessageFilter::OnDatabaseCloseImmediately(
    const string16& origin_identifier,
    const string16& database_name) {
  WebKit::WebDatabase::closeDatabaseImmediately(
      origin_identifier, database_name);
}
