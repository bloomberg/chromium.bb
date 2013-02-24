// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/sqlite_server_bound_cert_store.h"

#include <list>
#include <set>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/diagnostics/sqlite_diagnostics.h"
#include "chrome/browser/net/clear_on_exit_policy.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/ssl_client_cert_type.h"
#include "net/base/x509_certificate.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"

using content::BrowserThread;

// This class is designed to be shared between any calling threads and the
// database thread.  It batches operations and commits them on a timer.
class SQLiteServerBoundCertStore::Backend
    : public base::RefCountedThreadSafe<SQLiteServerBoundCertStore::Backend> {
 public:
  Backend(const base::FilePath& path, ClearOnExitPolicy* clear_on_exit_policy)
      : path_(path),
        db_(NULL),
        num_pending_(0),
        force_keep_session_state_(false),
        clear_on_exit_policy_(clear_on_exit_policy) {
  }

  // Creates or loads the SQLite database.
  void Load(const LoadedCallback& loaded_callback);

  // Batch a server bound cert addition.
  void AddServerBoundCert(
      const net::DefaultServerBoundCertStore::ServerBoundCert& cert);

  // Batch a server bound cert deletion.
  void DeleteServerBoundCert(
      const net::DefaultServerBoundCertStore::ServerBoundCert& cert);

  // Commit pending operations as soon as possible.
  void Flush(const base::Closure& completion_task);

  // Commit any pending operations and close the database.  This must be called
  // before the object is destructed.
  void Close();

  void SetForceKeepSessionState();

 private:
  void LoadOnDBThreadAndNotify(const LoadedCallback& loaded_callback);
  void LoadOnDBThread(
      std::vector<net::DefaultServerBoundCertStore::ServerBoundCert*>* certs);

  friend class base::RefCountedThreadSafe<SQLiteServerBoundCertStore::Backend>;

  // You should call Close() before destructing this object.
  ~Backend() {
    DCHECK(!db_.get()) << "Close should have already been called.";
    DCHECK(num_pending_ == 0 && pending_.empty());
  }

  // Database upgrade statements.
  bool EnsureDatabaseVersion();

  class PendingOperation {
   public:
    typedef enum {
      CERT_ADD,
      CERT_DELETE
    } OperationType;

    PendingOperation(
        OperationType op,
        const net::DefaultServerBoundCertStore::ServerBoundCert& cert)
        : op_(op), cert_(cert) {}

    OperationType op() const { return op_; }
    const net::DefaultServerBoundCertStore::ServerBoundCert& cert() const {
        return cert_;
    }

   private:
    OperationType op_;
    net::DefaultServerBoundCertStore::ServerBoundCert cert_;
  };

 private:
  // Batch a server bound cert operation (add or delete)
  void BatchOperation(
      PendingOperation::OperationType op,
      const net::DefaultServerBoundCertStore::ServerBoundCert& cert);
  // Commit our pending operations to the database.
  void Commit();
  // Close() executed on the background thread.
  void InternalBackgroundClose();

  void DeleteCertificatesOnShutdown();

  base::FilePath path_;
  scoped_ptr<sql::Connection> db_;
  sql::MetaTable meta_table_;

  typedef std::list<PendingOperation*> PendingOperationsList;
  PendingOperationsList pending_;
  PendingOperationsList::size_type num_pending_;
  // True if the persistent store should skip clear on exit rules.
  bool force_keep_session_state_;
  // Guard |pending_|, |num_pending_| and |force_keep_session_state_|.
  base::Lock lock_;

  // Cache of origins we have certificates stored for.
  std::set<std::string> cert_origins_;

  scoped_refptr<ClearOnExitPolicy> clear_on_exit_policy_;

  DISALLOW_COPY_AND_ASSIGN(Backend);
};

// Version number of the database.
static const int kCurrentVersionNumber = 4;
static const int kCompatibleVersionNumber = 1;

