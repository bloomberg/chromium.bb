// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Typesafe composition of SQL query strings.

#ifndef CHROME_BROWSER_SYNC_UTIL_QUERY_HELPERS_H_
#define CHROME_BROWSER_SYNC_UTIL_QUERY_HELPERS_H_

#include <limits>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/string16.h"
#include "build/build_config.h"
#include "chrome/browser/sync/util/sync_types.h"
#include "third_party/sqlite/preprocessed/sqlite3.h"

enum SqliteNullType {
  SQLITE_NULL_VALUE
};

int SqliteOpen(const FilePath& filename, sqlite3** ppDb);

sqlite3_stmt* PrepareQuery(sqlite3* dbhandle, const char* query);
sqlite3_stmt* BindArg(sqlite3_stmt*, const std::string&, int index);
sqlite3_stmt* BindArg(sqlite3_stmt*, const char*, int index);
sqlite3_stmt* BindArg(sqlite3_stmt*, int32, int index);
sqlite3_stmt* BindArg(sqlite3_stmt*, int64, int index);
sqlite3_stmt* BindArg(sqlite3_stmt*, double, int index);
sqlite3_stmt* BindArg(sqlite3_stmt*, bool, int index);
sqlite3_stmt* BindArg(sqlite3_stmt*, const std::vector<uint8>&, int index);
sqlite3_stmt* BindArg(sqlite3_stmt*, SqliteNullType, int index);

void GetColumn(sqlite3_stmt*, int index, string16* value);
void GetColumn(sqlite3_stmt*, int index, std::string* value);
void GetColumn(sqlite3_stmt*, int index, int32* value);
void GetColumn(sqlite3_stmt*, int index, int64* value);
void GetColumn(sqlite3_stmt*, int index, double* value);
void GetColumn(sqlite3_stmt*, int index, bool* value);
void GetColumn(sqlite3_stmt*, int index, std::vector<uint8>* value);

bool DoesTableExist(sqlite3* dbhandle, const std::string& tablename);

// Prepares a query with a WHERE clause that filters the values by the items
// passed inside of the Vector.
// Example:
//
// vector<std::string> v;
// v.push_back("abc");
// v.push_back("123");
// PrepareQuery(dbhandle, "SELECT * FROM table", "column_name", v.begin(),
//              v.end(), "ORDER BY id");
//
// will produce the following query.
//
// SELECT * FROM table WHERE column_name = 'abc' OR column_name = '123' ORDER BY
// id.
//
template<typename ItemIterator>
sqlite3_stmt* PrepareQueryWhereColumnIn(sqlite3* dbhandle,
                                        const std::string& query_head,
                                        const std::string& filtername,
                                        ItemIterator begin, ItemIterator end,
                                        const std::string& query_options) {
  std::string query;
  query.reserve(512);
  query += query_head;
  const char* joiner = " WHERE ";
  for (ItemIterator it = begin; it != end; ++it) {
    query += joiner;
    query += filtername;
    query += " = ?";
    joiner = " OR ";
  }
  query += " ";
  query += query_options;
  sqlite3_stmt* statement = NULL;
  const char* query_tail;
  if (SQLITE_OK != sqlite3_prepare(dbhandle, query.data(),
                                   CountBytes(query), &statement,
                                   &query_tail)) {
    LOG(ERROR) << query << "\n" << sqlite3_errmsg(dbhandle);
  }
  int index = 1;
  for (ItemIterator it = begin; it != end; ++it) {
    BindArg(statement, *it, index);
    ++index;
  }
  return statement;
}

template <typename Type1>
inline sqlite3_stmt* PrepareQuery(sqlite3* dbhandle, const char* query,
                                  const Type1& arg1) {
  return BindArg(PrepareQuery(dbhandle, query), arg1, 1);
}

template <typename Type1, typename Type2>
inline sqlite3_stmt* PrepareQuery(sqlite3* dbhandle, const char* query,
                                  const Type1& arg1, const Type2& arg2) {
  return BindArg(PrepareQuery(dbhandle, query, arg1), arg2, 2);
}

