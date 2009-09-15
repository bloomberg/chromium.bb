// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/sql/statement.h"

#include "base/logging.h"
#include "third_party/sqlite/preprocessed/sqlite3.h"

namespace sql {

// This empty constructor initializes our reference with an empty one so that
// we don't have to NULL-check the ref_ to see if the statement is valid: we
// only have to check the ref's validity bit.
Statement::Statement()
    : ref_(new Connection::StatementRef),
      succeeded_(false) {
}

Statement::Statement(scoped_refptr<Connection::StatementRef> ref)
    : ref_(ref),
      succeeded_(false) {
}

Statement::~Statement() {
  // Free the resources associated with this statement. We assume there's only
  // one statement active for a given sqlite3_stmt at any time, so this won't
  // mess with anything.
  Reset();
}

void Statement::Assign(scoped_refptr<Connection::StatementRef> ref) {
  ref_ = ref;
}

bool Statement::Run() {
  if (!is_valid())
    return false;
  return CheckError(sqlite3_step(ref_->stmt())) == SQLITE_DONE;
}

bool Statement::Step() {
  if (!is_valid())
    return false;
  return CheckError(sqlite3_step(ref_->stmt())) == SQLITE_ROW;
}

void Statement::Reset() {
  if (is_valid())
    CheckError(sqlite3_reset(ref_->stmt()));
  succeeded_ = false;
}

bool Statement::Succeeded() const {
  if (!is_valid())
    return false;
  return succeeded_;
}

bool Statement::BindNull(int col) {
  if (is_valid()) {
    int err = CheckError(sqlite3_bind_null(ref_->stmt(), col + 1));
    DCHECK(err == SQLITE_OK) << ref_->connection()->GetErrorMessage();
    return err == SQLITE_OK;
  }
  return false;
}

bool Statement::BindInt(int col, int val) {
  if (is_valid()) {
    int err = CheckError(sqlite3_bind_int(ref_->stmt(), col + 1, val));
    DCHECK(err == SQLITE_OK) << ref_->connection()->GetErrorMessage();
    return err == SQLITE_OK;
  }
  return false;
}

bool Statement::BindInt64(int col, int64 val) {
  if (is_valid()) {
    int err = CheckError(sqlite3_bind_int64(ref_->stmt(), col + 1, val));
    DCHECK(err == SQLITE_OK) << ref_->connection()->GetErrorMessage();
    return err == SQLITE_OK;
  }
  return false;
}

bool Statement::BindDouble(int col, double val) {
  if (is_valid()) {
    int err = CheckError(sqlite3_bind_double(ref_->stmt(), col + 1, val));
    DCHECK(err == SQLITE_OK) << ref_->connection()->GetErrorMessage();
    return err == SQLITE_OK;
  }
  return false;
}

bool Statement::BindCString(int col, const char* val) {
  if (is_valid()) {
    int err = CheckError(sqlite3_bind_text(ref_->stmt(), col + 1, val, -1,
                         SQLITE_TRANSIENT));
    DCHECK(err == SQLITE_OK) << ref_->connection()->GetErrorMessage();
    return err == SQLITE_OK;
  }
  return false;
}

bool Statement::BindString(int col, const std::string& val) {
  if (is_valid()) {
    int err = CheckError(sqlite3_bind_text(ref_->stmt(), col + 1, val.data(),
                                           val.size(), SQLITE_TRANSIENT));
    DCHECK(err == SQLITE_OK) << ref_->connection()->GetErrorMessage();
    return err == SQLITE_OK;
  }
  return false;
}

bool Statement::BindBlob(int col, const void* val, int val_len) {
  if (is_valid()) {
    int err = CheckError(sqlite3_bind_blob(ref_->stmt(), col + 1,
                         val, val_len, SQLITE_TRANSIENT));
    DCHECK(err == SQLITE_OK) << ref_->connection()->GetErrorMessage();
    return err == SQLITE_OK;
  }
  return false;
}

int Statement::ColumnCount() const {
  if (!is_valid()) {
    NOTREACHED();
    return 0;
  }
  return sqlite3_column_count(ref_->stmt());
}

int Statement::ColumnInt(int col) const {
  if (!is_valid()) {
    NOTREACHED();
    return 0;
  }
  return sqlite3_column_int(ref_->stmt(), col);
}

int64 Statement::ColumnInt64(int col) const {
  if (!is_valid()) {
    NOTREACHED();
    return 0;
  }
  return sqlite3_column_int64(ref_->stmt(), col);
}

double Statement::ColumnDouble(int col) const {
  if (!is_valid()) {
    NOTREACHED();
    return 0;
  }
  return sqlite3_column_double(ref_->stmt(), col);
}

std::string Statement::ColumnString(int col) const {
  if (!is_valid()) {
    NOTREACHED();
    return 0;
  }
  const char* str = reinterpret_cast<const char*>(
      sqlite3_column_text(ref_->stmt(), col));
  int len = sqlite3_column_bytes(ref_->stmt(), col);

  std::string result;
  if (str && len > 0)
    result.assign(str, len);
  return result;
}

int Statement::ColumnByteLength(int col) const {
  if (!is_valid()) {
    NOTREACHED();
    return 0;
  }
  return sqlite3_column_bytes(ref_->stmt(), col);
}

const void* Statement::ColumnBlob(int col) const {
  if (!is_valid()) {
    NOTREACHED();
    return NULL;
  }

  return sqlite3_column_blob(ref_->stmt(), col);
}

void Statement::ColumnBlobAsVector(int col, std::vector<char>* val) const {
  val->clear();
  if (!is_valid()) {
    NOTREACHED();
    return;
  }

  const void* data = sqlite3_column_blob(ref_->stmt(), col);
  int len = sqlite3_column_bytes(ref_->stmt(), col);
  if (data && len > 0) {
    val->resize(len);
    memcpy(&(*val)[0], data, len);
  }
}

void Statement::ColumnBlobAsVector(
    int col,
    std::vector<unsigned char>* val) const {
  ColumnBlobAsVector(col, reinterpret_cast< std::vector<char>* >(val));
}

int Statement::CheckError(int err) {
  succeeded_ = (err == SQLITE_OK || err == SQLITE_ROW || err == SQLITE_DONE);

  // TODO(brettw) enhance this to process the error.
  return err;
}

}  // namespace sql