namespace {

// Initializes the certs table, returning true on success.
bool InitTable(sql::Connection* db) {
  // The table is named "origin_bound_certs" for backwards compatability before
  // we renamed this class to SQLiteServerBoundCertStore.  Likewise, the primary
  // key is "origin", but now can be other things like a plain domain.
  if (!db->DoesTableExist("origin_bound_certs")) {
    if (!db->Execute("CREATE TABLE origin_bound_certs ("
                     "origin TEXT NOT NULL UNIQUE PRIMARY KEY,"
                     "private_key BLOB NOT NULL,"
                     "cert BLOB NOT NULL,"
                     "cert_type INTEGER,"
                     "expiration_time INTEGER,"
                     "creation_time INTEGER)"))
      return false;
  }

  return true;
}

}  // namespace

void SQLiteServerBoundCertStore::Backend::Load(
    const LoadedCallback& loaded_callback) {
  // This function should be called only once per instance.
  DCHECK(!db_.get());

  BrowserThread::PostTask(
      BrowserThread::DB, FROM_HERE,
      base::Bind(&Backend::LoadOnDBThreadAndNotify, this, loaded_callback));
}

void SQLiteServerBoundCertStore::Backend::LoadOnDBThreadAndNotify(
    const LoadedCallback& loaded_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  scoped_ptr<ScopedVector<net::DefaultServerBoundCertStore::ServerBoundCert> >
      certs(new ScopedVector<net::DefaultServerBoundCertStore::ServerBoundCert>(
          ));

  LoadOnDBThread(&certs->get());

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(loaded_callback, base::Passed(&certs)));
}

void SQLiteServerBoundCertStore::Backend::LoadOnDBThread(
    std::vector<net::DefaultServerBoundCertStore::ServerBoundCert*>* certs) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));

  // This method should be called only once per instance.
  DCHECK(!db_.get());

  base::TimeTicks start = base::TimeTicks::Now();

  // Ensure the parent directory for storing certs is created before reading
  // from it.
  const base::FilePath dir = path_.DirName();
  if (!file_util::PathExists(dir) && !file_util::CreateDirectory(dir))
    return;

  int64 db_size = 0;
  if (file_util::GetFileSize(path_, &db_size))
    UMA_HISTOGRAM_COUNTS("DomainBoundCerts.DBSizeInKB", db_size / 1024 );

  db_.reset(new sql::Connection);
  if (!db_->Open(path_)) {
    NOTREACHED() << "Unable to open cert DB.";
    db_.reset();
    return;
  }

  if (!EnsureDatabaseVersion() || !InitTable(db_.get())) {
    NOTREACHED() << "Unable to open cert DB.";
    db_.reset();
    return;
  }

  db_->Preload();

  // Slurp all the certs into the out-vector.
  sql::Statement smt(db_->GetUniqueStatement(
      "SELECT origin, private_key, cert, cert_type, expiration_time, "
      "creation_time FROM origin_bound_certs"));
  if (!smt.is_valid()) {
    db_.reset();
    return;
  }

  while (smt.Step()) {
    std::string private_key_from_db, cert_from_db;
    smt.ColumnBlobAsString(1, &private_key_from_db);
    smt.ColumnBlobAsString(2, &cert_from_db);
    scoped_ptr<net::DefaultServerBoundCertStore::ServerBoundCert> cert(
        new net::DefaultServerBoundCertStore::ServerBoundCert(
            smt.ColumnString(0),  // origin
            static_cast<net::SSLClientCertType>(smt.ColumnInt(3)),
            base::Time::FromInternalValue(smt.ColumnInt64(5)),
            base::Time::FromInternalValue(smt.ColumnInt64(4)),
            private_key_from_db,
            cert_from_db));
    cert_origins_.insert(cert->server_identifier());
    certs->push_back(cert.release());
  }

  UMA_HISTOGRAM_COUNTS_10000("DomainBoundCerts.DBLoadedCount", certs->size());
  base::TimeDelta load_time = base::TimeTicks::Now() - start;
  UMA_HISTOGRAM_CUSTOM_TIMES("DomainBoundCerts.DBLoadTime",
                             load_time,
                             base::TimeDelta::FromMilliseconds(1),
                             base::TimeDelta::FromMinutes(1),
                             50);
  DVLOG(1) << "loaded " << certs->size() << " in " << load_time.InMilliseconds()
           << " ms";
}