template <typename Type1, typename Type2, typename Type3>
inline sqlite3_stmt* PrepareQuery(sqlite3* dbhandle, const char* query,
                                  const Type1& arg1, const Type2& arg2,
                                  const Type3& arg3) {
  return BindArg(PrepareQuery(dbhandle, query, arg1, arg2), arg3, 3);
}

template <typename Type1, typename Type2, typename Type3, typename Type4>
inline sqlite3_stmt* PrepareQuery(sqlite3* dbhandle, const char* query,
                                  const Type1& arg1, const Type2& arg2,
                                  const Type3& arg3, const Type4& arg4) {
  return BindArg(PrepareQuery(dbhandle, query, arg1, arg2, arg3), arg4, 4);
}

template <typename Type1, typename Type2, typename Type3, typename Type4,
          typename Type5>
inline sqlite3_stmt* PrepareQuery(sqlite3* dbhandle, const char* query,
                                  const Type1& arg1, const Type2& arg2,
                                  const Type3& arg3, const Type4& arg4,
                                  const Type5& arg5) {
  return BindArg(PrepareQuery(dbhandle, query, arg1, arg2, arg3, arg4),
                 arg5, 5);
}

template <typename Type1, typename Type2, typename Type3, typename Type4,
          typename Type5, typename Type6>
inline sqlite3_stmt* PrepareQuery(sqlite3* dbhandle, const char* query,
                                  const Type1& arg1, const Type2& arg2,
                                  const Type3& arg3, const Type4& arg4,
                                  const Type5& arg5, const Type6& arg6) {
  return BindArg(PrepareQuery(dbhandle, query, arg1, arg2, arg3, arg4, arg5),
                 arg6, 6);
}

template <typename Type1, typename Type2, typename Type3, typename Type4,
          typename Type5, typename Type6, typename Type7>
inline sqlite3_stmt* PrepareQuery(sqlite3* dbhandle, const char* query,
                                  const Type1& arg1, const Type2& arg2,
                                  const Type3& arg3, const Type4& arg4,
                                  const Type5& arg5, const Type6& arg6,
                                  const Type7& arg7) {
  return BindArg(PrepareQuery(dbhandle, query, arg1, arg2, arg3, arg4, arg5,
                              arg6),
                 arg7, 7);
}

template <typename Type1, typename Type2, typename Type3, typename Type4,
          typename Type5, typename Type6, typename Type7, typename Type8>
inline sqlite3_stmt* PrepareQuery(sqlite3* dbhandle, const char* query,
                                  const Type1& arg1, const Type2& arg2,
                                  const Type3& arg3, const Type4& arg4,
                                  const Type5& arg5, const Type6& arg6,
                                  const Type7& arg7, const Type8& arg8) {
  return BindArg(PrepareQuery(dbhandle, query, arg1, arg2, arg3, arg4, arg5,
                              arg6, arg7),
                 arg8, 8);
}

template <typename Type1, typename Type2, typename Type3, typename Type4,
          typename Type5, typename Type6, typename Type7, typename Type8,
          typename Type9>
inline sqlite3_stmt* PrepareQuery(sqlite3* dbhandle, const char* query,
                                  const Type1& arg1, const Type2& arg2,
                                  const Type3& arg3, const Type4& arg4,
                                  const Type5& arg5, const Type6& arg6,
                                  const Type7& arg7, const Type8& arg8,
                                  const Type9& arg9) {
  return BindArg(PrepareQuery(dbhandle, query, arg1, arg2, arg3, arg4, arg5,
                              arg6, arg7, arg8),
                 arg9, 9);
}

template <typename Type1, typename Type2, typename Type3, typename Type4,
          typename Type5, typename Type6, typename Type7, typename Type8,
          typename Type9, typename Type10>
