// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_FRAME_METADATA_H_
#define MEDIA_BASE_VIDEO_FRAME_METADATA_H_

#include "base/compiler_specific.h"
#include "base/time/time.h"
#include "base/values.h"
#include "media/base/media_export.h"

namespace media {

class MEDIA_EXPORT VideoFrameMetadata {
 public:
  enum Key {
    // Video capture begin/end timestamps.  Consumers can use these values for
    // dynamic optimizations, logging stats, etc.  Use Get/SetTimeTicks() for
    // these keys.
    CAPTURE_BEGIN_TIME,
    CAPTURE_END_TIME,

    // Represents either the fixed frame rate, or the maximum frame rate to
    // expect from a variable-rate source.  Use Get/SetDouble() for this key.
    FRAME_RATE,

    NUM_KEYS
  };

  VideoFrameMetadata();
  ~VideoFrameMetadata();

  bool HasKey(Key key) const;

  void Clear() { dictionary_.Clear(); }

  // Setters.  Overwrites existing value, if present.
  void SetBoolean(Key key, bool value);
  void SetInteger(Key key, int value);
  void SetDouble(Key key, double value);
  void SetString(Key key, const std::string& value);
  void SetTimeTicks(Key key, const base::TimeTicks& value);
  void SetValue(Key key, scoped_ptr<base::Value> value);

  // Getters.  Returns true if |key| was present and has the value has been set.
  bool GetBoolean(Key key, bool* value) const WARN_UNUSED_RESULT;
  bool GetInteger(Key key, int* value) const WARN_UNUSED_RESULT;
  bool GetDouble(Key key, double* value) const WARN_UNUSED_RESULT;
  bool GetString(Key key, std::string* value) const WARN_UNUSED_RESULT;
  bool GetTimeTicks(Key key, base::TimeTicks* value) const WARN_UNUSED_RESULT;

  // Returns null if |key| was not present.
  const base::Value* GetValue(Key key) const WARN_UNUSED_RESULT;

  // For serialization.
  void MergeInternalValuesInto(base::DictionaryValue* out) const;
  void MergeInternalValuesFrom(const base::DictionaryValue& in);

 private:
  const base::BinaryValue* GetBinaryValue(Key key) const;

  base::DictionaryValue dictionary_;

  DISALLOW_COPY_AND_ASSIGN(VideoFrameMetadata);
};

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_FRAME_METADATA_H_
