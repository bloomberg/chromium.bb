// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/extras/sqlite/sqlite_channel_id_store.h"

#include <set>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/metrics/histogram.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_util.h"
#include "net/cert/x509_certificate.h"
#include "net/cookies/cookie_util.h"
#include "net/ssl/ssl_client_cert_type.h"
#include "sql/error_delegate_util.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "url/gurl.h"

namespace {

// Version number of the database.
const int kCurrentVersionNumber = 4;
const int kCompatibleVersionNumber = 1;

// Initializes the certs table, returning true on success.
bool InitTable(sql::Connection* db) {
  // The table is named "origin_bound_certs" for backwards compatability before
  // we renamed this class to SQLiteChannelIDStore.  Likewise, the primary
  // key is "origin", but now can be other things like a plain domain.
  if (!db->DoesTableExist("origin_bound_certs")) {
    if (!db->Execute(
            "CREATE TABLE origin_bound_certs ("
            "origin TEXT NOT NULL UNIQUE PRIMARY KEY,"
            "private_key BLOB NOT NULL,"
            "cert BLOB NOT NULL,"
            "cert_type INTEGER,"
            "expiration_time INTEGER,"
            "creation_time INTEGER)")) {
      return false;
    }
  }

  return true;
}

}  // namespace

namespace net {

// This class is designed to be shared between any calling threads and the
// background task runner. It batches operations and commits them on a timer.
class SQLiteChannelIDStore::Backend
    : public base::RefCountedThreadSafe<SQLiteChannelIDStore::Backend> {
 public:
  Backend(
      const base::FilePath& path,
      const scoped_refptr<base::SequencedTaskRunner>& background_task_runner)
      : path_(path),
        num_pending_(0),
        force_keep_session_state_(false),
        background_task_runner_(background_task_runner),
        corruption_detected_(false) {}

  // Creates or loads the SQLite database.
  void Load(const LoadedCallback& loaded_callback);

  // Batch a channel ID addition.
  void AddChannelID(const DefaultChannelIDStore::ChannelID& channel_id);

  // Batch a channel ID deletion.
  void DeleteChannelID(const DefaultChannelIDStore::ChannelID& channel_id);

  // Post background delete of all channel ids for |server_identifiers|.
  void DeleteAllInList(const std::list<std::string>& server_identifiers);

  // Commit any pending operations and close the database.  This must be called
  // before the object is destructed.
  void Close();

  void SetForceKeepSessionState();

 private:
  friend class base::RefCountedThreadSafe<SQLiteChannelIDStore::Backend>;

  // You should call Close() before destructing this object.
  virtual ~Backend() {
    DCHECK(!db_.get()) << "Close should have already been called.";
    DCHECK_EQ(0u, num_pending_);
    DCHECK(pending_.empty());
  }

  void LoadInBackground(
      ScopedVector<DefaultChannelIDStore::ChannelID>* channel_ids);

  // Database upgrade statements.
  bool EnsureDatabaseVersion();

  class PendingOperation {
   public:
    enum OperationType { CHANNEL_ID_ADD, CHANNEL_ID_DELETE };

    PendingOperation(OperationType op,
                     const DefaultChannelIDStore::ChannelID& channel_id)
        : op_(op), channel_id_(channel_id) {}

    OperationType op() const { return op_; }
    const DefaultChannelIDStore::ChannelID& channel_id() const {
      return channel_id_;
    }

   private:
    OperationType op_;
    DefaultChannelIDStore::ChannelID channel_id_;
  };

 private:
  // Batch a channel id operation (add or delete).
  void BatchOperation(PendingOperation::OperationType op,
                      const DefaultChannelIDStore::ChannelID& channel_id);
  // Commit our pending operations to the database.
  void Commit();
  // Close() executed on the background task runner.
  void InternalBackgroundClose();

  void BackgroundDeleteAllInList(
      const std::list<std::string>& server_identifiers);

  void DatabaseErrorCallback(int error, sql::Statement* stmt);
  void KillDatabase();

  const base::FilePath path_;
  scoped_ptr<sql::Connection> db_;
  sql::MetaTable meta_table_;

  typedef std::list<PendingOperation*> PendingOperationsList;
  PendingOperationsList pending_;
  PendingOperationsList::size_type num_pending_;
  // True if the persistent store should skip clear on exit rules.
  bool force_keep_session_state_;
  // Guard |pending_|, |num_pending_| and |force_keep_session_state_|.
  base::Lock lock_;

  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  // Indicates if the kill-database callback has been scheduled.
  bool corruption_detected_;

  DISALLOW_COPY_AND_ASSIGN(Backend);
};

void SQLiteChannelIDStore::Backend::Load(
    const LoadedCallback& loaded_callback) {
  // This function should be called only once per instance.
  DCHECK(!db_.get());
  scoped_ptr<ScopedVector<DefaultChannelIDStore::ChannelID> > channel_ids(
      new ScopedVector<DefaultChannelIDStore::ChannelID>());
  ScopedVector<DefaultChannelIDStore::ChannelID>* channel_ids_ptr =
      channel_ids.get();

  background_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&Backend::LoadInBackground, this, channel_ids_ptr),
      base::Bind(loaded_callback, base::Passed(&channel_ids)));
}

