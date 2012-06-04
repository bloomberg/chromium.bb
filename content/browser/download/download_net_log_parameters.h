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

namespace download_net_logs {

enum DownloadType {
  SRC_NEW_DOWNLOAD,
  SRC_HISTORY_IMPORT,
  SRC_SAVE_PAGE_AS
};

// NetLog parameters when a DownloadItem is activated.
class ItemActivatedParameters : public net::NetLog::EventParameters {
 public:
  ItemActivatedParameters(DownloadType download_type,
                          int64 id,
                          const std::string& original_url,
                          const std::string& final_url,
                          const std::string& file_name,
                          content::DownloadDangerType danger_type,
                          content::DownloadItem::SafetyState safety_state,
                          int64 start_offset);
  virtual base::Value* ToValue() const OVERRIDE;

 protected:
  virtual ~ItemActivatedParameters();

 private:
  const DownloadType type_;
  const int64 id_;
  const std::string original_url_;
  const std::string final_url_;
  const std::string file_name_;
  const content::DownloadDangerType danger_type_;
  const content::DownloadItem::SafetyState safety_state_;
  const int64 start_offset_;

  DISALLOW_COPY_AND_ASSIGN(ItemActivatedParameters);
};

// NetLog parameters when a DownloadItem is checked for danger.
class ItemCheckedParameters : public net::NetLog::EventParameters {
 public:
  ItemCheckedParameters(content::DownloadDangerType danger_type,
                        content::DownloadItem::SafetyState safety_state);
  virtual base::Value* ToValue() const OVERRIDE;

 protected:
  virtual ~ItemCheckedParameters();

 private:
  const content::DownloadDangerType danger_type_;
  const content::DownloadItem::SafetyState safety_state_;

  DISALLOW_COPY_AND_ASSIGN(ItemCheckedParameters);
};

// NetLog parameters when a DownloadItem is added to the history database.
class ItemInHistoryParameters : public net::NetLog::EventParameters {
 public:
  ItemInHistoryParameters(int64 handle);
  virtual base::Value* ToValue() const OVERRIDE;

 protected:
  virtual ~ItemInHistoryParameters();

 private:
  const int64 db_handle_;

  DISALLOW_COPY_AND_ASSIGN(ItemInHistoryParameters);
};

// NetLog parameters when a DownloadItem is updated.
class ItemUpdatedParameters : public net::NetLog::EventParameters {
 public:
  ItemUpdatedParameters(int64 bytes_so_far);
  virtual base::Value* ToValue() const OVERRIDE;

 protected:
  virtual ~ItemUpdatedParameters();

 private:
  const int64 bytes_so_far_;

  DISALLOW_COPY_AND_ASSIGN(ItemUpdatedParameters);
};

// NetLog parameters when a DownloadItem is renamed.
class ItemRenamedParameters : public net::NetLog::EventParameters {
 public:
  ItemRenamedParameters(
      const std::string& old_filename, const std::string& new_filename);
  virtual base::Value* ToValue() const OVERRIDE;

 protected:
  virtual ~ItemRenamedParameters();

 private:
  const std::string old_filename_;
  const std::string new_filename_;

  DISALLOW_COPY_AND_ASSIGN(ItemRenamedParameters);
};

// NetLog parameters when a DownloadItem is interrupted.
class ItemInterruptedParameters : public net::NetLog::EventParameters {
 public:
  ItemInterruptedParameters(content::DownloadInterruptReason reason,
                            int64 bytes_so_far,
                            const std::string& hash_state);
  virtual base::Value* ToValue() const OVERRIDE;

 protected:
  virtual ~ItemInterruptedParameters();

 private:
  const content::DownloadInterruptReason reason_;
  const int64 bytes_so_far_;
  const std::string hash_state_;

  DISALLOW_COPY_AND_ASSIGN(ItemInterruptedParameters);
};

// NetLog parameters when a DownloadItem is finished.
class ItemFinishedParameters : public net::NetLog::EventParameters {
 public:
  ItemFinishedParameters(int64 bytes_so_far, const std::string& final_hash);
  virtual base::Value* ToValue() const OVERRIDE;

 protected:
  virtual ~ItemFinishedParameters();

 private:
  const int64 bytes_so_far_;
  const std::string final_hash_;

  DISALLOW_COPY_AND_ASSIGN(ItemFinishedParameters);
};

// NetLog parameters when a DownloadItem is canceled.
class ItemCanceledParameters : public net::NetLog::EventParameters {
 public:
  ItemCanceledParameters(int64 bytes_so_far, const std::string& hash_state);
  virtual base::Value* ToValue() const OVERRIDE;

 protected:
  virtual ~ItemCanceledParameters();

 private:
  const int64 bytes_so_far_;
  const std::string hash_state_;

  DISALLOW_COPY_AND_ASSIGN(ItemCanceledParameters);
};

// NetLog parameters when a DownloadFile is opened.
class FileOpenedParameters : public net::NetLog::EventParameters {
 public:
  FileOpenedParameters(const std::string& file_name,
                       int64 start_offset);
  virtual base::Value* ToValue() const OVERRIDE;

 protected:
  virtual ~FileOpenedParameters();

 private:
  const std::string file_name_;
  const int64 start_offset_;

  DISALLOW_COPY_AND_ASSIGN(FileOpenedParameters);
};

// NetLog parameters when a DownloadFile is opened.
class FileStreamDrainedParameters : public net::NetLog::EventParameters {
 public:
  FileStreamDrainedParameters(size_t stream_size,
                            size_t num_buffers);
  virtual base::Value* ToValue() const OVERRIDE;

 protected:
  virtual ~FileStreamDrainedParameters();

 private:
  const size_t stream_size_;
  const size_t num_buffers_;

  DISALLOW_COPY_AND_ASSIGN(FileStreamDrainedParameters);
};

// NetLog parameters when a DownloadFile is renamed.
class FileRenamedParameters : public net::NetLog::EventParameters {
 public:
  FileRenamedParameters(
      const std::string& old_filename, const std::string& new_filename);
  virtual base::Value* ToValue() const OVERRIDE;

 protected:
  virtual ~FileRenamedParameters();

 private:
  const std::string old_filename_;
  const std::string new_filename_;

  DISALLOW_COPY_AND_ASSIGN(FileRenamedParameters);
};

// NetLog parameters when a File has an error.
class FileErrorParameters : public net::NetLog::EventParameters {
 public:
  FileErrorParameters(const std::string& operation, net::Error net_error);
  virtual base::Value* ToValue() const OVERRIDE;

 protected:
  virtual ~FileErrorParameters();

 private:
  const std::string operation_;
  const net::Error net_error_;

  DISALLOW_COPY_AND_ASSIGN(FileErrorParameters);
};

}  // namespace download_net_logs

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_NET_LOG_PARAMETERS_H_
