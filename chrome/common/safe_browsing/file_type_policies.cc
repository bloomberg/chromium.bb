// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/string_util.h"
#include "chrome/common/safe_browsing/download_protection_util.h"
#include "chrome/common/safe_browsing/file_type_policies.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace safe_browsing {

FileTypePolicies::FileTypePolicies() {
  // Setup a file-type policy to use if the ResourceBundle is unreadable.
  // This should normally never be used.
  last_resort_default_.set_uma_value(99999);  // TODO: Add this to xml.
  auto settings = last_resort_default_.add_platform_settings();
  settings->set_danger_level(DownloadFileType::ALLOW_ON_USER_GESTURE);
  settings->set_auto_open_hint(DownloadFileType::DISALLOW_AUTO_OPEN);
  settings->set_ping_setting(DownloadFileType::NO_PING);
}

FileTypePolicies::~FileTypePolicies() {}

void FileTypePolicies::ReadResourceBundle(std::string* binary_pb) {
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  bundle.GetRawDataResource(IDR_DOWNLOAD_FILE_TYPES_PB).CopyToString(binary_pb);
}

void FileTypePolicies::RecordUpdateMetrics(UpdateResult result,
                                           const std::string& src_name) {
  // src_name should be "ResourceBundle" or "DynamicUpdate".
  UMA_HISTOGRAM_SPARSE_SLOWLY(
      "SafeBrowsing.FileTypeUpdate." + src_name + "Result",
      static_cast<unsigned int>(result));

  if (result == UpdateResult::SUCCESS) {
    UMA_HISTOGRAM_SPARSE_SLOWLY(
        "SafeBrowsing.FileTypeUpdate." + src_name + "Version",
        config_->version_id());
    UMA_HISTOGRAM_SPARSE_SLOWLY(
        "SafeBrowsing.FileTypeUpdate." + src_name + "TypeCount",
        config_->file_types().size());
  }
}

void FileTypePolicies::PopulateFromResourceBundle() {
  std::string binary_pb;
  ReadResourceBundle(&binary_pb);
  UpdateResult result = PopulateFromBinaryPb(binary_pb);
  RecordUpdateMetrics(result, "ResourceBundle");
}

void FileTypePolicies::PopulateFromDynamicUpdate(const std::string& binary_pb) {
  UpdateResult result = PopulateFromBinaryPb(binary_pb);
  RecordUpdateMetrics(result, "DynamicUpdate");
}

FileTypePolicies::UpdateResult FileTypePolicies::PopulateFromBinaryPb(
    const std::string& binary_pb) {
  // Parse the proto and do some validation on it.
  if (binary_pb.empty())
    return UpdateResult::FAILED_EMPTY;

  std::unique_ptr<DownloadFileTypeConfig> new_config(
      new DownloadFileTypeConfig);
  if (!new_config->ParseFromString(binary_pb))
    return UpdateResult::FAILED_PROTO_PARSE;

  // Need at least a default setting.
  if (new_config->default_file_type().platform_settings().size() == 0)
    return UpdateResult::FAILED_DEFAULT_SETTING_SET;

  // Every file type should have exactly one setting, pre-filtered for this
  // platform.
  for (const auto& file_type : new_config->file_types()) {
    if (file_type.platform_settings().size() != 1)
      return UpdateResult::FAILED_WRONG_SETTINGS_COUNT;
  }

  // Compare against existing config, if we have one.
  if (config_) {
    // Check that version number increases
    if (new_config->version_id() <= config_->version_id())
      return UpdateResult::FAILED_VERSION_CHECK;

    // Check that we haven't dropped more than 1/2 the list.
    if (new_config->file_types().size() * 2 < config_->file_types().size())
      return UpdateResult::FAILED_DELTA_CHECK;
  }

  // Looks good. Update our internal list.
  config_.reset(new_config.release());

  // Build an index for faster lookup.
  file_type_by_ext_.clear();
  for (const DownloadFileType& file_type : config_->file_types()) {
    // If there are dups, first one wins.
    file_type_by_ext_.insert(std::make_pair(file_type.extension(), &file_type));
  }

  return UpdateResult::SUCCESS;
}

float FileTypePolicies::SampledPingProbability() const {
  return config_ ? config_->sampled_ping_probability() : 0.0;
}

// static
std::string FileTypePolicies::CanonicalizedExtension(
    const base::FilePath& file) {
  // The policy list is all ASCII, so a non-ASCII extension won't be in it.
  const base::FilePath::StringType ext =
      download_protection_util::GetFileExtension(file);
  std::string ascii_ext =
      base::ToLowerASCII(base::FilePath(ext).MaybeAsASCII());
  if (ascii_ext[0] == '.')
    ascii_ext.erase(0, 1);
  return ascii_ext;
}

const DownloadFileType& FileTypePolicies::PolicyForFile(
    const base::FilePath& file) {
  // This could happen if the ResourceBundle is corrupted.
  if (!config_) {
    DCHECK(false);
    return last_resort_default_;
  }

  std::string ascii_ext = CanonicalizedExtension(file);
  auto itr = file_type_by_ext_.find(ascii_ext);
  if (itr != file_type_by_ext_.end())
    return *itr->second;
  else
    return config_->default_file_type();
}

const DownloadFileType::PlatformSettings& FileTypePolicies::SettingsForFile(
    const base::FilePath& file) {
  DCHECK_EQ(1, PolicyForFile(file).platform_settings().size());
  return PolicyForFile(file).platform_settings(0);
}

int64_t FileTypePolicies::UmaValueForFile(const base::FilePath& file) {
  return PolicyForFile(file).uma_value();
}

bool FileTypePolicies::IsFileAnArchive(const base::FilePath& file) {
  return PolicyForFile(file).is_archive();
}

}  // namespace safe_browsing
