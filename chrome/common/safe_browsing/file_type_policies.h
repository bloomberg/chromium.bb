// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SAFE_BROWSING_FILE_TYPE_POLICIES_H_
#define CHROME_COMMON_SAFE_BROWSING_FILE_TYPE_POLICIES_H_

#include <map>
#include <memory>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "chrome/common/safe_browsing/download_file_types.pb.h"

namespace safe_browsing {

// This holds a list of file types (aka file extensions) that we know about,
// with policies related to how Safe Browsing and the download UI should treat
// them.
//
// The data to populate it is read from a ResourceBundle and then also
// fetched periodically from Google to get the most up-to-date policies.
//
// It should be setup and accessed on IO thread.

// TODO(nparker): Replace the following methods' contents with calls to
//   g_browser_process->safe_browsing_service()->file_type_policies()->***.
//
//   bool IsSupportedBinaryFile(const base::FilePath& file);
//   bool IsArchiveFile(const base::FilePath& file);
//   ClientDownloadRequest::DownloadType GetDownloadType(
//       const base::FilePath& file);
//   int GetSBClientDownloadTypeValueForUMA(const base::FilePath& file);
//   bool IsAllowedToOpenAutomatically(const base::FilePath& path);
//   DownloadDangerLevel GetFileDangerLevel(const base::FilePath& path);

class FileTypePolicies {
 public:
  // Creator must call one of Populate* before calling other methods.
  FileTypePolicies();
  virtual ~FileTypePolicies();

  // Read data from the main ResourceBundle. This updates the internal list
  // only if the data passes integrity checks. This is normally called once
  // after construction.
  void PopulateFromResourceBundle();

  // Update the internal list from a binary proto fetched from the network.
  // Same integrity checks apply. This can be called multiple times with new
  // protos.
  void PopulateFromDynamicUpdate(const std::string& binary_pb);

  // Accessors
  const DownloadFileType& PolicyForFile(const base::FilePath& file);
  const DownloadFileType::PlatformSettings& SettingsForFile(
      const base::FilePath& file);
  int64_t UmaValueForFile(const base::FilePath& file);
  bool IsFileAnArchive(const base::FilePath& file);
  float SampledPingProbability() const;

 protected:
  // Used in metrics, do not reorder.
  enum class UpdateResult {
    SUCCESS = 1,
    FAILED_EMPTY = 2,
    FAILED_PROTO_PARSE = 3,
    FAILED_DELTA_CHECK = 4,
    FAILED_VERSION_CHECK = 5,
    FAILED_DEFAULT_SETTING_SET = 6,
    FAILED_WRONG_SETTINGS_COUNT = 7,
  };

  // Read data from an serialized protobuf and update the internal list
  // only if it passes integrity checks.
  virtual UpdateResult PopulateFromBinaryPb(const std::string& binary_pb);

  // Fetch the blob from the main resource bundle.
  virtual void ReadResourceBundle(std::string* binary_pb);

  // Record the result of an update attempt.
  virtual void RecordUpdateMetrics(UpdateResult result,
                                   const std::string& src_name);

  // Return the ASCII lowercase extension w/o leading dot, or empty.
  static std::string CanonicalizedExtension(const base::FilePath& file);

 private:
  // The latest config we've committed. Starts out null.
  std::unique_ptr<DownloadFileTypeConfig> config_;

  // This references entries in config_.
  std::map<std::string, const DownloadFileType*> file_type_by_ext_;

  // Type used if we can't load from disk.
  DownloadFileType last_resort_default_;

  FRIEND_TEST_ALL_PREFIXES(FileTypePoliciesTest, UnpackResourceBundle);
  FRIEND_TEST_ALL_PREFIXES(FileTypePoliciesTest, BadProto);
  FRIEND_TEST_ALL_PREFIXES(FileTypePoliciesTest, BadUpdateFromExisting);
};

}  // namespace safe_browsing

#endif  // CHROME_COMMON_SAFE_BROWSING_FILE_TYPE_POLICIES_H_