bool SQLiteServerBoundCertStore::Backend::EnsureDatabaseVersion() {
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
    if (!db_->Execute("ALTER TABLE origin_bound_certs ADD COLUMN cert_type "
                      "INTEGER")) {
      LOG(WARNING) << "Unable to update server bound cert database to "
                   << "version 2.";
      return false;
    }
    // All certs in version 1 database are rsa_sign, which has a value of 1.
    if (!db_->Execute("UPDATE origin_bound_certs SET cert_type = 1")) {
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
      if (!db_->Execute("ALTER TABLE origin_bound_certs ADD COLUMN "
                        "expiration_time INTEGER")) {
        LOG(WARNING) << "Unable to update server bound cert database to "
                     << "version 4.";
        return false;
      }
    }

    if (!db_->Execute("ALTER TABLE origin_bound_certs ADD COLUMN "
                      "creation_time INTEGER")) {
      LOG(WARNING) << "Unable to update server bound cert database to "
                   << "version 4.";
      return false;
    }

    sql::Statement smt(db_->GetUniqueStatement(
        "SELECT origin, cert FROM origin_bound_certs"));
    sql::Statement update_expires_smt(db_->GetUniqueStatement(
        "UPDATE origin_bound_certs SET expiration_time = ? WHERE origin = ?"));
    sql::Statement update_creation_smt(db_->GetUniqueStatement(
        "UPDATE origin_bound_certs SET creation_time = ? WHERE origin = ?"));
    if (!smt.is_valid() ||
        !update_expires_smt.is_valid() ||
        !update_creation_smt.is_valid()) {
      LOG(WARNING) << "Unable to update server bound cert database to "
                   << "version 4.";
      return false;
    }

    while (smt.Step()) {
      std::string origin = smt.ColumnString(0);
      std::string cert_from_db;
      smt.ColumnBlobAsString(1, &cert_from_db);
      // Parse the cert and extract the real value and then update the DB.
      scoped_refptr<net::X509Certificate> cert(
          net::X509Certificate::CreateFromBytes(
              cert_from_db.data(), cert_from_db.size()));
      if (cert) {
        if (cur_version == 2) {
          update_expires_smt.Reset(true);
          update_expires_smt.BindInt64(0,
                                       cert->valid_expiry().ToInternalValue());
          update_expires_smt.BindString(1, origin);
          if (!update_expires_smt.Run()) {
            LOG(WARNING) << "Unable to update server bound cert database to "
                         << "version 4.";
            return false;
          }
        }

        update_creation_smt.Reset(true);
        update_creation_smt.BindInt64(0, cert->valid_start().ToInternalValue());
        update_creation_smt.BindString(1, origin);
        if (!update_creation_smt.Run()) {
          LOG(WARNING) << "Unable to update server bound cert database to "
                       << "version 4.";
          return false;
        }
      } else {
        // If there's a cert we can't parse, just leave it.  It'll get replaced
        // with a new one if we ever try to use it.
        LOG(WARNING) << "Error parsing cert for database upgrade for origin "
                     << smt.ColumnString(0);
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
  LOG_IF(WARNING, cur_version < kCurrentVersionNumber) <<
      "Server bound cert database version " << cur_version <<
      " is too old to handle.";

  return true;
}

void SQLiteServerBoundCertStore::Backend::AddServerBoundCert(
    const net::DefaultServerBoundCertStore::ServerBoundCert& cert) {
  BatchOperation(PendingOperation::CERT_ADD, cert);
}

void SQLiteServerBoundCertStore::Backend::DeleteServerBoundCert(
    const net::DefaultServerBoundCertStore::ServerBoundCert& cert) {
  BatchOperation(PendingOperation::CERT_DELETE, cert);
}

void SQLiteServerBoundCertStore::Backend::BatchOperation(
    PendingOperation::OperationType op,
    const net::DefaultServerBoundCertStore::ServerBoundCert& cert) {
  // Commit every 30 seconds.
  static const int kCommitIntervalMs = 30 * 1000;
  // Commit right away if we have more than 512 outstanding operations.
  static const size_t kCommitAfterBatchSize = 512;
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::DB));

  // We do a full copy of the cert here, and hopefully just here.
  scoped_ptr<PendingOperation> po(new PendingOperation(op, cert));

  PendingOperationsList::size_type num_pending;
  {
    base::AutoLock locked(lock_);
    pending_.push_back(po.release());
    num_pending = ++num_pending_;
  }

  if (num_pending == 1) {
    // We've gotten our first entry for this batch, fire off the timer.
    BrowserThread::PostDelayedTask(
        BrowserThread::DB, FROM_HERE,
        base::Bind(&Backend::Commit, this),
        base::TimeDelta::FromMilliseconds(kCommitIntervalMs));
  } else if (num_pending == kCommitAfterBatchSize) {
    // We've reached a big enough batch, fire off a commit now.
    BrowserThread::PostTask(
        BrowserThread::DB, FROM_HERE,
        base::Bind(&Backend::Commit, this));
  }
}

