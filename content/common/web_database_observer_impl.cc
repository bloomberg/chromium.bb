// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/web_database_observer_impl.h"

#include "base/metrics/histogram.h"
#include "base/string16.h"
#include "content/common/database_messages.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDatabase.h"
#include "third_party/sqlite/sqlite3.h"

using WebKit::WebDatabase;

namespace content {
namespace {

const int kResultHistogramSize = 50;
const int kCallsiteHistogramSize = 10;

int DetermineHistogramResult(int websql_error, int sqlite_error) {
  // If we have a sqlite error, log it after trimming the extended bits.
  // There are 26 possible values, but we leave room for some new ones.
  if (sqlite_error)
    return std::min(sqlite_error & 0xff, 30);

  // Otherwise, websql_error may be an SQLExceptionCode, SQLErrorCode
  // or a DOMExceptionCode, or -1 for success.
  if (websql_error == -1)
    return 0;  // no error

  // SQLExceptionCode starts at 1000
  if (websql_error >= 1000)
    websql_error -= 1000;

  return std::min(websql_error + 30, kResultHistogramSize - 1);
}

#define HISTOGRAM_WEBSQL_RESULT(name, database, callsite, \
                                websql_error, sqlite_error) \
  do { \
    DCHECK(callsite < kCallsiteHistogramSize); \
    int result = DetermineHistogramResult(websql_error, sqlite_error); \
    if (database.isSyncDatabase()) { \
      UMA_HISTOGRAM_ENUMERATION("websql.Sync." name, \
                                result, kResultHistogramSize); \
      if (result) { \
        UMA_HISTOGRAM_ENUMERATION("websql.Sync." name ".ErrorSite", \
                                  callsite, kCallsiteHistogramSize); \
      } \
    } else { \
      UMA_HISTOGRAM_ENUMERATION("websql.Async." name, \
                                result, kResultHistogramSize); \
      if (result) { \
        UMA_HISTOGRAM_ENUMERATION("websql.Async." name ".ErrorSite", \
                                  callsite, kCallsiteHistogramSize); \
      } \
    } \
  } while (0)

}  // namespace

WebDatabaseObserverImpl::WebDatabaseObserverImpl(
    IPC::SyncMessageFilter* sender)
    : sender_(sender),
      open_connections_(new webkit_database::DatabaseConnectionsWrapper) {
  DCHECK(sender);
}

WebDatabaseObserverImpl::~WebDatabaseObserverImpl() {
}

void WebDatabaseObserverImpl::databaseOpened(
    const WebDatabase& database) {
  string16 origin_identifier = database.securityOrigin().databaseIdentifier();
  string16 database_name = database.name();
  open_connections_->AddOpenConnection(origin_identifier, database_name);
  sender_->Send(new DatabaseHostMsg_Opened(
      origin_identifier, database_name,
      database.displayName(), database.estimatedSize()));
}

void WebDatabaseObserverImpl::databaseModified(
    const WebDatabase& database) {
  sender_->Send(new DatabaseHostMsg_Modified(
      database.securityOrigin().databaseIdentifier(), database.name()));
}

void WebDatabaseObserverImpl::databaseClosed(
    const WebDatabase& database) {
  string16 origin_identifier = database.securityOrigin().databaseIdentifier();
  string16 database_name = database.name();
  sender_->Send(new DatabaseHostMsg_Closed(
      origin_identifier, database_name));
  open_connections_->RemoveOpenConnection(origin_identifier, database_name);
}

void WebDatabaseObserverImpl::reportOpenDatabaseResult(
    const WebDatabase& database, int callsite, int websql_error,
    int sqlite_error) {
  HISTOGRAM_WEBSQL_RESULT("OpenResult", database, callsite,
                          websql_error, sqlite_error);
  HandleSqliteError(database, sqlite_error);
}

void WebDatabaseObserverImpl::reportChangeVersionResult(
    const WebDatabase& database, int callsite, int websql_error,
    int sqlite_error) {
  HISTOGRAM_WEBSQL_RESULT("ChangeVersionResult", database, callsite,
                          websql_error, sqlite_error);
  HandleSqliteError(database, sqlite_error);
}

void WebDatabaseObserverImpl::reportStartTransactionResult(
    const WebDatabase& database, int callsite, int websql_error,
    int sqlite_error) {
  HISTOGRAM_WEBSQL_RESULT("BeginResult", database, callsite,
                          websql_error, sqlite_error);
  HandleSqliteError(database, sqlite_error);
}

void WebDatabaseObserverImpl::reportCommitTransactionResult(
    const WebDatabase& database, int callsite, int websql_error,
    int sqlite_error) {
  HISTOGRAM_WEBSQL_RESULT("CommitResult", database, callsite,
                          websql_error, sqlite_error);
  HandleSqliteError(database, sqlite_error);
}

void WebDatabaseObserverImpl::reportExecuteStatementResult(
    const WebDatabase& database, int callsite, int websql_error,
    int sqlite_error) {
  HISTOGRAM_WEBSQL_RESULT("StatementResult", database, callsite,
                          websql_error, sqlite_error);
  HandleSqliteError(database, sqlite_error);
}

void WebDatabaseObserverImpl::reportVacuumDatabaseResult(
    const WebDatabase& database, int sqlite_error) {
  int result = DetermineHistogramResult(-1, sqlite_error);
  if (database.isSyncDatabase()) {
    UMA_HISTOGRAM_ENUMERATION("websql.Sync.VacuumResult",
                              result, kResultHistogramSize);
  } else {
    UMA_HISTOGRAM_ENUMERATION("websql.Async.VacuumResult",
                              result, kResultHistogramSize);
  }
  HandleSqliteError(database, sqlite_error);
}

void WebDatabaseObserverImpl::WaitForAllDatabasesToClose() {
  open_connections_->WaitForAllDatabasesToClose();
}

void WebDatabaseObserverImpl::HandleSqliteError(
    const WebDatabase& database, int error) {
  // We filter out errors which the backend doesn't act on to avoid
  // a unnecessary ipc traffic, this method can get called at a fairly
  // high frequency (per-sqlstatement).
  if (error == SQLITE_CORRUPT || error == SQLITE_NOTADB) {
    sender_->Send(new DatabaseHostMsg_HandleSqliteError(
        database.securityOrigin().databaseIdentifier(),
        database.name(),
        error));
  }
}

}  // namespace content