void SQLiteChannelIDStore::Backend::LoadInBackground(
    ScopedVector<DefaultChannelIDStore::ChannelID>* channel_ids) {
  DCHECK(background_task_runner_->RunsTasksOnCurrentThread());

  // This method should be called only once per instance.
  DCHECK(!db_.get());

  base::TimeTicks start = base::TimeTicks::Now();

  // Ensure the parent directory for storing certs is created before reading
  // from it.
  const base::FilePath dir = path_.DirName();
  if (!base::PathExists(dir) && !base::CreateDirectory(dir))
    return;

  int64 db_size = 0;
  if (base::GetFileSize(path_, &db_size))
    UMA_HISTOGRAM_COUNTS("DomainBoundCerts.DBSizeInKB", db_size / 1024);

  db_.reset(new sql::Connection);
  db_->set_histogram_tag("DomainBoundCerts");

  // Unretained to avoid a ref loop with db_.
  db_->set_error_callback(
      base::Bind(&SQLiteChannelIDStore::Backend::DatabaseErrorCallback,
                 base::Unretained(this)));

  if (!db_->Open(path_)) {
    NOTREACHED() << "Unable to open cert DB.";
    if (corruption_detected_)
      KillDatabase();
    db_.reset();
    return;
  }

  if (!EnsureDatabaseVersion() || !InitTable(db_.get())) {
    NOTREACHED() << "Unable to open cert DB.";
    if (corruption_detected_)
      KillDatabase();
    meta_table_.Reset();
    db_.reset();
    return;
  }

  db_->Preload();

  // Slurp all the certs into the out-vector.
  sql::Statement smt(db_->GetUniqueStatement(
      "SELECT origin, private_key, cert, cert_type, expiration_time, "
      "creation_time FROM origin_bound_certs"));
  if (!smt.is_valid()) {
    if (corruption_detected_)
      KillDatabase();
    meta_table_.Reset();
    db_.reset();
    return;
  }

  while (smt.Step()) {
    SSLClientCertType type = static_cast<SSLClientCertType>(smt.ColumnInt(3));
    if (type != CLIENT_CERT_ECDSA_SIGN)
      continue;
    std::string private_key_from_db, cert_from_db;
    smt.ColumnBlobAsString(1, &private_key_from_db);
    smt.ColumnBlobAsString(2, &cert_from_db);
    scoped_ptr<DefaultChannelIDStore::ChannelID> channel_id(
        new DefaultChannelIDStore::ChannelID(
            smt.ColumnString(0),  // origin
            base::Time::FromInternalValue(smt.ColumnInt64(5)),
            base::Time::FromInternalValue(smt.ColumnInt64(4)),
            private_key_from_db,
            cert_from_db));
    channel_ids->push_back(channel_id.release());
  }

  UMA_HISTOGRAM_COUNTS_10000(
      "DomainBoundCerts.DBLoadedCount",
      static_cast<base::HistogramBase::Sample>(channel_ids->size()));
  base::TimeDelta load_time = base::TimeTicks::Now() - start;
  UMA_HISTOGRAM_CUSTOM_TIMES("DomainBoundCerts.DBLoadTime",
                             load_time,
                             base::TimeDelta::FromMilliseconds(1),
                             base::TimeDelta::FromMinutes(1),
                             50);
  DVLOG(1) << "loaded " << channel_ids->size() << " in "
           << load_time.InMilliseconds() << " ms";
}

