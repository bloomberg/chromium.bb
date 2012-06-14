// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_NET_LOG_PARAMETERS_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_NET_LOG_PARAMETERS_H_
#pragma once

#include <string>

#include "content/public/browser/download_item.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"

class FilePath;
class GURL;

namespace download_net_logs {

enum DownloadType {
  SRC_NEW_DOWNLOAD,
  SRC_HISTORY_IMPORT,
  SRC_SAVE_PAGE_AS
};

// Returns NetLog parameters when a DownloadItem is activated.
base::Value* ItemActivatedCallback(
    const content::DownloadItem* download_item,
    DownloadType download_type,
    const std::string* file_name,
    net::NetLog::LogLevel log_level);

// Returns NetLog parameters when a DownloadItem is checked for danger.
base::Value* ItemCheckedCallback(
    content::DownloadDangerType danger_type,
    content::DownloadItem::SafetyState safety_state,
    net::NetLog::LogLevel log_level);

// Returns NetLog parameters when a DownloadItem is renamed.
base::Value* ItemRenamedCallback(const FilePath* old_filename,
                                 const FilePath* new_filename,
                                 net::NetLog::LogLevel log_level);

// Returns NetLog parameters when a DownloadItem is interrupted.
base::Value* ItemInterruptedCallback(content::DownloadInterruptReason reason,
                                     int64 bytes_so_far,
                                     const std::string* hash_state,
                                     net::NetLog::LogLevel log_level);

// Returns NetLog parameters when a DownloadItem is finished.
base::Value* ItemFinishedCallback(int64 bytes_so_far,
                                  const std::string* final_hash,
                                  net::NetLog::LogLevel log_level);

// Returns NetLog parameters when a DownloadItem is canceled.
base::Value* ItemCanceledCallback(int64 bytes_so_far,
                                  const std::string* hash_state,
                                  net::NetLog::LogLevel log_level);

// Returns NetLog parameters when a DownloadFile is opened.
base::Value* FileOpenedCallback(const FilePath* file_name,
                                int64 start_offset,
                                net::NetLog::LogLevel log_level);

// Returns NetLog parameters when a DownloadFile is opened.
base::Value* FileStreamDrainedCallback(size_t stream_size,
                                       size_t num_buffers,
                                       net::NetLog::LogLevel log_level);

// Returns NetLog parameters when a DownloadFile is renamed.
base::Value* FileRenamedCallback(const FilePath* old_filename,
                                 const FilePath* new_filename,
                                 net::NetLog::LogLevel log_level);

// Returns NetLog parameters when a File has an error.
base::Value* FileErrorCallback(const char* operation,
                               net::Error net_error,
                               net::NetLog::LogLevel log_level);

}  // namespace download_net_logs

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_NET_LOG_PARAMETERS_H_
