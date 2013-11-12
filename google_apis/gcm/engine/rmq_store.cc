// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/rmq_store.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/tracked_objects.h"
#include "components/webdata/encryptor/encryptor.h"
#include "google_apis/gcm/base/mcs_message.h"
#include "google_apis/gcm/base/mcs_util.h"
#include "google_apis/gcm/protocol/mcs.pb.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

namespace gcm {

namespace {

// ---- LevelDB keys. ----
// Key for this device's android id.
const char kDeviceAIDKey[] = "device_aid_key";
// Key for this device's android security token.
const char kDeviceTokenKey[] = "device_token_key";
// Lowest lexicographically ordered incoming message key.
// Used for prefixing messages.
const char kIncomingMsgKeyStart[] = "incoming1-";
// Key guaranteed to be higher than all incoming message keys.
// Used for limiting iteration.
const char kIncomingMsgKeyEnd[] = "incoming2-";
// Lowest lexicographically ordered outgoing message key.
// Used for prefixing outgoing messages.
const char kOutgoingMsgKeyStart[] = "outgoing1-";
// Key guaranteed to be higher than all outgoing message keys.
// Used for limiting iteration.
const char kOutgoingMsgKeyEnd[] = "outgoing2-";

std::string MakeIncomingKey(const std::string& persistent_id) {
  return kIncomingMsgKeyStart + persistent_id;
}

std::string MakeOutgoingKey(const std::string& persistent_id) {
  return kOutgoingMsgKeyStart + persistent_id;
}

std::string ParseOutgoingKey(const std::string& key) {
  return key.substr(arraysize(kOutgoingMsgKeyStart) - 1);
}

leveldb::Slice MakeSlice(const base::StringPiece& s) {
  return leveldb::Slice(s.begin(), s.size());
}

}  // namespace

class RMQStore::Backend : public base::RefCountedThreadSafe<RMQStore::Backend> {
 public:
  Backend(const base::FilePath& path,
          scoped_refptr<base::SequencedTaskRunner> foreground_runner);

  // Blocking implementations of RMQStore methods.
  void Load(const LoadCallback& callback);
  void Destroy(const UpdateCallback& callback);
  void SetDeviceCredentials(uint64 device_android_id,
                            uint64 device_security_token,
                            const UpdateCallback& callback);
  void AddIncomingMessage(const std::string& persistent_id,
                          const UpdateCallback& callback);
  void RemoveIncomingMessages(const PersistentIdList& persistent_ids,
                              const UpdateCallback& callback);
  void AddOutgoingMessage(const std::string& persistent_id,
                          const MCSMessage& message,
                          const UpdateCallback& callback);
  void RemoveOutgoingMessages(const PersistentIdList& persistent_ids,
                              const UpdateCallback& callback);

 private:
  friend class base::RefCountedThreadSafe<Backend>;
  ~Backend();

  bool LoadDeviceCredentials(uint64* android_id, uint64* security_token);
  bool LoadIncomingMessages(std::vector<std::string>* incoming_messages);
  bool LoadOutgoingMessages(
      std::map<std::string, google::protobuf::MessageLite*>* outgoing_messages);

  const base::FilePath path_;
  scoped_refptr<base::SequencedTaskRunner> foreground_task_runner_;