bool SQLiteChannelIDStore::Backend::EnsureDatabaseVersion() {
  // Version check.
  if (!meta_table_.Init(
          db_.get(), kCurrentVersionNumber, kCompatibleVersionNumber)) {
    return false;
  }

  if (meta_table_.GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    LOG(WARNING) << "Server bound cert database is too new.";
    return false;
  }

  int cur_version = meta_table_.GetVersionNumber();
  if (cur_version == 1) {
    sql::Transaction transaction(db_.get());
    if (!transaction.Begin())
      return false;
    if (!db_->Execute(
            "ALTER TABLE origin_bound_certs ADD COLUMN cert_type "
            "INTEGER")) {
      LOG(WARNING) << "Unable to update server bound cert database to "
                   << "version 2.";
      return false;
    }
    // All certs in version 1 database are rsa_sign, which are unsupported.
    // Just discard them all.
    if (!db_->Execute("DELETE from origin_bound_certs")) {
      LOG(WARNING) << "Unable to update server bound cert database to "
                   << "version 2.";
      return false;
    }
    ++cur_version;
    meta_table_.SetVersionNumber(cur_version);
    meta_table_.SetCompatibleVersionNumber(
        std::min(cur_version, kCompatibleVersionNumber));
    transaction.Commit();
  }

  if (cur_version <= 3) {
    sql::Transaction transaction(db_.get());
    if (!transaction.Begin())
      return false;

    if (cur_version == 2) {
      if (!db_->Execute(
              "ALTER TABLE origin_bound_certs ADD COLUMN "
              "expiration_time INTEGER")) {
        LOG(WARNING) << "Unable to update server bound cert database to "
                     << "version 4.";
        return false;
      }
    }

    if (!db_->Execute(
            "ALTER TABLE origin_bound_certs ADD COLUMN "
            "creation_time INTEGER")) {
      LOG(WARNING) << "Unable to update server bound cert database to "
                   << "version 4.";
      return false;
    }

    sql::Statement statement(
        db_->GetUniqueStatement("SELECT origin, cert FROM origin_bound_certs"));
    sql::Statement update_expires_statement(db_->GetUniqueStatement(
        "UPDATE origin_bound_certs SET expiration_time = ? WHERE origin = ?"));
    sql::Statement update_creation_statement(db_->GetUniqueStatement(
        "UPDATE origin_bound_certs SET creation_time = ? WHERE origin = ?"));
    if (!statement.is_valid() || !update_expires_statement.is_valid() ||
        !update_creation_statement.is_valid()) {
      LOG(WARNING) << "Unable to update server bound cert database to "
                   << "version 4.";
      return false;
    }

    while (statement.Step()) {
      std::string origin = statement.ColumnString(0);
      std::string cert_from_db;
      statement.ColumnBlobAsString(1, &cert_from_db);
      // Parse the cert and extract the real value and then update the DB.
      scoped_refptr<X509Certificate> cert(X509Certificate::CreateFromBytes(
          cert_from_db.data(), static_cast<int>(cert_from_db.size())));
      if (cert.get()) {
        if (cur_version == 2) {
          update_expires_statement.Reset(true);
          update_expires_statement.BindInt64(
              0, cert->valid_expiry().ToInternalValue());
          update_expires_statement.BindString(1, origin);
          if (!update_expires_statement.Run()) {
            LOG(WARNING) << "Unable to update server bound cert database to "
                         << "version 4.";
            return false;
          }
        }

        update_creation_statement.Reset(true);
        update_creation_statement.BindInt64(
            0, cert->valid_start().ToInternalValue());
        update_creation_statement.BindString(1, origin);
        if (!update_creation_statement.Run()) {
          LOG(WARNING) << "Unable to update server bound cert database to "
                       << "version 4.";
          return false;
        }
      } else {
        // If there's a cert we can't parse, just leave it.  It'll get replaced
        // with a new one if we ever try to use it.
        LOG(WARNING) << "Error parsing cert for database upgrade for origin "
                     << statement.ColumnString(0);
      }
    }

    cur_version = 4;
    meta_table_.SetVersionNumber(cur_version);
    meta_table_.SetCompatibleVersionNumber(
        std::min(cur_version, kCompatibleVersionNumber));
    transaction.Commit();
  }

  // Put future migration cases here.

  // When the version is too old, we just try to continue anyway, there should
  // not be a released product that makes a database too old for us to handle.
  LOG_IF(WARNING, cur_version < kCurrentVersionNumber)
      << "Server bound cert database version " << cur_version
      << " is too old to handle.";

  return true;
}

