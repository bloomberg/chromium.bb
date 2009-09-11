// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_SQL_STATEMENT_H_
#define APP_SQL_STATEMENT_H_

#include <string>
#include <vector>

#include "app/sql/connection.h"
#include "base/basictypes.h"
#include "base/ref_counted.h"

namespace sql {

// Normal usage:
//   sql::Statement s = connection_.GetUniqueStatement(...);
//   if (!s)  // You should check for errors before using the statement.
//     return false;
//
//   s.BindInt(0, a);
//   if (s.Step())
//     return s.ColumnString(0);
class Statement {
 public:
  // Creates an uninitialized statement. The statement will be invalid until
  // you initialize it via Assign.
  Statement();

  Statement(scoped_refptr<Connection::StatementRef> ref);
  ~Statement();

  // Initializes this object with the given statement, which may or may not
  // be valid. Use is_valid() to check if it's OK.
  void Assign(scoped_refptr<Connection::StatementRef> ref);

  // Returns true if the statement can be executed. All functions can still
  // be used if the statement is invalid, but they will return failure or some
  // default value. This is because the statement can become invalid in the
  // middle of executing a command if there is a serioud error and the database
  // has to be reset.
  bool is_valid() const { return ref_->is_valid(); }

  // These operators allow conveniently checking if the statement is valid
  // or not. See the pattern above for an example.
  operator bool() const { return is_valid(); }
  bool operator!() const { return !is_valid(); }

  // Running -------------------------------------------------------------------

  // Executes the statement, returning true on success. This is like Step but
  // for when there is no output, like an INSERT statement.
  bool Run();

  // Executes the statement, returning true if there is a row of data returned.
  // You can keep calling Step() until it returns false to iterate through all
  // the rows in your result set.
  //
  // When Step returns false, the result is either that there is no more data
  // or there is an error. This makes it most convenient for loop usage. If you
  // need to disambiguate these cases, use Succeeded().
  //
  // Typical example:
  //   while (s.Step()) {
  //     ...
  //   }
  //   return s.Succeeded();
  bool Step();

  // Resets the statement to its initial condition. This includes clearing all
  // the bound variables and any current result row.
  void Reset();

  // Returns true if the last executed thing in this statement succeeded. If
  // there was no last executed thing or the statement is invalid, this will
  // return false.
  bool Succeeded() const;

  // Binding -------------------------------------------------------------------

  // These all take a 0-based argument index and return true on failure. You
  // may not always care about the return value (they'll DCHECK if they fail).
  // The main thing you may want to check is when binding large blobs or
  // strings there may be out of memory.
  bool BindNull(int col);
  bool BindInt(int col, int val);
  bool BindInt64(int col, int64 val);
  bool BindDouble(int col, double val);
  bool BindCString(int col, const char* val);
  bool BindString(int col, const std::string& val);
  bool BindBlob(int col, const void* value, int value_len);

  // Retrieving ----------------------------------------------------------------

  // Returns the number of output columns in the result.
  int ColumnCount() const;

  // These all take a 0-based argument index.
  int ColumnInt(int col) const;
  int64 ColumnInt64(int col) const;
  double ColumnDouble(int col) const;
  std::string ColumnString(int col) const;

  // When reading a blob, you can get a raw pointer to the underlying data,
  // along with the length, or you can just ask us to copy the blob into a
  // vector. Danger! ColumnBlob may return NULL if there is no data!
  int ColumnByteLength(int col);
  const void* ColumnBlob(int col);
  void ColumnBlobAsVector(int col, std::vector<char>* val);

 private:
  // This is intended to check for serious errors and report them to the
  // connection object. It takes a sqlite error code, and returns the same
  // code. Currently this function just updates the succeeded flag, but will be
  // enhanced in the future to do the notification.
  int CheckError(int err);

  // The actual sqlite statement. This may be unique to us, or it may be cached
  // by the connection, which is why it's refcounted. This pointer is
  // guaranteed non-NULL.
  scoped_refptr<Connection::StatementRef> ref_;

  // See Succeeded() for what this holds.
  bool succeeded_;

  DISALLOW_COPY_AND_ASSIGN(Statement);
};

}  // namespace sql

#endif  // APP_SQL_STATEMENT_H_
