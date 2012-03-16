// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_state_info.h"

#include "content/public/browser/download_item.h"

DownloadStateInfo::DownloadStateInfo()
    : path_uniquifier(0),
      has_user_gesture(false),
      transition_type(content::PAGE_TRANSITION_LINK),
      prompt_user_for_save_location(false),
      danger(content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS) {
}

DownloadStateInfo::DownloadStateInfo(const FilePath& forced_name,
                                     bool has_user_gesture,
                                     content::PageTransition transition_type,
                                     bool prompt_user_for_save_location)
    : path_uniquifier(0),
      has_user_gesture(has_user_gesture),
      transition_type(transition_type),
      prompt_user_for_save_location(prompt_user_for_save_location),
      danger(content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS),
      force_file_name(forced_name) {
}

bool DownloadStateInfo::IsDangerous() const {
  // TODO(noelutz): At this point only the windows views UI supports
  // warnings based on dangerous content.
#ifdef OS_WIN
  return (danger == content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE ||
          danger == content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL ||
          danger == content::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT ||
          danger == content::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT);
#else
  return (danger == content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE ||
          danger == content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL);
#endif
}
