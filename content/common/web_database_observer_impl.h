// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_WEB_DATABASE_OBSERVER_IMPL_H_
#define CONTENT_COMMON_WEB_DATABASE_OBSERVER_IMPL_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "ipc/ipc_message.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDatabaseObserver.h"
#include "webkit/database/database_connections.h"

class WebDatabaseObserverImpl : public WebKit::WebDatabaseObserver {
 public:
  explicit WebDatabaseObserverImpl(IPC::Message::Sender* sender);
  virtual ~WebDatabaseObserverImpl();

  virtual void databaseOpened(const WebKit::WebDatabase& database) OVERRIDE;
  virtual void databaseModified(const WebKit::WebDatabase& database) OVERRIDE;
  virtual void databaseClosed(const WebKit::WebDatabase& database) OVERRIDE;

  virtual void reportOpenDatabaseResult(
      const WebKit::WebDatabase& database, int callsite,
      int webSqlErrorCode, int sqliteErrorCode) OVERRIDE;
  virtual void reportChangeVersionResult(
      const WebKit::WebDatabase& database, int callsite,
      int webSqlErrorCode, int sqliteErrorCode) OVERRIDE;
  virtual void reportStartTransactionResult(
      const WebKit::WebDatabase& database, int callsite,
      int webSqlErrorCode, int sqliteErrorCode) OVERRIDE;
  virtual void reportCommitTransactionResult(
      const WebKit::WebDatabase& database, int callsite,
      int webSqlErrorCode, int sqliteErrorCode) OVERRIDE;
  virtual void reportExecuteStatementResult(
      const WebKit::WebDatabase& database, int callsite,
      int webSqlErrorCode, int sqliteErrorCode) OVERRIDE;
  virtual void reportVacuumDatabaseResult(
      const WebKit::WebDatabase& database, int sqliteErrorCode) OVERRIDE;

  void WaitForAllDatabasesToClose();

 private:
  IPC::Message::Sender* sender_;
  scoped_refptr<webkit_database::DatabaseConnectionsWrapper> open_connections_;
};

#endif  // CONTENT_COMMON_WEB_DATABASE_OBSERVER_IMPL_H_
