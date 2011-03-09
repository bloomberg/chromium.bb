// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_FORMAT_H_
#define MEDIA_BASE_MEDIA_FORMAT_H_

#include <map>
#include <string>

#include "base/values.h"

namespace media {

// MediaFormat is used to describe the output of a Filter to determine whether
// a downstream filter can accept the output from an upstream filter.
//
// For example, an audio decoder could output key-values for the number of
// channels and the sample rate.  A downstream audio renderer would use this
// information to properly initialize the audio hardware before playback
// started.
class MediaFormat {
 public:
  // Common keys.
  static const char kURL[];
  static const char kSurfaceType[];
  static const char kSurfaceFormat[];
  static const char kSampleRate[];
  static const char kSampleBits[];
  static const char kChannels[];
  static const char kWidth[];
  static const char kHeight[];

  MediaFormat();
  ~MediaFormat();

  // Basic map operations.
  bool empty() const { return value_map_.empty(); }

  bool Contains(const std::string& key) const;

  void Clear();

  // Value accessors.
  void SetAsBoolean(const std::string& key, bool in_value);
  void SetAsInteger(const std::string& key, int in_value);
  void SetAsReal(const std::string& key, double in_value);
  void SetAsString(const std::string& key, const std::string& in_value);

  bool GetAsBoolean(const std::string& key, bool* out_value) const;
  bool GetAsInteger(const std::string& key, int* out_value) const;
  bool GetAsReal(const std::string& key, double* out_value) const;
  bool GetAsString(const std::string& key, std::string* out_value) const;

 private:
  // Helper to return a value.
  Value* GetValue(const std::string& key) const;

  // Helper to release Value of the key
  void ReleaseValue(const std::string& key);

  typedef std::map<std::string, Value*> ValueMap;
  ValueMap value_map_;

  DISALLOW_COPY_AND_ASSIGN(MediaFormat);
};

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_FORMAT_H_
