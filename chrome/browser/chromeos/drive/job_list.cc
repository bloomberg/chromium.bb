// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/job_list.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"

namespace drive {

std::string JobTypeToString(JobType type) {
  switch (type){
    case TYPE_GET_ABOUT_RESOURCE:
      return "TYPE_GET_ABOUT_RESOURCE";
    case TYPE_GET_APP_LIST:
      return "TYPE_GET_APP_LIST";
    case TYPE_GET_ALL_RESOURCE_LIST:
      return "TYPE_GET_ALL_RESOURCE_LIST";
    case TYPE_GET_RESOURCE_LIST_IN_DIRECTORY:
      return "TYPE_GET_RESOURCE_LIST_IN_DIRECTORY";
    case TYPE_SEARCH:
      return "TYPE_SEARCH";
    case TYPE_GET_CHANGE_LIST:
      return "TYPE_GET_CHANGE_LIST";
    case TYPE_CONTINUE_GET_RESOURCE_LIST:
      return "TYPE_CONTINUE_GET_RESOURCE_LIST";
    case TYPE_GET_RESOURCE_ENTRY:
      return "TYPE_GET_RESOURCE_ENTRY";
    case TYPE_GET_SHARE_URL:
      return "TYPE_GET_SHARE_URL";
    case TYPE_DELETE_RESOURCE:
      return "TYPE_DELETE_RESOURCE";
    case TYPE_COPY_RESOURCE:
      return "TYPE_COPY_RESOURCE";
    case TYPE_COPY_HOSTED_DOCUMENT:
      return "TYPE_COPY_HOSTED_DOCUMENT";
    case TYPE_MOVE_RESOURCE:
      return "TYPE_MOVE_RESOURCE";
    case TYPE_RENAME_RESOURCE:
      return "TYPE_RENAME_RESOURCE";
    case TYPE_TOUCH_RESOURCE:
      return "TYPE_TOUCH_RESOURCE";
    case TYPE_ADD_RESOURCE_TO_DIRECTORY:
      return "TYPE_ADD_RESOURCE_TO_DIRECTORY";
    case TYPE_REMOVE_RESOURCE_FROM_DIRECTORY:
      return "TYPE_REMOVE_RESOURCE_FROM_DIRECTORY";
    case TYPE_ADD_NEW_DIRECTORY:
      return "TYPE_ADD_NEW_DIRECTORY";
    case TYPE_DOWNLOAD_FILE:
      return "TYPE_DOWNLOAD_FILE";
    case TYPE_UPLOAD_NEW_FILE:
      return "TYPE_UPLOAD_NEW_FILE";
    case TYPE_UPLOAD_EXISTING_FILE:
      return "TYPE_UPLOAD_EXISTING_FILE";
    case TYPE_CREATE_FILE:
      return "TYPE_CREATE_FILE";
  }
  NOTREACHED();
  return "(unknown job type)";
}

std::string JobStateToString(JobState state) {
  switch (state) {
    case STATE_NONE:
      return "STATE_NONE";
    case STATE_RUNNING:
      return "STATE_RUNNING";
    case STATE_RETRY:
      return "STATE_RETRY";
  }
  NOTREACHED();
  return "(unknown job state)";
}

JobInfo::JobInfo(JobType in_job_type)
    : job_type(in_job_type),
      job_id(-1),
      state(STATE_NONE),
      num_completed_bytes(0),
      num_total_bytes(0) {
}

std::string JobInfo::ToString() const {
  std::string output = base::StringPrintf(
      "%s %s [%d]",
      JobTypeToString(job_type).c_str(),
      JobStateToString(state).c_str(),
      job_id);
  if (job_type == TYPE_DOWNLOAD_FILE ||
      job_type == TYPE_UPLOAD_NEW_FILE ||
      job_type == TYPE_UPLOAD_EXISTING_FILE) {
    base::StringAppendF(&output,
                        " bytes: %s/%s",
                        base::Int64ToString(num_completed_bytes).c_str(),
                        base::Int64ToString(num_total_bytes).c_str());
  }
  return output;
}

bool IsActiveFileTransferJobInfo(const JobInfo& job_info) {
  // Using switch statement so that compiler can warn when new states or types
  // are added.
  switch (job_info.state) {
    case STATE_NONE:
      return false;
    case STATE_RUNNING:
    case STATE_RETRY:
      break;
  }

  switch (job_info.job_type) {
    case TYPE_GET_ABOUT_RESOURCE:
    case TYPE_GET_APP_LIST:
    case TYPE_GET_ALL_RESOURCE_LIST:
    case TYPE_GET_RESOURCE_LIST_IN_DIRECTORY:
    case TYPE_SEARCH:
    case TYPE_GET_CHANGE_LIST:
    case TYPE_CONTINUE_GET_RESOURCE_LIST:
    case TYPE_GET_RESOURCE_ENTRY:
    case TYPE_GET_SHARE_URL:
    case TYPE_DELETE_RESOURCE:
    case TYPE_COPY_RESOURCE:
    case TYPE_COPY_HOSTED_DOCUMENT:
    case TYPE_MOVE_RESOURCE:
    case TYPE_RENAME_RESOURCE:
    case TYPE_TOUCH_RESOURCE:
    case TYPE_ADD_RESOURCE_TO_DIRECTORY:
    case TYPE_REMOVE_RESOURCE_FROM_DIRECTORY:
    case TYPE_ADD_NEW_DIRECTORY:
    case TYPE_CREATE_FILE:
      return false;
    case TYPE_DOWNLOAD_FILE:
    case TYPE_UPLOAD_NEW_FILE:
    case TYPE_UPLOAD_EXISTING_FILE:
      break;
  }

  return true;
}

}  // namespace drive
