// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/json_pref_store.h"

#include <algorithm>

#include "base/file_util.h"
#include "base/values.h"
#include "chrome/common/json_value_serializer.h"

namespace {

// Some extensions we'll tack on to copies of the Preferences files.
const FilePath::CharType* kBadExtension = FILE_PATH_LITERAL("bad");

}  // namespace

JsonPrefStore::JsonPrefStore(const FilePath& filename,
                             base::MessageLoopProxy* file_message_loop_proxy)
    : path_(filename),
      prefs_(new DictionaryValue()),
      read_only_(false),
      writer_(filename, file_message_loop_proxy) {
}

JsonPrefStore::~JsonPrefStore() {
  if (writer_.HasPendingWrite() && !read_only_)
    writer_.DoScheduledWrite();
}

PrefStore::ReadResult JsonPrefStore::GetValue(const std::string& key,
                                              Value** result) const {
  return prefs_->Get(key, result) ? READ_OK : READ_NO_VALUE;
}

void JsonPrefStore::AddObserver(PrefStore::Observer* observer) {
  observers_.AddObserver(observer);
}

void JsonPrefStore::RemoveObserver(PrefStore::Observer* observer) {
  observers_.RemoveObserver(observer);
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

PersistentPrefStore::PrefReadError JsonPrefStore::ReadPrefs() {
  if (path_.empty()) {
    read_only_ = true;
    return PREF_READ_ERROR_FILE_NOT_SPECIFIED;
  }
  JSONFileValueSerializer serializer(path_);

  int error_code = 0;
  std::string error_msg;
  scoped_ptr<Value> value(serializer.Deserialize(&error_code, &error_msg));
  if (!value.get()) {
    PrefReadError error;
    switch (error_code) {
      case JSONFileValueSerializer::JSON_ACCESS_DENIED:
        // If the file exists but is simply unreadable, put the file into a
        // state where we don't try to save changes.  Otherwise, we could
        // clobber the existing prefs.
        error = PREF_READ_ERROR_ACCESS_DENIED;
        read_only_ = true;
        break;
      case JSONFileValueSerializer::JSON_CANNOT_READ_FILE:
        error = PREF_READ_ERROR_FILE_OTHER;
        read_only_ = true;
        break;
      case JSONFileValueSerializer::JSON_FILE_LOCKED:
        error = PREF_READ_ERROR_FILE_LOCKED;
        read_only_ = true;
        break;
      case JSONFileValueSerializer::JSON_NO_SUCH_FILE:
        // If the file just doesn't exist, maybe this is first run.  In any case
        // there's no harm in writing out default prefs in this case.
        error = PREF_READ_ERROR_NO_FILE;
        break;
      default:
        error = PREF_READ_ERROR_JSON_PARSE;
        // JSON errors indicate file corruption of some sort.
        // Since the file is corrupt, move it to the side and continue with
        // empty preferences.  This will result in them losing their settings.
        // We keep the old file for possible support and debugging assistance
        // as well as to detect if they're seeing these errors repeatedly.
        // TODO(erikkay) Instead, use the last known good file.
        FilePath bad = path_.ReplaceExtension(kBadExtension);

        // If they've ever had a parse error before, put them in another bucket.
        // TODO(erikkay) if we keep this error checking for very long, we may
        // want to differentiate between recent and long ago errors.
        if (file_util::PathExists(bad))
          error = PREF_READ_ERROR_JSON_REPEAT;
        file_util::Move(path_, bad);
        break;
    }
    return error;
  }

  // Preferences should always have a dictionary root.
  if (!value->IsType(Value::TYPE_DICTIONARY)) {
    // See comment for the default case above.
    read_only_ = true;
    return PREF_READ_ERROR_JSON_TYPE;
  }

  prefs_.reset(static_cast<DictionaryValue*>(value.release()));

  return PREF_READ_ERROR_NONE;
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

bool JsonPrefStore::SerializeData(std::string* output) {
  // TODO(tc): Do we want to prune webkit preferences that match the default
  // value?
  JSONStringValueSerializer serializer(output);
  serializer.set_pretty_print(true);
  scoped_ptr<DictionaryValue> copy(prefs_->DeepCopyWithoutEmptyChildren());
  return serializer.Serialize(*(copy.get()));
}