void SQLiteChannelIDStore::Backend::DatabaseErrorCallback(
    int error,
    sql::Statement* stmt) {
  DCHECK(background_task_runner_->RunsTasksOnCurrentThread());

  if (!sql::IsErrorCatastrophic(error))
    return;

  // TODO(shess): Running KillDatabase() multiple times should be
  // safe.
  if (corruption_detected_)
    return;

  corruption_detected_ = true;

  // TODO(shess): Consider just calling RazeAndClose() immediately.
  // db_ may not be safe to reset at this point, but RazeAndClose()
  // would cause the stack to unwind safely with errors.
  background_task_runner_->PostTask(FROM_HERE,
                                    base::Bind(&Backend::KillDatabase, this));
}

void SQLiteChannelIDStore::Backend::KillDatabase() {
  DCHECK(background_task_runner_->RunsTasksOnCurrentThread());

  if (db_) {
    // This Backend will now be in-memory only. In a future run the database
    // will be recreated. Hopefully things go better then!
    bool success = db_->RazeAndClose();
    UMA_HISTOGRAM_BOOLEAN("DomainBoundCerts.KillDatabaseResult", success);
    meta_table_.Reset();
    db_.reset();
  }
}

void SQLiteChannelIDStore::Backend::AddChannelID(
    const DefaultChannelIDStore::ChannelID& channel_id) {
  BatchOperation(PendingOperation::CHANNEL_ID_ADD, channel_id);
}

void SQLiteChannelIDStore::Backend::DeleteChannelID(
    const DefaultChannelIDStore::ChannelID& channel_id) {
  BatchOperation(PendingOperation::CHANNEL_ID_DELETE, channel_id);
}

void SQLiteChannelIDStore::Backend::DeleteAllInList(
    const std::list<std::string>& server_identifiers) {
  if (server_identifiers.empty())
    return;
  // Perform deletion on background task runner.
  background_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &Backend::BackgroundDeleteAllInList, this, server_identifiers));
}

void SQLiteChannelIDStore::Backend::BatchOperation(
    PendingOperation::OperationType op,
    const DefaultChannelIDStore::ChannelID& channel_id) {
  // Commit every 30 seconds.
  static const int kCommitIntervalMs = 30 * 1000;
  // Commit right away if we have more than 512 outstanding operations.
  static const size_t kCommitAfterBatchSize = 512;

  // We do a full copy of the cert here, and hopefully just here.
  scoped_ptr<PendingOperation> po(new PendingOperation(op, channel_id));

  PendingOperationsList::size_type num_pending;
  {
    base::AutoLock locked(lock_);
    pending_.push_back(po.release());
    num_pending = ++num_pending_;
  }

  if (num_pending == 1) {
    // We've gotten our first entry for this batch, fire off the timer.
    background_task_runner_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&Backend::Commit, this),
        base::TimeDelta::FromMilliseconds(kCommitIntervalMs));
  } else if (num_pending == kCommitAfterBatchSize) {
    // We've reached a big enough batch, fire off a commit now.
    background_task_runner_->PostTask(FROM_HERE,
                                      base::Bind(&Backend::Commit, this));
  }
}

