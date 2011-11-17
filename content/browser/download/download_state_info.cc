// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_state_info.h"

#include "content/browser/download/download_item.h"

DownloadStateInfo::DownloadStateInfo()
    : path_uniquifier(0),
      has_user_gesture(false),
      transition_type(content::PAGE_TRANSITION_LINK),
      prompt_user_for_save_location(false),
      danger(NOT_DANGEROUS) {
}

DownloadStateInfo::DownloadStateInfo(
    bool has_user_gesture,
    bool prompt_user_for_save_location)
    : path_uniquifier(0),
      has_user_gesture(has_user_gesture),
      transition_type(content::PAGE_TRANSITION_LINK),
      prompt_user_for_save_location(prompt_user_for_save_location),
      danger(NOT_DANGEROUS) {
}

DownloadStateInfo::DownloadStateInfo(
    const FilePath& target,
    const FilePath& forced_name,
    bool has_user_gesture,
    content::PageTransition transition_type,
    bool prompt_user_for_save_location,
    int uniquifier,
    DangerType danger_type)
    : target_name(target),
      path_uniquifier(uniquifier),
      has_user_gesture(has_user_gesture),
      transition_type(transition_type),
      prompt_user_for_save_location(prompt_user_for_save_location),
      danger(danger_type),
      force_file_name(forced_name) {
}

bool DownloadStateInfo::IsDangerous() const {
  // TODO(noelutz): At this point we can't mark the download as dangerous
  // if it's content is dangerous because the UI doesn't yet support it.
  // Once the UI has been changed we should return true when the danger
  // type is set to DANGEROUS_CONTENT.
  // |dangerous_content| is true.
  return (danger == DANGEROUS_FILE ||
          danger == DANGEROUS_URL);
}