inline sqlite3_stmt* PrepareQuery(sqlite3* dbhandle, const char* query,
                                  const Type1& arg1, const Type2& arg2,
                                  const Type3& arg3, const Type4& arg4,
                                  const Type5& arg5, const Type6& arg6,
                                  const Type7& arg7, const Type8& arg8,
                                  const Type9& arg9, const Type10& arg10) {
  return BindArg(PrepareQuery(dbhandle, query, arg1, arg2, arg3, arg4, arg5,
                              arg6, arg7, arg8, arg9),
                 arg10, 10);
}

template <typename Type1, typename Type2, typename Type3, typename Type4,
          typename Type5, typename Type6, typename Type7, typename Type8,
          typename Type9, typename Type10, typename Type11>
inline sqlite3_stmt* PrepareQuery(sqlite3* dbhandle, const char* query,
                                  const Type1& arg1, const Type2& arg2,
                                  const Type3& arg3, const Type4& arg4,
                                  const Type5& arg5, const Type6& arg6,
                                  const Type7& arg7, const Type8& arg8,
                                  const Type9& arg9, const Type10& arg10,
                                  const Type11& arg11) {
  return BindArg(PrepareQuery(dbhandle, query, arg1, arg2, arg3, arg4, arg5,
                              arg6, arg7, arg8, arg9, arg10),
                 arg11, 11);
}

template <typename Type1, typename Type2, typename Type3, typename Type4,
          typename Type5, typename Type6, typename Type7, typename Type8,
          typename Type9, typename Type10, typename Type11, typename Type12>
inline sqlite3_stmt* PrepareQuery(sqlite3* dbhandle, const char* query,
                                  const Type1& arg1, const Type2& arg2,
                                  const Type3& arg3, const Type4& arg4,
                                  const Type5& arg5, const Type6& arg6,
                                  const Type7& arg7, const Type8& arg8,
                                  const Type9& arg9, const Type10& arg10,
                                  const Type11& arg11, const Type12& arg12) {
  return BindArg(PrepareQuery(dbhandle, query, arg1, arg2, arg3, arg4, arg5,
                              arg6, arg7, arg8, arg9, arg10, arg11),
                 arg12, 12);
}

template <typename Type1, typename Type2, typename Type3, typename Type4,
          typename Type5, typename Type6, typename Type7, typename Type8,
          typename Type9, typename Type10, typename Type11, typename Type12,
          typename Type13>
inline sqlite3_stmt* PrepareQuery(sqlite3* dbhandle, const char* query,
                                  const Type1& arg1, const Type2& arg2,
                                  const Type3& arg3, const Type4& arg4,
                                  const Type5& arg5, const Type6& arg6,
                                  const Type7& arg7, const Type8& arg8,
                                  const Type9& arg9, const Type10& arg10,
                                  const Type11& arg11, const Type12& arg12,
                                  const Type13& arg13) {
  return BindArg(PrepareQuery(dbhandle, query, arg1, arg2, arg3, arg4, arg5,
                              arg6, arg7, arg8, arg9, arg10, arg11, arg12),
                 arg13, 13);
}

template <typename Type1, typename Type2, typename Type3, typename Type4,
          typename Type5, typename Type6, typename Type7, typename Type8,
          typename Type9, typename Type10, typename Type11, typename Type12,
          typename Type13, typename Type14>
inline sqlite3_stmt* PrepareQuery(sqlite3* dbhandle, const char* query,
                                  const Type1& arg1, const Type2& arg2,
                                  const Type3& arg3, const Type4& arg4,
                                  const Type5& arg5, const Type6& arg6,
                                  const Type7& arg7, const Type8& arg8,
                                  const Type9& arg9, const Type10& arg10,
                                  const Type11& arg11, const Type12& arg12,
                                  const Type13& arg13, const Type14& arg14) {
  return BindArg(PrepareQuery(dbhandle, query, arg1, arg2, arg3, arg4, arg5,
                              arg6, arg7, arg8, arg9, arg10, arg11, arg12,
                              arg13),
                 arg14, 14);
}

template <typename Type1, typename Type2, typename Type3, typename Type4,
          typename Type5, typename Type6, typename Type7, typename Type8,
          typename Type9, typename Type10, typename Type11, typename Type12,
          typename Type13, typename Type14, typename Type15>
