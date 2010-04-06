// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/db_message_filter.h"

#include "chrome/common/render_messages.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDatabase.h"

DBMessageFilter::DBMessageFilter() {
}

bool DBMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DBMessageFilter, message)
    IPC_MESSAGE_HANDLER(ViewMsg_DatabaseUpdateSize, OnDatabaseUpdateSize)
    IPC_MESSAGE_HANDLER(ViewMsg_DatabaseCloseImmediately,
                        OnDatabaseCloseImmediately)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void DBMessageFilter::OnDatabaseUpdateSize(const string16& origin_identifier,
                                           const string16& database_name,
                                           int64 database_size,
                                           int64 space_available) {
  WebKit::WebDatabase::updateDatabaseSize(
      origin_identifier, database_name, database_size, space_available);
}

void DBMessageFilter::OnDatabaseCloseImmediately(
    const string16& origin_identifier,
    const string16& database_name) {
  WebKit::WebDatabase::closeDatabaseImmediately(
      origin_identifier, database_name);
}