void SQLiteServerBoundCertStore::Backend::Commit() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));

  PendingOperationsList ops;
  {
    base::AutoLock locked(lock_);
    pending_.swap(ops);
    num_pending_ = 0;
  }

  // Maybe an old timer fired or we are already Close()'ed.
  if (!db_.get() || ops.empty())
    return;

  sql::Statement add_smt(db_->GetCachedStatement(SQL_FROM_HERE,
      "INSERT INTO origin_bound_certs (origin, private_key, cert, cert_type, "
      "expiration_time, creation_time) VALUES (?,?,?,?,?,?)"));
  if (!add_smt.is_valid())
    return;

  sql::Statement del_smt(db_->GetCachedStatement(SQL_FROM_HERE,
                             "DELETE FROM origin_bound_certs WHERE origin=?"));
  if (!del_smt.is_valid())
    return;

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return;

  for (PendingOperationsList::iterator it = ops.begin();
       it != ops.end(); ++it) {
    // Free the certs as we commit them to the database.
    scoped_ptr<PendingOperation> po(*it);
    switch (po->op()) {
      case PendingOperation::CERT_ADD: {
        cert_origins_.insert(po->cert().server_identifier());
        add_smt.Reset(true);
        add_smt.BindString(0, po->cert().server_identifier());
        const std::string& private_key = po->cert().private_key();
        add_smt.BindBlob(1, private_key.data(), private_key.size());
        const std::string& cert = po->cert().cert();
        add_smt.BindBlob(2, cert.data(), cert.size());
        add_smt.BindInt(3, po->cert().type());
        add_smt.BindInt64(4, po->cert().expiration_time().ToInternalValue());
        add_smt.BindInt64(5, po->cert().creation_time().ToInternalValue());
        if (!add_smt.Run())
          NOTREACHED() << "Could not add a server bound cert to the DB.";
        break;
      }
      case PendingOperation::CERT_DELETE:
        cert_origins_.erase(po->cert().server_identifier());
        del_smt.Reset(true);
        del_smt.BindString(0, po->cert().server_identifier());
        if (!del_smt.Run())
          NOTREACHED() << "Could not delete a server bound cert from the DB.";
        break;

      default:
        NOTREACHED();
        break;
    }
  }
  transaction.Commit();
}