inline sqlite3_stmt* PrepareQuery(sqlite3* dbhandle, const char* query,
                                  const Type1& arg1, const Type2& arg2,
                                  const Type3& arg3, const Type4& arg4,
                                  const Type5& arg5, const Type6& arg6,
                                  const Type7& arg7, const Type8& arg8,
                                  const Type9& arg9, const Type10& arg10,
                                  const Type11& arg11, const Type12& arg12,
                                  const Type13& arg13, const Type14& arg14,
                                  const Type15& arg15) {
  return BindArg(PrepareQuery(dbhandle, query, arg1, arg2, arg3, arg4, arg5,
                              arg6, arg7, arg8, arg9, arg10, arg11, arg12,
                              arg13, arg14),
                 arg15, 15);
}

template <typename Type1, typename Type2, typename Type3, typename Type4,
          typename Type5, typename Type6, typename Type7, typename Type8,
          typename Type9, typename Type10, typename Type11, typename Type12,
          typename Type13, typename Type14, typename Type15, typename Type16>
inline sqlite3_stmt* PrepareQuery(sqlite3* dbhandle, const char* query,
                                  const Type1& arg1, const Type2& arg2,
                                  const Type3& arg3, const Type4& arg4,
                                  const Type5& arg5, const Type6& arg6,
                                  const Type7& arg7, const Type8& arg8,
                                  const Type9& arg9, const Type10& arg10,
                                  const Type11& arg11, const Type12& arg12,
                                  const Type13& arg13, const Type14& arg14,
                                  const Type15& arg15, const Type16& arg16) {
  return BindArg(PrepareQuery(dbhandle, query, arg1, arg2, arg3, arg4, arg5,
                              arg6, arg7, arg8, arg9, arg10, arg11, arg12,
                              arg13, arg14, arg15),
                 arg16, 16);
}

template <typename Type1, typename Type2, typename Type3, typename Type4,
          typename Type5, typename Type6, typename Type7, typename Type8,
          typename Type9, typename Type10, typename Type11, typename Type12,
          typename Type13, typename Type14, typename Type15, typename Type16,
          typename Type17>
inline sqlite3_stmt* PrepareQuery(sqlite3* dbhandle, const char* query,
                                  const Type1& arg1, const Type2& arg2,
                                  const Type3& arg3, const Type4& arg4,
                                  const Type5& arg5, const Type6& arg6,
                                  const Type7& arg7, const Type8& arg8,
                                  const Type9& arg9, const Type10& arg10,
                                  const Type11& arg11, const Type12& arg12,
                                  const Type13& arg13, const Type14& arg14,
                                  const Type15& arg15, const Type16& arg16,
                                  const Type17& arg17) {
  return BindArg(PrepareQuery(dbhandle, query, arg1, arg2, arg3, arg4, arg5,
                              arg6, arg7, arg8, arg9, arg10, arg11, arg12,
                              arg13, arg14, arg15, arg16),
                 arg17, 17);
}

void ExecOrDie(sqlite3* dbhandle, const char* query);

// Finalizes (deletes) the query before returning.
void ExecOrDie(sqlite3* dbhandle, const char* query, sqlite3_stmt* statement);

template <typename Type1, typename Type2, typename Type3, typename Type4,
          typename Type5, typename Type6, typename Type7, typename Type8,
          typename Type9, typename Type10>
inline void ExecOrDie(sqlite3* dbhandle, const char* query,
                      const Type1& arg1, const Type2& arg2,
                      const Type3& arg3, const Type4& arg4,
                      const Type5& arg5, const Type6& arg6,
                      const Type7& arg7, const Type8& arg8,
                      const Type9& arg9, const Type10& arg10) {
  return ExecOrDie(dbhandle, query,
                   PrepareQuery(dbhandle, query, arg1, arg2, arg3, arg4, arg5,
                                arg6, arg7, arg8, arg9, arg10));
}

template <typename Type1, typename Type2, typename Type3, typename Type4,
          typename Type5, typename Type6, typename Type7, typename Type8,
          typename Type9>
