// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/util/query_helpers.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#include <limits>
#include <string>
#include <vector>

#include "chrome/browser/sync/util/sync_types.h"

using std::numeric_limits;
using std::string;
using std::vector;

sqlite3_stmt* PrepareQuery(sqlite3* dbhandle, const char* query) {
  sqlite3_stmt* statement = NULL;
  const char* query_tail;
  if (SQLITE_OK != sqlite3_prepare(dbhandle, query,
                                   CountBytes(query), &statement,
                                   &query_tail)) {
    LOG(ERROR) << query << "\n" << sqlite3_errmsg(dbhandle);
  }
  return statement;
}

void ExecOrDie(sqlite3* dbhandle, const char* query) {
  return ExecOrDie(dbhandle, query, PrepareQuery(dbhandle, query));
}

// Finalizes (deletes) the query before returning.
void ExecOrDie(sqlite3* dbhandle, const char* query, sqlite3_stmt* statement) {
  int result = Exec(dbhandle, query, statement);
  if (SQLITE_DONE != result) {
    LOG(FATAL) << query << "\n" << sqlite3_errmsg(dbhandle);
  }
}

int Exec(sqlite3* dbhandle, const char* query) {
  return Exec(dbhandle, query, PrepareQuery(dbhandle, query));
}

// Finalizes (deletes) the query before returning.
int Exec(sqlite3* dbhandle, const char* query, sqlite3_stmt* statement) {
  int result;
  do {
    result = sqlite3_step(statement);
  } while (SQLITE_ROW == result);
  int finalize_result = sqlite3_finalize(statement);
  return SQLITE_OK == finalize_result ? result : finalize_result;
}

int SqliteOpen(PathString filename, sqlite3** db) {
  int result =
#if PATHSTRING_IS_STD_STRING
  sqlite3_open
#else
  sqlite3_open16
#endif
  (filename.c_str(), db);
  LOG_IF(ERROR, SQLITE_OK != result) << "Error opening " << filename << ": "
                                     << result;
#if defined(OS_WIN)
  if (SQLITE_OK == result) {
    // Make sure we mark the db file as not indexed so since if any other app
    // opens it, it can break our db locking.
    DWORD attrs = GetFileAttributes(filename.c_str());
    if (FILE_ATTRIBUTE_NORMAL == attrs)
      attrs = FILE_ATTRIBUTE_NOT_CONTENT_INDEXED;
    else
      attrs = attrs | FILE_ATTRIBUTE_NOT_CONTENT_INDEXED;
    SetFileAttributes(filename.c_str(), attrs);
  }
#endif  // defined(OS_WIN)
  // Be patient as we set pragmas.
  sqlite3_busy_timeout(*db, numeric_limits<int>::max());
#if !defined(DISABLE_SQLITE_FULL_FSYNC)
  ExecOrDie(*db, "PRAGMA fullfsync = 1");
#endif  // !defined(DISABLE_SQLITE_FULL_FSYNC)
  ExecOrDie(*db, "PRAGMA synchronous = 2");
  sqlite3_busy_timeout(*db, 0);
  return SQLITE_OK;
}

#if !PATHSTRING_IS_STD_STRING
sqlite3_stmt* BindArg(sqlite3_stmt* statement, const PathString& s, int index) {
  if (NULL == statement)
    return statement;
  CHECK(SQLITE_OK == sqlite3_bind_text16(statement, index, s.data(),
    CountBytes(s), SQLITE_TRANSIENT));
  return statement;
}

sqlite3_stmt* BindArg(sqlite3_stmt* statement, const PathChar* s, int index) {
  if (NULL == statement)
    return statement;
  CHECK(SQLITE_OK == sqlite3_bind_text16(statement,
                                         index,
                                         s,
                                         -1,  // -1 means s is zero-terminated
                                         SQLITE_TRANSIENT));
  return statement;
}
#endif  // !PATHSTRING_IS_STD_STRING

sqlite3_stmt* BindArg(sqlite3_stmt* statement, const string& s, int index) {
  if (NULL == statement)
    return statement;
  CHECK(SQLITE_OK == sqlite3_bind_text(statement,
                                       index,
                                       s.data(),
                                       CountBytes(s),
                                       SQLITE_TRANSIENT));
  return statement;
}

sqlite3_stmt* BindArg(sqlite3_stmt* statement, const char* s, int index) {
  if (NULL == statement)
    return statement;
  CHECK(SQLITE_OK == sqlite3_bind_text(statement,
                                       index,
                                       s,
                                       -1,  // -1 means s is zero-terminated
                                       SQLITE_TRANSIENT));
  return statement;
}

sqlite3_stmt* BindArg(sqlite3_stmt* statement, int32 n, int index) {
  if (NULL == statement)
    return statement;
  CHECK(SQLITE_OK == sqlite3_bind_int(statement, index, n));
  return statement;
}

