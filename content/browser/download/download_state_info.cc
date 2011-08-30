// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_state_info.h"

#include "content/browser/download/download_item.h"

DownloadStateInfo::DownloadStateInfo()
    : path_uniquifier(0),
      has_user_gesture(false),
      transition_type(PageTransition::LINK),
      prompt_user_for_save_location(false),
      is_dangerous_file(false),
      is_dangerous_url(false) {
}

DownloadStateInfo::DownloadStateInfo(
    bool has_user_gesture,
    bool prompt_user_for_save_location)
    : path_uniquifier(0),
      has_user_gesture(has_user_gesture),
      transition_type(PageTransition::LINK),
      prompt_user_for_save_location(prompt_user_for_save_location),
      is_dangerous_file(false),
      is_dangerous_url(false) {
}

DownloadStateInfo::DownloadStateInfo(
    const FilePath& target,
    const FilePath& forced_name,
    bool has_user_gesture,
    PageTransition::Type transition_type,
    bool prompt_user_for_save_location,
    int uniquifier,
    bool dangerous_file,
    bool dangerous_url)
    : target_name(target),
      path_uniquifier(uniquifier),
      has_user_gesture(has_user_gesture),
      transition_type(transition_type),
      prompt_user_for_save_location(prompt_user_for_save_location),
      is_dangerous_file(dangerous_file),
      is_dangerous_url(dangerous_url),
      force_file_name(forced_name) {
}

bool DownloadStateInfo::IsDangerous() const {
  return is_dangerous_url || is_dangerous_file;
}