void SQLiteServerBoundCertStore::Backend::Flush(
    const base::Closure& completion_task) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::DB));
  BrowserThread::PostTask(
      BrowserThread::DB, FROM_HERE, base::Bind(&Backend::Commit, this));
  if (!completion_task.is_null()) {
    // We want the completion task to run immediately after Commit() returns.
    // Posting it from here means there is less chance of another task getting
    // onto the message queue first, than if we posted it from Commit() itself.
    BrowserThread::PostTask(BrowserThread::DB, FROM_HERE, completion_task);
  }
}

// Fire off a close message to the background thread. We could still have a
// pending commit timer that will be holding a reference on us, but if/when
// this fires we will already have been cleaned up and it will be ignored.
void SQLiteServerBoundCertStore::Backend::Close() {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::DB));
  // Must close the backend on the background thread.
  BrowserThread::PostTask(
      BrowserThread::DB, FROM_HERE,
      base::Bind(&Backend::InternalBackgroundClose, this));
}

void SQLiteServerBoundCertStore::Backend::InternalBackgroundClose() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  // Commit any pending operations
  Commit();

  if (!force_keep_session_state_ && clear_on_exit_policy_.get() &&
      clear_on_exit_policy_->HasClearOnExitOrigins()) {
    DeleteCertificatesOnShutdown();
  }

  db_.reset();
}

void SQLiteServerBoundCertStore::Backend::DeleteCertificatesOnShutdown() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));

  if (!db_.get())
    return;

  if (cert_origins_.empty())
    return;

  sql::Statement del_smt(db_->GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM origin_bound_certs WHERE origin=?"));
  if (!del_smt.is_valid()) {
    LOG(WARNING) << "Unable to delete certificates on shutdown.";
    return;
  }

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin()) {
    LOG(WARNING) << "Unable to delete certificates on shutdown.";
    return;
  }

  for (std::set<std::string>::iterator it = cert_origins_.begin();
       it != cert_origins_.end(); ++it) {
    if (!clear_on_exit_policy_->ShouldClearOriginOnExit(*it, true))
      continue;
    del_smt.Reset(true);
    del_smt.BindString(0, *it);
    if (!del_smt.Run())
      NOTREACHED() << "Could not delete a certificate from the DB.";
  }

  if (!transaction.Commit())
    LOG(WARNING) << "Unable to delete certificates on shutdown.";
}

void SQLiteServerBoundCertStore::Backend::SetForceKeepSessionState() {
  base::AutoLock locked(lock_);
  force_keep_session_state_ = true;
}

SQLiteServerBoundCertStore::SQLiteServerBoundCertStore(
    const base::FilePath& path,
    ClearOnExitPolicy* clear_on_exit_policy)
    : backend_(new Backend(path, clear_on_exit_policy)) {
}

void SQLiteServerBoundCertStore::Load(
    const LoadedCallback& loaded_callback) {
  backend_->Load(loaded_callback);
}

void SQLiteServerBoundCertStore::AddServerBoundCert(
    const net::DefaultServerBoundCertStore::ServerBoundCert& cert) {
  backend_->AddServerBoundCert(cert);
}

void SQLiteServerBoundCertStore::DeleteServerBoundCert(
    const net::DefaultServerBoundCertStore::ServerBoundCert& cert) {
  backend_->DeleteServerBoundCert(cert);
}

void SQLiteServerBoundCertStore::SetForceKeepSessionState() {
  backend_->SetForceKeepSessionState();
}

void SQLiteServerBoundCertStore::Flush(const base::Closure& completion_task) {
  backend_->Flush(completion_task);
}

SQLiteServerBoundCertStore::~SQLiteServerBoundCertStore() {
  backend_->Close();
  // We release our reference to the Backend, though it will probably still have
  // a reference if the background thread has not run Close() yet.
}