void SQLiteChannelIDStore::Backend::Commit() {
  DCHECK(background_task_runner_->RunsTasksOnCurrentThread());

  PendingOperationsList ops;
  {
    base::AutoLock locked(lock_);
    pending_.swap(ops);
    num_pending_ = 0;
  }

  // Maybe an old timer fired or we are already Close()'ed.
  if (!db_.get() || ops.empty())
    return;

  sql::Statement add_statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO origin_bound_certs (origin, private_key, cert, cert_type, "
      "expiration_time, creation_time) VALUES (?,?,?,?,?,?)"));
  if (!add_statement.is_valid())
    return;

  sql::Statement del_statement(db_->GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM origin_bound_certs WHERE origin=?"));
  if (!del_statement.is_valid())
    return;

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return;

  for (PendingOperationsList::iterator it = ops.begin(); it != ops.end();
       ++it) {
    // Free the certs as we commit them to the database.
    scoped_ptr<PendingOperation> po(*it);
    switch (po->op()) {
      case PendingOperation::CHANNEL_ID_ADD: {
        add_statement.Reset(true);
        add_statement.BindString(0, po->channel_id().server_identifier());
        const std::string& private_key = po->channel_id().private_key();
        add_statement.BindBlob(
            1, private_key.data(), static_cast<int>(private_key.size()));
        const std::string& cert = po->channel_id().cert();
        add_statement.BindBlob(2, cert.data(), static_cast<int>(cert.size()));
        add_statement.BindInt(3, CLIENT_CERT_ECDSA_SIGN);
        add_statement.BindInt64(
            4, po->channel_id().expiration_time().ToInternalValue());
        add_statement.BindInt64(
            5, po->channel_id().creation_time().ToInternalValue());
        if (!add_statement.Run())
          NOTREACHED() << "Could not add a server bound cert to the DB.";
        break;
      }
      case PendingOperation::CHANNEL_ID_DELETE:
        del_statement.Reset(true);
        del_statement.BindString(0, po->channel_id().server_identifier());
        if (!del_statement.Run())
          NOTREACHED() << "Could not delete a server bound cert from the DB.";
        break;

      default:
        NOTREACHED();
        break;
    }
  }
  transaction.Commit();
}

// Fire off a close message to the background task runner. We could still have a
// pending commit timer that will be holding a reference on us, but if/when
// this fires we will already have been cleaned up and it will be ignored.
void SQLiteChannelIDStore::Backend::Close() {
  // Must close the backend on the background task runner.
  background_task_runner_->PostTask(
      FROM_HERE, base::Bind(&Backend::InternalBackgroundClose, this));
}

void SQLiteChannelIDStore::Backend::InternalBackgroundClose() {
  DCHECK(background_task_runner_->RunsTasksOnCurrentThread());
  // Commit any pending operations
  Commit();
  db_.reset();
}

void SQLiteChannelIDStore::Backend::BackgroundDeleteAllInList(
    const std::list<std::string>& server_identifiers) {
  DCHECK(background_task_runner_->RunsTasksOnCurrentThread());

  if (!db_.get())
    return;

  sql::Statement del_smt(db_->GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM origin_bound_certs WHERE origin=?"));
  if (!del_smt.is_valid()) {
    LOG(WARNING) << "Unable to delete channel ids.";
    return;
  }

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin()) {
    LOG(WARNING) << "Unable to delete channel ids.";
    return;
  }

  for (std::list<std::string>::const_iterator it = server_identifiers.begin();
       it != server_identifiers.end();
       ++it) {
    del_smt.Reset(true);
    del_smt.BindString(0, *it);
    if (!del_smt.Run())
      NOTREACHED() << "Could not delete a channel id from the DB.";
  }

  if (!transaction.Commit())
    LOG(WARNING) << "Unable to delete channel ids.";
}

void SQLiteChannelIDStore::Backend::SetForceKeepSessionState() {
  base::AutoLock locked(lock_);
  force_keep_session_state_ = true;
}

SQLiteChannelIDStore::SQLiteChannelIDStore(
    const base::FilePath& path,
    const scoped_refptr<base::SequencedTaskRunner>& background_task_runner)
    : backend_(new Backend(path, background_task_runner)) {
}

void SQLiteChannelIDStore::Load(const LoadedCallback& loaded_callback) {
  backend_->Load(loaded_callback);
}

void SQLiteChannelIDStore::AddChannelID(
    const DefaultChannelIDStore::ChannelID& channel_id) {
  backend_->AddChannelID(channel_id);
}

void SQLiteChannelIDStore::DeleteChannelID(
    const DefaultChannelIDStore::ChannelID& channel_id) {
  backend_->DeleteChannelID(channel_id);
}

void SQLiteChannelIDStore::DeleteAllInList(
    const std::list<std::string>& server_identifiers) {
  backend_->DeleteAllInList(server_identifiers);
}

void SQLiteChannelIDStore::SetForceKeepSessionState() {
  backend_->SetForceKeepSessionState();
}

SQLiteChannelIDStore::~SQLiteChannelIDStore() {
  backend_->Close();
  // We release our reference to the Backend, though it will probably still have
  // a reference if the background task runner has not run Close() yet.
}

}  // namespace net
