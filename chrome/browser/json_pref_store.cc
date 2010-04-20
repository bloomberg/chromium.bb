// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/json_pref_store.h"

#include <algorithm>

#include "base/values.h"
#include "chrome/common/json_value_serializer.h"

JsonPrefStore::JsonPrefStore(const FilePath& filename)
    : path_(filename),
      prefs_(new DictionaryValue()),
      read_only_(false),
      writer_(filename) { }

JsonPrefStore::~JsonPrefStore() {
  if (writer_.HasPendingWrite() && !read_only_)
    writer_.DoScheduledWrite();
}

PrefStore::PrefReadError JsonPrefStore::ReadPrefs() {
  JSONFileValueSerializer serializer(path_);

  int error_code = 0;
  std::string error_msg;
  scoped_ptr<Value> value(serializer.Deserialize(&error_code, &error_msg));
  if (!value.get()) {
#if defined(GOOGLE_CHROME_BUILD)
    // This log could be used for more detailed client-side error diagnosis,
    // but since this triggers often with unit tests, we need to disable it
    // in non-official builds.
    PLOG(ERROR) << "Error reading Preferences: " << error_msg << " " <<
        path_.value();
#endif
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
        // It's possible the user hand-edited the file, so don't clobber it yet.
        // Give them a chance to recover the file.
        // TODO(erikkay) maybe we should just move it aside and continue.
        read_only_ = true;
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