inline void ExecOrDie(sqlite3* dbhandle, const char* query,
                      const Type1& arg1, const Type2& arg2,
                      const Type3& arg3, const Type4& arg4,
                      const Type5& arg5, const Type6& arg6,
                      const Type7& arg7, const Type8& arg8,
                      const Type9& arg9) {
  return ExecOrDie(dbhandle, query,
                   PrepareQuery(dbhandle, query, arg1, arg2, arg3, arg4, arg5,
                                arg6, arg7, arg8, arg9));
}

template <typename Type1, typename Type2, typename Type3, typename Type4,
          typename Type5, typename Type6, typename Type7, typename Type8>
inline void ExecOrDie(sqlite3* dbhandle, const char* query,
                      const Type1& arg1, const Type2& arg2,
                      const Type3& arg3, const Type4& arg4,
                      const Type5& arg5, const Type6& arg6,
                      const Type7& arg7, const Type8& arg8) {
  return ExecOrDie(dbhandle, query,
                   PrepareQuery(dbhandle, query, arg1, arg2, arg3, arg4, arg5,
                                arg6, arg7, arg8));
}

template <typename Type1, typename Type2, typename Type3, typename Type4,
          typename Type5, typename Type6, typename Type7>
inline void ExecOrDie(sqlite3* dbhandle, const char* query,
                      const Type1& arg1, const Type2& arg2,
                      const Type3& arg3, const Type4& arg4,
                      const Type5& arg5, const Type6& arg6,
                      const Type7& arg7) {
  return ExecOrDie(dbhandle, query,
                   PrepareQuery(dbhandle, query, arg1, arg2, arg3, arg4, arg5,
                                arg6, arg7));
}

template <typename Type1, typename Type2, typename Type3, typename Type4,
          typename Type5, typename Type6>
inline void ExecOrDie(sqlite3* dbhandle, const char* query,
                      const Type1& arg1, const Type2& arg2,
                      const Type3& arg3, const Type4& arg4,
                      const Type5& arg5, const Type6& arg6) {
  return ExecOrDie(dbhandle, query, PrepareQuery(dbhandle, query, arg1, arg2,
                                                 arg3, arg4, arg5, arg6));
}

template <typename Type1, typename Type2, typename Type3, typename Type4,
          typename Type5>
inline void ExecOrDie(sqlite3* dbhandle, const char* query,
                      const Type1& arg1, const Type2& arg2,
                      const Type3& arg3, const Type4& arg4,
                      const Type5& arg5) {
  return ExecOrDie(dbhandle, query, PrepareQuery(dbhandle, query, arg1, arg2,
                                                 arg3, arg4, arg5));
}

template <typename Type1, typename Type2, typename Type3, typename Type4>
inline void ExecOrDie(sqlite3* dbhandle, const char* query,
                      const Type1& arg1, const Type2& arg2,
                      const Type3& arg3, const Type4& arg4) {
  return ExecOrDie(dbhandle, query, PrepareQuery(dbhandle, query, arg1, arg2,
                                                 arg3, arg4));
}

template <typename Type1, typename Type2, typename Type3>
inline void ExecOrDie(sqlite3* dbhandle, const char* query,
                      const Type1& arg1, const Type2& arg2,
                      const Type3& arg3) {
  return ExecOrDie(dbhandle, query, PrepareQuery(dbhandle, query, arg1, arg2,
                                                 arg3));
}

template <typename Type1, typename Type2>
inline void ExecOrDie(sqlite3* dbhandle, const char* query,
                      const Type1& arg1, const Type2& arg2) {
  return ExecOrDie(dbhandle, query, PrepareQuery(dbhandle, query, arg1, arg2));
}

template <typename Type1>
inline void ExecOrDie(sqlite3* dbhandle, const char* query,
                      const Type1& arg1) {
  return ExecOrDie(dbhandle, query, PrepareQuery(dbhandle, query, arg1));
}


int Exec(sqlite3* dbhandle, const char* query);
// Finalizes (deletes) the query before returning.
int Exec(sqlite3* dbhandle, const char* query, sqlite3_stmt* statement);

