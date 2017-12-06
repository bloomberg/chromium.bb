// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_MEDIA_ENGAGEMENT_PRELOADED_LIST_H_
#define CHROME_BROWSER_MEDIA_MEDIA_ENGAGEMENT_PRELOADED_LIST_H_

#include <string>
#include <vector>

#include "base/macros.h"

namespace base {
class FilePath;
}  // namespace base

namespace url {
class Origin;
}  // namespace url

class MediaEngagementPreloadedList {
 public:
  MediaEngagementPreloadedList();
  ~MediaEngagementPreloadedList();

  // Load the contents from |path|.
  bool LoadFromFile(const base::FilePath& path);

  // Checks whether |origin| has a high global engagement and is present in the
  // preloaded list.
  bool CheckOriginIsPresent(const url::Origin& origin) const;

  // Check whether we have loaded a list.
  bool loaded() const { return is_loaded_; }

  // Check whether the list we have loaded is empty.
  bool empty() const { return dafsa_.empty(); }

 protected:
  friend class MediaEngagementPreloadedListTest;

  // The names of the CheckResult and LoadResult histograms.
  static const char kHistogramCheckResultName[];
  static const char kHistogramLoadResultName[];

  // The result of the CheckStringIsPresent operation. This enum is used to
  // record a histogram and should not be renumbered.
  enum class CheckResult {
    // The check succeeded and the string was found in the data.
    kFound = 0,

    // The check succeeded but the string was not found in the data.
    kNotFound,

    // The check failed because the list is empty.
    kListEmpty,

    // The check failed because the list has not been loaded.
    kListNotLoaded,

    kCount
  };

  // The result of the LoadFromFile operation. This enum is used to record
  // a histogram and should not be renumbered.
  enum class LoadResult {
    // The list was loaded successfully.
    kLoaded = 0,

    // The list was not loaded because the file was not found.
    kFileNotFound,

    // The list was not loaded because the file could not be read.
    kFileReadFailed,

    // The list was not loaded because the proto stored in the file could not be
    // parsed.
    kParseProtoFailed,

    kCount
  };

  // Checks if |input| is present in the preloaded data.
  bool CheckStringIsPresent(const std::string& input) const;

  // Records |result| to the LoadResult histogram.
  void RecordLoadResult(LoadResult result);

  // Records |result| to the CheckResult histogram.
  void RecordCheckResult(CheckResult result) const;

 private:
  // The preloaded data in dafsa format.
  std::vector<unsigned char> dafsa_;

  // If a list has been successfully loaded.
  bool is_loaded_ = false;

  DISALLOW_COPY_AND_ASSIGN(MediaEngagementPreloadedList);
};

#endif  // CHROME_BROWSER_MEDIA_MEDIA_ENGAGEMENT_PRELOADED_LIST_H_
