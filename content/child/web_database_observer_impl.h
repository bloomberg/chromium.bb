// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEB_DATABASE_OBSERVER_IMPL_H_
#define CONTENT_CHILD_WEB_DATABASE_OBSERVER_IMPL_H_

#include "base/memory/ref_counted.h"
#include "ipc/ipc_sync_message_filter.h"
#include "third_party/WebKit/public/web/WebDatabaseObserver.h"
#include "webkit/common/database/database_connections.h"

namespace blink {
class WebString;
}

namespace content {

class WebDatabaseObserverImpl : public blink::WebDatabaseObserver {
 public:
  explicit WebDatabaseObserverImpl(IPC::SyncMessageFilter* sender);
  virtual ~WebDatabaseObserverImpl();

  virtual void databaseOpened(const blink::WebDatabase& database) OVERRIDE;
  virtual void databaseModified(const blink::WebDatabase& database) OVERRIDE;
  virtual void databaseClosed(const blink::WebString& origin_identifier,
                              const blink::WebString& database_name);
  // TODO(jochen): Remove this version once the blink side has rolled.
  virtual void databaseClosed(const blink::WebDatabase& database);

  virtual void reportOpenDatabaseResult(
      const blink::WebDatabase& database, int callsite,
      int websql_error, int sqlite_error) OVERRIDE;
  virtual void reportChangeVersionResult(
      const blink::WebDatabase& database, int callsite,
      int websql_error, int sqlite_error) OVERRIDE;
  virtual void reportStartTransactionResult(
      const blink::WebDatabase& database, int callsite,
      int websql_error, int sqlite_error) OVERRIDE;
  virtual void reportCommitTransactionResult(
      const blink::WebDatabase& database, int callsite,
      int websql_error, int sqlite_error) OVERRIDE;
  virtual void reportExecuteStatementResult(
      const blink::WebDatabase& database, int callsite,
      int websql_error, int sqlite_error) OVERRIDE;
  virtual void reportVacuumDatabaseResult(
      const blink::WebDatabase& database, int sqlite_error) OVERRIDE;

  void WaitForAllDatabasesToClose();

 private:
  void HandleSqliteError(const blink::WebDatabase& database, int error);

  scoped_refptr<IPC::SyncMessageFilter> sender_;
  scoped_refptr<webkit_database::DatabaseConnectionsWrapper> open_connections_;
};

}  // namespace content

#endif  // CONTENT_CHILD_WEB_DATABASE_OBSERVER_IMPL_H_