template <typename Type1, typename Type2, typename Type3, typename Type4,
          typename Type5, typename Type6, typename Type7, typename Type8,
          typename Type9, typename Type10, typename Type11, typename Type12,
          typename Type13, typename Type14, typename Type15, typename Type16,
          typename Type17>
inline int Exec(sqlite3* dbhandle, const char* query,
                const Type1& arg1, const Type2& arg2,
                const Type3& arg3, const Type4& arg4,
                const Type5& arg5, const Type6& arg6,
                const Type7& arg7, const Type8& arg8,
                const Type9& arg9, const Type10& arg10,
                const Type11& arg11, const Type12& arg12,
                const Type13& arg13, const Type14& arg14,
                const Type15& arg15, const Type16& arg16,
                const Type17& arg17) {
  return Exec(dbhandle, query,
              PrepareQuery(dbhandle, query, arg1, arg2, arg3, arg4, arg5,
                           arg6, arg7, arg8, arg9, arg10, arg11, arg12, arg13,
                           arg14, arg15, arg16, arg17));
}

template <typename Type1, typename Type2, typename Type3, typename Type4,
          typename Type5, typename Type6, typename Type7, typename Type8,
          typename Type9, typename Type10, typename Type11, typename Type12,
          typename Type13, typename Type14, typename Type15, typename Type16>
inline int Exec(sqlite3* dbhandle, const char* query,
                const Type1& arg1, const Type2& arg2,
                const Type3& arg3, const Type4& arg4,
                const Type5& arg5, const Type6& arg6,
                const Type7& arg7, const Type8& arg8,
                const Type9& arg9, const Type10& arg10,
                const Type11& arg11, const Type12& arg12,
                const Type13& arg13, const Type14& arg14,
                const Type15& arg15, const Type16& arg16) {
  return Exec(dbhandle, query,
              PrepareQuery(dbhandle, query, arg1, arg2, arg3, arg4, arg5,
                           arg6, arg7, arg8, arg9, arg10, arg11, arg12, arg13,
                           arg14, arg15, arg16));
}

template <typename Type1, typename Type2, typename Type3, typename Type4,
          typename Type5, typename Type6, typename Type7, typename Type8,
          typename Type9, typename Type10, typename Type11, typename Type12,
          typename Type13, typename Type14, typename Type15>
inline int Exec(sqlite3* dbhandle, const char* query,
                const Type1& arg1, const Type2& arg2,
                const Type3& arg3, const Type4& arg4,
                const Type5& arg5, const Type6& arg6,
                const Type7& arg7, const Type8& arg8,
                const Type9& arg9, const Type10& arg10,
                const Type11& arg11, const Type12& arg12,
                const Type13& arg13, const Type14& arg14,
                const Type15& arg15) {
  return Exec(dbhandle, query,
              PrepareQuery(dbhandle, query, arg1, arg2, arg3, arg4, arg5,
                           arg6, arg7, arg8, arg9, arg10, arg11, arg12, arg13,
                           arg14, arg15));
}

template <typename Type1, typename Type2, typename Type3, typename Type4,
          typename Type5, typename Type6, typename Type7, typename Type8,
          typename Type9, typename Type10, typename Type11, typename Type12,
          typename Type13, typename Type14>
inline int Exec(sqlite3* dbhandle, const char* query,
                const Type1& arg1, const Type2& arg2,
                const Type3& arg3, const Type4& arg4,
                const Type5& arg5, const Type6& arg6,
                const Type7& arg7, const Type8& arg8,
                const Type9& arg9, const Type10& arg10,
                const Type11& arg11, const Type12& arg12,
                const Type13& arg13, const Type14& arg14) {
  return Exec(dbhandle, query,
              PrepareQuery(dbhandle, query, arg1, arg2, arg3, arg4, arg5,
                           arg6, arg7, arg8, arg9, arg10, arg11, arg12, arg13,
                           arg14));
}

