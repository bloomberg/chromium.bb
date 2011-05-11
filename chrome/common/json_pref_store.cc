// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/json_pref_store.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop_proxy.h"
#include "base/values.h"
#include "content/common/json_value_serializer.h"

namespace {

// Some extensions we'll tack on to copies of the Preferences files.
const FilePath::CharType* kBadExtension = FILE_PATH_LITERAL("bad");

// Differentiates file loading between origin thread and passed
// (aka file) thread.
class FileThreadDeserializer
    : public base::RefCountedThreadSafe<FileThreadDeserializer> {
 public:
  explicit FileThreadDeserializer(JsonPrefStore* delegate,
                                  base::MessageLoopProxy* file_loop_proxy)
      : delegate_(delegate),
        file_loop_proxy_(file_loop_proxy),
        origin_loop_proxy_(base::MessageLoopProxy::CreateForCurrentThread()) {
  }

  void Start(const FilePath& path) {
    DCHECK(origin_loop_proxy_->BelongsToCurrentThread());
    file_loop_proxy_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this,
                          &FileThreadDeserializer::ReadFileAndReport,
                          path));
  }

  // Deserializes JSON on the file thread.
  void ReadFileAndReport(const FilePath& path) {
    DCHECK(file_loop_proxy_->BelongsToCurrentThread());

    value_.reset(DoReading(path, &error_, &no_dir_));

    origin_loop_proxy_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &FileThreadDeserializer::ReportOnOriginThread));
  }

  // Reports deserialization result on the origin thread.
  void ReportOnOriginThread() {
    DCHECK(origin_loop_proxy_->BelongsToCurrentThread());
    delegate_->OnFileRead(value_.release(), error_, no_dir_);
  }

  static Value* DoReading(const FilePath& path,
                          PersistentPrefStore::PrefReadError* error,
                          bool* no_dir) {
    int error_code;
    std::string error_msg;
    JSONFileValueSerializer serializer(path);
    Value* value = serializer.Deserialize(&error_code, &error_msg);
    HandleErrors(value, path, error_code, error_msg, error);
    *no_dir = !file_util::PathExists(path.DirName());
    return value;
  }

  static void HandleErrors(const Value* value,
                           const FilePath& path,
                           int error_code,
                           const std::string& error_msg,
                           PersistentPrefStore::PrefReadError* error);

 private:
  friend class base::RefCountedThreadSafe<FileThreadDeserializer>;

  bool no_dir_;
  PersistentPrefStore::PrefReadError error_;
  scoped_ptr<Value> value_;
  scoped_refptr<JsonPrefStore> delegate_;
  scoped_refptr<base::MessageLoopProxy> file_loop_proxy_;
  scoped_refptr<base::MessageLoopProxy> origin_loop_proxy_;
};

// static
void FileThreadDeserializer::HandleErrors(
    const Value* value,
    const FilePath& path,
    int error_code,
    const std::string& error_msg,
    PersistentPrefStore::PrefReadError* error) {
  *error = PersistentPrefStore::PREF_READ_ERROR_NONE;
  if (!value) {
    VLOG(1) << "Error while loading JSON file: " << error_msg;
    switch (error_code) {
      case JSONFileValueSerializer::JSON_ACCESS_DENIED:
        *error = PersistentPrefStore::PREF_READ_ERROR_ACCESS_DENIED;
        break;
      case JSONFileValueSerializer::JSON_CANNOT_READ_FILE:
        *error = PersistentPrefStore::PREF_READ_ERROR_FILE_OTHER;
        break;
      case JSONFileValueSerializer::JSON_FILE_LOCKED:
        *error = PersistentPrefStore::PREF_READ_ERROR_FILE_LOCKED;
        break;
      case JSONFileValueSerializer::JSON_NO_SUCH_FILE:
        *error = PersistentPrefStore::PREF_READ_ERROR_NO_FILE;
        break;
      default:
        *error = PersistentPrefStore::PREF_READ_ERROR_JSON_PARSE;
        // JSON errors indicate file corruption of some sort.
        // Since the file is corrupt, move it to the side and continue with
        // empty preferences.  This will result in them losing their settings.
        // We keep the old file for possible support and debugging assistance
        // as well as to detect if they're seeing these errors repeatedly.
        // TODO(erikkay) Instead, use the last known good file.
        FilePath bad = path.ReplaceExtension(kBadExtension);

        // If they've ever had a parse error before, put them in another bucket.
        // TODO(erikkay) if we keep this error checking for very long, we may
        // want to differentiate between recent and long ago errors.
        if (file_util::PathExists(bad))
          *error = PersistentPrefStore::PREF_READ_ERROR_JSON_REPEAT;
        file_util::Move(path, bad);
        break;
    }
  } else if (!value->IsType(Value::TYPE_DICTIONARY)) {
    *error = PersistentPrefStore::PREF_READ_ERROR_JSON_TYPE;
  }
}

}  // namespace

JsonPrefStore::JsonPrefStore(const FilePath& filename,
                             base::MessageLoopProxy* file_message_loop_proxy)
    : path_(filename),
      file_message_loop_proxy_(file_message_loop_proxy),
      prefs_(new DictionaryValue()),
      read_only_(false),
      writer_(filename, file_message_loop_proxy),
      error_delegate_(NULL),
      initialized_(false) {
}

JsonPrefStore::~JsonPrefStore() {
  CommitPendingWrite();
}

