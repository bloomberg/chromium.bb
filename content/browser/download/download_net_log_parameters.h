// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_NET_LOG_PARAMETERS_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_NET_LOG_PARAMETERS_H_
#pragma once

#include <string>

#include "net/base/net_errors.h"
#include "net/base/net_log.h"

namespace download_net_logs {

// NetLog parameters when a DownloadFile is opened.
class FileOpenedParameters : public net::NetLog::EventParameters {
 public:
  FileOpenedParameters(const std::string& file_name,
                       int64 start_offset);
  virtual base::Value* ToValue() const OVERRIDE;

 private:
  const std::string file_name_;
  const int64 start_offset_;

  DISALLOW_COPY_AND_ASSIGN(FileOpenedParameters);
};

// NetLog parameters when a DownloadFile is renamed.
class FileRenamedParameters : public net::NetLog::EventParameters {
 public:
  FileRenamedParameters(
      const std::string& old_filename, const std::string& new_filename);
  virtual base::Value* ToValue() const OVERRIDE;

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

 private:
  const std::string operation_;
  const net::Error net_error_;

  DISALLOW_COPY_AND_ASSIGN(FileErrorParameters);
};

}  // namespace download_net_logs

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_NET_LOG_PARAMETERS_H_