template <typename Type1, typename Type2, typename Type3, typename Type4,
          typename Type5, typename Type6, typename Type7, typename Type8,
          typename Type9, typename Type10, typename Type11, typename Type12,
          typename Type13>
inline int Exec(sqlite3* dbhandle, const char* query,
                const Type1& arg1, const Type2& arg2,
                const Type3& arg3, const Type4& arg4,
                const Type5& arg5, const Type6& arg6,
                const Type7& arg7, const Type8& arg8,
                const Type9& arg9, const Type10& arg10,
                const Type11& arg11, const Type12& arg12,
                const Type13& arg13) {
  return Exec(dbhandle, query,
              PrepareQuery(dbhandle, query, arg1, arg2, arg3, arg4, arg5,
                           arg6, arg7, arg8, arg9, arg10, arg11, arg12, arg13));
}

template <typename Type1, typename Type2, typename Type3, typename Type4,
          typename Type5, typename Type6, typename Type7, typename Type8,
          typename Type9, typename Type10, typename Type11, typename Type12>
inline int Exec(sqlite3* dbhandle, const char* query,
                const Type1& arg1, const Type2& arg2,
                const Type3& arg3, const Type4& arg4,
                const Type5& arg5, const Type6& arg6,
                const Type7& arg7, const Type8& arg8,
                const Type9& arg9, const Type10& arg10,
                const Type11& arg11, const Type12& arg12) {
  return Exec(dbhandle, query,
              PrepareQuery(dbhandle, query, arg1, arg2, arg3, arg4, arg5,
                           arg6, arg7, arg8, arg9, arg10, arg11, arg12));
}

template <typename Type1, typename Type2, typename Type3, typename Type4,
          typename Type5, typename Type6, typename Type7, typename Type8,
          typename Type9, typename Type10, typename Type11>
inline int Exec(sqlite3* dbhandle, const char* query,
                const Type1& arg1, const Type2& arg2,
                const Type3& arg3, const Type4& arg4,
                const Type5& arg5, const Type6& arg6,
                const Type7& arg7, const Type8& arg8,
                const Type9& arg9, const Type10& arg10,
                const Type11& arg11) {
  return Exec(dbhandle, query,
              PrepareQuery(dbhandle, query, arg1, arg2, arg3, arg4, arg5,
                           arg6, arg7, arg8, arg9, arg10, arg11));
}

template <typename Type1, typename Type2, typename Type3, typename Type4,
          typename Type5, typename Type6, typename Type7, typename Type8,
          typename Type9, typename Type10>
inline int Exec(sqlite3* dbhandle, const char* query,
                const Type1& arg1, const Type2& arg2,
                const Type3& arg3, const Type4& arg4,
                const Type5& arg5, const Type6& arg6,
                const Type7& arg7, const Type8& arg8,
                const Type9& arg9, const Type10& arg10) {
  return Exec(dbhandle, query,
              PrepareQuery(dbhandle, query, arg1, arg2, arg3, arg4, arg5,
                           arg6, arg7, arg8, arg9, arg10));
}

template <typename Type1, typename Type2, typename Type3, typename Type4,
          typename Type5, typename Type6, typename Type7, typename Type8,
          typename Type9>
inline int Exec(sqlite3* dbhandle, const char* query,
                const Type1& arg1, const Type2& arg2,
                const Type3& arg3, const Type4& arg4,
                const Type5& arg5, const Type6& arg6,
                const Type7& arg7, const Type8& arg8,
                const Type9& arg9) {
  return Exec(dbhandle, query,
              PrepareQuery(dbhandle, query, arg1, arg2, arg3, arg4, arg5,
                           arg6, arg7, arg8, arg9));
}

template <typename Type1, typename Type2, typename Type3, typename Type4,
          typename Type5, typename Type6, typename Type7, typename Type8>
inline int Exec(sqlite3* dbhandle, const char* query,
                const Type1& arg1, const Type2& arg2,
                const Type3& arg3, const Type4& arg4,
                const Type5& arg5, const Type6& arg6,
                const Type7& arg7, const Type8& arg8) {
  return Exec(dbhandle, query,
              PrepareQuery(dbhandle, query, arg1, arg2, arg3, arg4, arg5,
                           arg6, arg7, arg8));
}

