// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_engagement_preloaded_list.h"

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "chrome/browser/media/media_engagement_preload.pb.h"
#include "net/base/lookup_string_in_fixed_set.h"
#include "url/origin.h"

MediaEngagementPreloadedList::MediaEngagementPreloadedList() = default;

MediaEngagementPreloadedList::~MediaEngagementPreloadedList() = default;

bool MediaEngagementPreloadedList::LoadFromFile(const base::FilePath& path) {
  // Check the file exists.
  if (!base::PathExists(path))
    return false;

  // Read the file to a string.
  std::string file_data;
  if (!base::ReadFileToString(path, &file_data))
    return false;

  // Load the preloaded list into a proto message.
  chrome_browser_media::PreloadedData message;
  if (!message.ParseFromString(file_data))
    return false;

  // Copy data from the protobuf message.
  dafsa_ = std::vector<unsigned char>(
      message.dafsa().c_str(),
      message.dafsa().c_str() + message.dafsa().length());

  is_loaded_ = true;
  return true;
}

bool MediaEngagementPreloadedList::CheckOriginIsPresent(
    const url::Origin& origin) const {
  return CheckStringIsPresent(origin.Serialize());
}

bool MediaEngagementPreloadedList::CheckStringIsPresent(
    const std::string& input) const {
  return net::LookupStringInFixedSet(dafsa_.data(), dafsa_.size(),
                                     input.c_str(),
                                     input.size()) == net::kDafsaFound;
}