  scoped_ptr<leveldb::DB> db_;
};

RMQStore::Backend::Backend(
    const base::FilePath& path,
    scoped_refptr<base::SequencedTaskRunner> foreground_task_runner)
    : path_(path),
      foreground_task_runner_(foreground_task_runner) {
}

RMQStore::Backend::~Backend() {
}

void RMQStore::Backend::Load(const LoadCallback& callback) {
  LoadResult result;

  leveldb::Options options;
  options.create_if_missing = true;
  leveldb::DB* db;
  leveldb::Status status = leveldb::DB::Open(options,
                                             path_.AsUTF8Unsafe(),
                                             &db);
  if (!status.ok()) {
    LOG(ERROR) << "Failed to open database " << path_.value()
               << ": " << status.ToString();
    foreground_task_runner_->PostTask(FROM_HERE,
                                      base::Bind(callback, result));
    return;
  }
  db_.reset(db);

  if (!LoadDeviceCredentials(&result.device_android_id,
                             &result.device_security_token) ||
      !LoadIncomingMessages(&result.incoming_messages) ||
      !LoadOutgoingMessages(&result.outgoing_messages)) {
    result.device_android_id = 0;
    result.device_security_token = 0;
    result.incoming_messages.clear();
    STLDeleteContainerPairSecondPointers(result.outgoing_messages.begin(),
                                         result.outgoing_messages.end());
    result.outgoing_messages.clear();
    foreground_task_runner_->PostTask(FROM_HERE,
                                      base::Bind(callback, result));
    return;
  }

  DVLOG(1) << "Succeeded in loading " << result.incoming_messages.size()
          << " unacknowledged incoming messages and "
          << result.outgoing_messages.size()
          << " unacknowledged outgoing messages.";
  result.success = true;
  foreground_task_runner_->PostTask(FROM_HERE,
                                    base::Bind(callback, result));
  return;
}

void RMQStore::Backend::Destroy(const UpdateCallback& callback) {
  DVLOG(1) << "Destroying RMQ store.";
  const leveldb::Status s =
      leveldb::DestroyDB(path_.AsUTF8Unsafe(),
                         leveldb::Options());
  if (s.ok()) {
    foreground_task_runner_->PostTask(FROM_HERE,
                                      base::Bind(callback, true));
    return;
  }
  LOG(ERROR) << "Destroy failed.";
  foreground_task_runner_->PostTask(FROM_HERE,
                                    base::Bind(callback, false));
}

void RMQStore::Backend::SetDeviceCredentials(uint64 device_android_id,
                                             uint64 device_security_token,
                                             const UpdateCallback& callback) {
  DVLOG(1) << "Saving device credentials with AID " << device_android_id;
  leveldb::WriteOptions write_options;
  write_options.sync = true;

  std::string encrypted_token;
  Encryptor::EncryptString(base::Uint64ToString(device_security_token),
                           &encrypted_token);
  leveldb::Status s =
      db_->Put(write_options,
               MakeSlice(kDeviceAIDKey),
               MakeSlice(base::Uint64ToString(device_android_id)));
  if (s.ok()) {
    s = db_->Put(write_options,
                 MakeSlice(kDeviceTokenKey),
                 MakeSlice(encrypted_token));
  }
  if (s.ok()) {
    foreground_task_runner_->PostTask(FROM_HERE,
                                      base::Bind(callback, true));
    return;
  }
  LOG(ERROR) << "LevelDB put failed: " << s.ToString();
  foreground_task_runner_->PostTask(FROM_HERE,
                                    base::Bind(callback, false));
}

void RMQStore::Backend::AddIncomingMessage(const std::string& persistent_id,
                                           const UpdateCallback& callback) {
  DVLOG(1) << "Saving incoming message with id " << persistent_id;
  leveldb::WriteOptions write_options;
  write_options.sync = true;

  const leveldb::Status s =
      db_->Put(write_options,
               MakeSlice(MakeIncomingKey(persistent_id)),
               MakeSlice(persistent_id));
  if (s.ok()) {
    foreground_task_runner_->PostTask(FROM_HERE,
                                      base::Bind(callback, true));
    return;
  }
  LOG(ERROR) << "LevelDB put failed: " << s.ToString();
  foreground_task_runner_->PostTask(FROM_HERE,
                                    base::Bind(callback, false));
}

void RMQStore::Backend::RemoveIncomingMessages(
    const PersistentIdList& persistent_ids,
    const UpdateCallback& callback) {
  leveldb::WriteOptions write_options;
  write_options.sync = true;

  leveldb::Status s;
  for (PersistentIdList::const_iterator iter = persistent_ids.begin();
       iter != persistent_ids.end(); ++iter){
    DVLOG(1) << "Removing incoming message with id " << *iter;
    s = db_->Delete(write_options,
                    MakeSlice(MakeIncomingKey(*iter)));
    if (!s.ok())
      break;
  }
  if (s.ok()) {
    foreground_task_runner_->PostTask(FROM_HERE,
                                      base::Bind(callback, true));
    return;
  }
  LOG(ERROR) << "LevelDB remove failed: " << s.ToString();
  foreground_task_runner_->PostTask(FROM_HERE,
                                    base::Bind(callback, false));
}

void RMQStore::Backend::AddOutgoingMessage(
   const std::string& persistent_id,
   const MCSMessage& message,
   const UpdateCallback& callback) {
  DVLOG(1) << "Saving outgoing message with id " << persistent_id;
  leveldb::WriteOptions write_options;
  write_options.sync = true;

  std::string data = static_cast<char>(message.tag()) +
      message.SerializeAsString();
  const leveldb::Status s =
      db_->Put(write_options,
               MakeSlice(MakeOutgoingKey(persistent_id)),
               MakeSlice(data));
  if (s.ok()) {
    foreground_task_runner_->PostTask(FROM_HERE,
                                      base::Bind(callback, true));
    return;
  }
  LOG(ERROR) << "LevelDB put failed: " << s.ToString();
  foreground_task_runner_->PostTask(FROM_HERE,
                                    base::Bind(callback, false));

}

void RMQStore::Backend::RemoveOutgoingMessages(
    const PersistentIdList& persistent_ids,
    const UpdateCallback& callback) {
  leveldb::WriteOptions write_options;
  write_options.sync = true;

  leveldb::Status s;
  for (PersistentIdList::const_iterator iter = persistent_ids.begin();
       iter != persistent_ids.end(); ++iter){
    DVLOG(1) << "Removing outgoing message with id " << *iter;
    s = db_->Delete(write_options,
                    MakeSlice(MakeOutgoingKey(*iter)));
    if (!s.ok())
      break;
  }
  if (s.ok()) {
    foreground_task_runner_->PostTask(FROM_HERE,
                                      base::Bind(callback, true));
    return;
  }
  LOG(ERROR) << "LevelDB remove failed: " << s.ToString();
  foreground_task_runner_->PostTask(FROM_HERE,
                                    base::Bind(callback, false));
}

bool RMQStore::Backend::LoadDeviceCredentials(uint64* android_id,
                                              uint64* security_token) {
  leveldb::ReadOptions read_options;
  read_options.verify_checksums = true;

  std::string result;
  leveldb::Status s = db_->Get(read_options,
                               MakeSlice(kDeviceAIDKey),
                               &result);
  if (s.ok()) {
    if (!base::StringToUint64(result, android_id)) {
      LOG(ERROR) << "Failed to restore device id.";
      return false;
    }
    result.clear();
    s = db_->Get(read_options,
                 MakeSlice(kDeviceTokenKey),
                 &result);
  }
  if (s.ok()) {
    std::string decrypted_token;
    Encryptor::DecryptString(result, &decrypted_token);
    if (!base::StringToUint64(decrypted_token, security_token)) {
      LOG(ERROR) << "Failed to restore security token.";
      return false;
    }
    return true;
  }

  if (s.IsNotFound()) {
    DVLOG(1) << "No credentials found.";
    return true;
  }

  LOG(ERROR) << "Error reading credentials from store.";
  return false;
}

bool RMQStore::Backend::LoadIncomingMessages(
    std::vector<std::string>* incoming_messages) {
  leveldb::ReadOptions read_options;
  read_options.verify_checksums = true;

  scoped_ptr<leveldb::Iterator> iter(db_->NewIterator(read_options));
  for (iter->Seek(MakeSlice(kIncomingMsgKeyStart));
       iter->Valid() && iter->key().ToString() < kIncomingMsgKeyEnd;
       iter->Next()) {
    leveldb::Slice s = iter->value();
    if (s.empty()) {
      LOG(ERROR) << "Error reading incoming message with key "
                 << iter->key().ToString();
      return false;
    }
    DVLOG(1) << "Found incoming message with id " << s.ToString();
    incoming_messages->push_back(s.ToString());
  }

  return true;
}

bool RMQStore::Backend::LoadOutgoingMessages(
    std::map<std::string, google::protobuf::MessageLite*>*
        outgoing_messages) {
  leveldb::ReadOptions read_options;
  read_options.verify_checksums = true;

  scoped_ptr<leveldb::Iterator> iter(db_->NewIterator(read_options));
  for (iter->Seek(MakeSlice(kOutgoingMsgKeyStart));
       iter->Valid() && iter->key().ToString() < kOutgoingMsgKeyEnd;
       iter->Next()) {
    leveldb::Slice s = iter->value();
    if (s.size() <= 1) {
      LOG(ERROR) << "Error reading incoming message with key " << s.ToString();
      return false;
    }
    uint8 tag = iter->value().data()[0];
    std::string id = ParseOutgoingKey(iter->key().ToString());
    scoped_ptr<google::protobuf::MessageLite> message(
        BuildProtobufFromTag(tag));
    if (!message.get() ||
        !message->ParseFromString(iter->value().ToString().substr(1))) {
      LOG(ERROR) << "Failed to parse outgoing message with id "
                 << id << " and tag " << tag;
      return false;
    }
    DVLOG(1) << "Found outgoing message with id " << id << " of type "
             << base::IntToString(tag);
    (*outgoing_messages)[id] = message.release();
  }

  return true;
}

RMQStore::LoadResult::LoadResult()
    : success(false),
      device_android_id(0),
      device_security_token(0) {
}
RMQStore::LoadResult::~LoadResult() {}

RMQStore::RMQStore(
    const base::FilePath& path,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner)
  : backend_(new Backend(path, base::MessageLoopProxy::current())),
    blocking_task_runner_(blocking_task_runner) {
}

RMQStore::~RMQStore() {
}

void RMQStore::Load(const LoadCallback& callback) {
  blocking_task_runner_->PostTask(FROM_HERE,
                                  base::Bind(&RMQStore::Backend::Load,
                                             backend_,
                                             callback));
}

void RMQStore::Destroy(const UpdateCallback& callback) {
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&RMQStore::Backend::Destroy,
                 backend_,
                 callback));
}

