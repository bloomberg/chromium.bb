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
#include "url/origin.h"

using blink::WebSecurityOrigin;
using blink::WebString;

namespace content {

DBMessageFilter::DBMessageFilter() {
}

bool DBMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DBMessageFilter, message)
    IPC_MESSAGE_HANDLER(DatabaseMsg_UpdateSize, OnDatabaseUpdateSize)
    IPC_MESSAGE_HANDLER(DatabaseMsg_CloseImmediately,
                        OnDatabaseCloseImmediately)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void DBMessageFilter::OnDatabaseUpdateSize(const url::Origin& origin,
                                           const base::string16& database_name,
                                           int64_t database_size) {
  DCHECK(!origin.unique());
  blink::WebDatabase::UpdateDatabaseSize(
      origin, WebString::FromUTF16(database_name), database_size);
}

void DBMessageFilter::OnDatabaseCloseImmediately(
    const url::Origin& origin,
    const base::string16& database_name) {
  DCHECK(!origin.unique());
  blink::WebDatabase::CloseDatabaseImmediately(
      origin, WebString::FromUTF16(database_name));
}

}  // namespace content