sqlite3_stmt* BindArg(sqlite3_stmt* statement, int64 n, int index) {
  if (NULL == statement)
    return statement;
  CHECK(SQLITE_OK == sqlite3_bind_int64(statement, index, n));
  return statement;
}

sqlite3_stmt* BindArg(sqlite3_stmt* statement, double n, int index) {
  if (NULL == statement)
    return statement;
  CHECK(SQLITE_OK == sqlite3_bind_double(statement, index, n));
  return statement;
}

sqlite3_stmt* BindArg(sqlite3_stmt* statement, bool b, int index) {
  if (NULL == statement)
    return statement;
  int32 n = b ? 1 : 0;
  CHECK(SQLITE_OK == sqlite3_bind_int(statement, index, n));
  return statement;
}

sqlite3_stmt* BindArg(sqlite3_stmt* statement, const vector<uint8>& v,
                      int index) {
  if (NULL == statement)
    return statement;
  uint8* blob = v.empty() ? NULL : const_cast<uint8*>(&v[0]);
  CHECK(SQLITE_OK == sqlite3_bind_blob(statement,
                                       index,
                                       blob,
                                       v.size(),
                                       SQLITE_TRANSIENT));
  return statement;
}

sqlite3_stmt* BindArg(sqlite3_stmt* statement, SqliteNullType, int index) {
  if (NULL == statement)
    return statement;
  CHECK(SQLITE_OK == sqlite3_bind_null(statement, index));
  return statement;
}

#if !PATHSTRING_IS_STD_STRING
void GetColumn(sqlite3_stmt* statement, int index, PathString* value) {
  if (sqlite3_column_type(statement, index) == SQLITE_NULL) {
    value->clear();
  } else {
    value->assign(
        static_cast<const PathChar*>(sqlite3_column_text16(statement, index)),
        sqlite3_column_bytes16(statement, index) / sizeof(PathChar));
  }
}
#endif  // !PATHSTRING_IS_STD_STRING

void GetColumn(sqlite3_stmt* statement, int index, string* value) {
  if (sqlite3_column_type(statement, index) == SQLITE_NULL) {
    value->clear();
  } else {
    value->assign(
        reinterpret_cast<const char*>(sqlite3_column_text(statement, index)),
        sqlite3_column_bytes(statement, index));
  }
}

void GetColumn(sqlite3_stmt* statement, int index, int32* value) {
  *value = sqlite3_column_int(statement, index);
}

void GetColumn(sqlite3_stmt* statement, int index, int64* value) {
  *value = sqlite3_column_int64(statement, index);
}

void GetColumn(sqlite3_stmt* statement, int index, double* value) {
  *value = sqlite3_column_double(statement, index);
}

void GetColumn(sqlite3_stmt* statement, int index, bool* value) {
  *value = (0 != sqlite3_column_int(statement, index));
}

void GetColumn(sqlite3_stmt* statement, int index, std::vector<uint8>* value) {
  if (sqlite3_column_type(statement, index) == SQLITE_NULL) {
    value->clear();
  } else {
    const uint8* blob =
        reinterpret_cast<const uint8*>(sqlite3_column_blob(statement, index));
    for (int i = 0; i < sqlite3_column_bytes(statement, index); i++)
      value->push_back(blob[i]);
  }
}

bool DoesTableExist(sqlite3* dbhandle, const string& table_name) {
  ScopedStatement count_query
    (PrepareQuery(dbhandle,
                  "SELECT count(*) from sqlite_master where name = ?",
                  table_name));

  int query_result = sqlite3_step(count_query.get());
  CHECK(SQLITE_ROW == query_result);
  int count = sqlite3_column_int(count_query.get(), 0);

  return 1 == count;
}

void ScopedStatement::reset(sqlite3_stmt* statement) {
  if (NULL != statement_)
    sqlite3_finalize(statement_);
  statement_ = statement;
}

ScopedStatement::~ScopedStatement() {
  reset(NULL);
}

ScopedStatementResetter::~ScopedStatementResetter() {
  sqlite3_reset(statement_);
}

// Useful for encoding any sequence of bytes into a string that can be used in
// a table name. Kind of like hex encoding, except that A is zero and P is 15.
string APEncode(const string& in) {
  string result;
  result.reserve(in.size() * 2);
  for (string::const_iterator i = in.begin(); i != in.end(); ++i) {
    unsigned int c = static_cast<unsigned char>(*i);
    result.push_back((c & 0x0F) + 'A');
    result.push_back(((c >> 4) & 0x0F) + 'A');
  }
  return result;
}

string APDecode(const string& in) {
  string result;
  result.reserve(in.size() / 2);
  for (string::const_iterator i = in.begin(); i != in.end(); ++i) {
    unsigned int c = *i - 'A';
    if (++i != in.end())
      c = c | (static_cast<unsigned char>(*i - 'A') << 4);
    result.push_back(c);
  }
  return result;
}