void RMQStore::SetDeviceCredentials(uint64 device_android_id,
                                    uint64 device_security_token,
                                    const UpdateCallback& callback) {
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&RMQStore::Backend::SetDeviceCredentials,
                 backend_,
                 device_android_id,
                 device_security_token,
                 callback));
}

void RMQStore::AddIncomingMessage(const std::string& persistent_id,
                                  const UpdateCallback& callback) {
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&RMQStore::Backend::AddIncomingMessage,
                 backend_,
                 persistent_id,
                 callback));
}

void RMQStore::RemoveIncomingMessage(const std::string& persistent_id,
                                     const UpdateCallback& callback) {
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&RMQStore::Backend::RemoveIncomingMessages,
                 backend_,
                 PersistentIdList(1, persistent_id),
                 callback));
}

void RMQStore::RemoveIncomingMessages(const PersistentIdList& persistent_ids,
                                      const UpdateCallback& callback) {
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&RMQStore::Backend::RemoveIncomingMessages,
                 backend_,
                 persistent_ids,
                 callback));
}

void RMQStore::AddOutgoingMessage(const std::string& persistent_id,
                                  const MCSMessage& message,
                                  const UpdateCallback& callback) {
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&RMQStore::Backend::AddOutgoingMessage,
                 backend_,
                 persistent_id,
                 message,
                 callback));
}

void RMQStore::RemoveOutgoingMessage(const std::string& persistent_id,
                                     const UpdateCallback& callback) {
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&RMQStore::Backend::RemoveOutgoingMessages,
                 backend_,
                 PersistentIdList(1, persistent_id),
                 callback));
}

void RMQStore::RemoveOutgoingMessages(const PersistentIdList& persistent_ids,
                                      const UpdateCallback& callback) {
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&RMQStore::Backend::RemoveOutgoingMessages,
                 backend_,
                 persistent_ids,
                 callback));
}

}  // namespace gcm