PrefStore::ReadResult JsonPrefStore::GetValue(const std::string& key,
                                              const Value** result) const {
  Value* tmp = NULL;
  if (prefs_->Get(key, &tmp)) {
    *result = tmp;
    return READ_OK;
  }
  return READ_NO_VALUE;
}

void JsonPrefStore::AddObserver(PrefStore::Observer* observer) {
  observers_.AddObserver(observer);
}

void JsonPrefStore::RemoveObserver(PrefStore::Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool JsonPrefStore::IsInitializationComplete() const {
  return initialized_;
}

PrefStore::ReadResult JsonPrefStore::GetMutableValue(const std::string& key,
                                                     Value** result) {
  return prefs_->Get(key, result) ? READ_OK : READ_NO_VALUE;
}

void JsonPrefStore::SetValue(const std::string& key, Value* value) {
  DCHECK(value);
  scoped_ptr<Value> new_value(value);
  Value* old_value = NULL;
  prefs_->Get(key, &old_value);
  if (!old_value || !value->Equals(old_value)) {
    prefs_->Set(key, new_value.release());
    FOR_EACH_OBSERVER(PrefStore::Observer, observers_, OnPrefValueChanged(key));
  }
}

void JsonPrefStore::SetValueSilently(const std::string& key, Value* value) {
  DCHECK(value);
  scoped_ptr<Value> new_value(value);
  Value* old_value = NULL;
  prefs_->Get(key, &old_value);
  if (!old_value || !value->Equals(old_value))
    prefs_->Set(key, new_value.release());
}

void JsonPrefStore::RemoveValue(const std::string& key) {
  if (prefs_->Remove(key, NULL)) {
    FOR_EACH_OBSERVER(PrefStore::Observer, observers_, OnPrefValueChanged(key));
  }
}

bool JsonPrefStore::ReadOnly() const {
  return read_only_;
}

void JsonPrefStore::OnFileRead(Value* value_owned,
                               PersistentPrefStore::PrefReadError error,
                               bool no_dir) {
  scoped_ptr<Value> value(value_owned);
  initialized_ = true;

  if (no_dir) {
    FOR_EACH_OBSERVER(PrefStore::Observer,
                      observers_,
                      OnInitializationCompleted(false));
    return;
  }

  switch (error) {
    case PREF_READ_ERROR_ACCESS_DENIED:
    case PREF_READ_ERROR_FILE_OTHER:
    case PREF_READ_ERROR_FILE_LOCKED:
    case PREF_READ_ERROR_JSON_TYPE:
    case PREF_READ_ERROR_FILE_NOT_SPECIFIED:
      read_only_ = true;
      break;
    case PREF_READ_ERROR_NONE:
      DCHECK(value.get());
      prefs_.reset(static_cast<DictionaryValue*>(value.release()));
      break;
    case PREF_READ_ERROR_NO_FILE:
      // If the file just doesn't exist, maybe this is first run.  In any case
      // there's no harm in writing out default prefs in this case.
      break;
    case PREF_READ_ERROR_JSON_PARSE:
    case PREF_READ_ERROR_JSON_REPEAT:
      break;
    default:
      NOTREACHED() << "Unknown error: " << error;
  }

  if (error_delegate_.get() && error != PREF_READ_ERROR_NONE)
    error_delegate_->OnError(error);

  FOR_EACH_OBSERVER(PrefStore::Observer,
                    observers_,
                    OnInitializationCompleted(true));
}

void JsonPrefStore::ReadPrefsAsync(ReadErrorDelegate *error_delegate) {
  initialized_ = false;
  error_delegate_.reset(error_delegate);
  if (path_.empty()) {
    OnFileRead(NULL, PREF_READ_ERROR_FILE_NOT_SPECIFIED, false);
    return;
  }

  // Start async reading of the preferences file. It will delete itself
  // in the end.
  scoped_refptr<FileThreadDeserializer> deserializer(
      new FileThreadDeserializer(this, file_message_loop_proxy_.get()));
  deserializer->Start(path_);
}

PersistentPrefStore::PrefReadError JsonPrefStore::ReadPrefs() {
  if (path_.empty()) {
    OnFileRead(NULL, PREF_READ_ERROR_FILE_NOT_SPECIFIED, false);
    return PREF_READ_ERROR_FILE_NOT_SPECIFIED;
  }

  PrefReadError error;
  bool no_dir;
  Value* value = FileThreadDeserializer::DoReading(path_, &error, &no_dir);
  OnFileRead(value, error, no_dir);
  return error;
}

bool JsonPrefStore::WritePrefs() {
  std::string data;
  if (!SerializeData(&data))
    return false;

  // Lie about our ability to save.
  if (read_only_)
    return true;

  writer_.WriteNow(data);
  return true;
}

void JsonPrefStore::ScheduleWritePrefs() {
  if (read_only_)
    return;

  writer_.ScheduleWrite(this);
}

void JsonPrefStore::CommitPendingWrite() {
  if (writer_.HasPendingWrite() && !read_only_)
    writer_.DoScheduledWrite();
}

void JsonPrefStore::ReportValueChanged(const std::string& key) {
  FOR_EACH_OBSERVER(PrefStore::Observer, observers_, OnPrefValueChanged(key));
}

bool JsonPrefStore::SerializeData(std::string* output) {
  // TODO(tc): Do we want to prune webkit preferences that match the default
  // value?
  JSONStringValueSerializer serializer(output);
  serializer.set_pretty_print(true);
  scoped_ptr<DictionaryValue> copy(prefs_->DeepCopyWithoutEmptyChildren());
  return serializer.Serialize(*(copy.get()));
}