template <typename Type1, typename Type2, typename Type3, typename Type4,
          typename Type5, typename Type6, typename Type7>
inline int Exec(sqlite3* dbhandle, const char* query,
                const Type1& arg1, const Type2& arg2,
                const Type3& arg3, const Type4& arg4,
                const Type5& arg5, const Type6& arg6,
                const Type7& arg7) {
  return Exec(dbhandle, query,
              PrepareQuery(dbhandle, query, arg1, arg2, arg3, arg4, arg5,
                           arg6, arg7));
}

template <typename Type1, typename Type2, typename Type3, typename Type4,
          typename Type5, typename Type6>
inline int Exec(sqlite3* dbhandle, const char* query,
                const Type1& arg1, const Type2& arg2,
                const Type3& arg3, const Type4& arg4,
                const Type5& arg5, const Type6& arg6) {
  return Exec(dbhandle, query, PrepareQuery(dbhandle, query, arg1, arg2,
                                            arg3, arg4, arg5, arg6));
}

template <typename Type1, typename Type2, typename Type3, typename Type4,
          typename Type5>
inline int Exec(sqlite3* dbhandle, const char* query,
                const Type1& arg1, const Type2& arg2,
                const Type3& arg3, const Type4& arg4,
                const Type5& arg5) {
  return Exec(dbhandle, query, PrepareQuery(dbhandle, query, arg1, arg2,
                                            arg3, arg4, arg5));
}

template <typename Type1, typename Type2, typename Type3, typename Type4>
inline int Exec(sqlite3* dbhandle, const char* query,
                const Type1& arg1, const Type2& arg2,
                const Type3& arg3, const Type4& arg4) {
  return Exec(dbhandle, query, PrepareQuery(dbhandle, query, arg1, arg2,
                                            arg3, arg4));
}

template <typename Type1, typename Type2, typename Type3>
inline int Exec(sqlite3* dbhandle, const char* query,
                const Type1& arg1, const Type2& arg2,
                const Type3& arg3) {
  return Exec(dbhandle, query, PrepareQuery(dbhandle, query, arg1, arg2,
                                            arg3));
}

template <typename Type1, typename Type2>
inline int Exec(sqlite3* dbhandle, const char* query,
                const Type1& arg1, const Type2& arg2) {
  return Exec(dbhandle, query, PrepareQuery(dbhandle, query, arg1, arg2));
}

template <typename Type1>
inline int Exec(sqlite3* dbhandle, const char* query,
                const Type1& arg1) {
  return Exec(dbhandle, query, PrepareQuery(dbhandle, query, arg1));
}


// Holds an sqlite3_stmt* and automatically finalizes when passes out of scope.
class ScopedStatement {
 public:
  explicit ScopedStatement(sqlite3_stmt* statement = 0)
      : statement_(statement) { }
  ~ScopedStatement();

  sqlite3_stmt* get() const { return statement_; }

  // Finalizes currently held statement and sets to new one.
  void reset(sqlite3_stmt* statement);
 protected:
  sqlite3_stmt* statement_;

  DISALLOW_COPY_AND_ASSIGN(ScopedStatement);
};


// Holds an sqlite3_stmt* and automatically resets when passes out of scope.
class ScopedStatementResetter {
 public:
  explicit ScopedStatementResetter(sqlite3_stmt* statement)
      : statement_(statement) { }
  ~ScopedStatementResetter();

 protected:
  sqlite3_stmt* const statement_;

  DISALLOW_COPY_AND_ASSIGN(ScopedStatementResetter);
};

// Useful for encoding any sequence of bytes into a string that can be used in
// a table name. Kind of like hex encoding, except that A is zero and P is 15.
std::string APEncode(const std::string& in);
std::string APDecode(const std::string& in);

#endif  // CHROME_BROWSER_SYNC_UTIL_QUERY_HELPERS_H_
