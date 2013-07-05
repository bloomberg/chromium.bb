// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_LOG_UPLOADER_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_LOG_UPLOADER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/platform_file.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace base {
class SharedMemory;
}

namespace net {
class URLFetcher;
class URLRequestContextGetter;
}

typedef struct z_stream_s z_stream;

class WebRtcLogURLRequestContextGetter;

// WebRtcLogUploader uploads WebRTC logs, keeps count of how many logs have
// been started and denies further logs if a limit is reached. It also adds
// the timestamp and report ID of the uploded log to a text file. There must
// only be one object of this type.
class WebRtcLogUploader : public net::URLFetcherDelegate {
 public:
  WebRtcLogUploader();
  virtual ~WebRtcLogUploader();

  // net::URLFetcherDelegate implementation.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;
  virtual void OnURLFetchUploadProgress(const net::URLFetcher* source,
                                        int64 current, int64 total) OVERRIDE;

  // Returns true is number of logs limit is not reached yet. Increases log
  // count if true is returned. Must be called before UploadLog().
  bool ApplyForStartLogging();

  // Uploads log and decreases log count. May only be called if permission to
  // to log has been granted by calling ApplyForStartLogging() and getting true
  // in return. After UploadLog has been called, a new permission must be
  // granted.
  void UploadLog(net::URLRequestContextGetter* request_context,
                 scoped_ptr<base::SharedMemory> shared_memory,
                 uint32 length,
                 const std::string& app_session_id,
                 const std::string& app_url);

  // For testing purposes. If called, the multipart will not be uploaded, but
  // written to |post_data_| instead.
  void OverrideUploadWithBufferForTesting(std::string* post_data) {
    post_data_ = post_data;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(WebRtcLogUploaderTest,
                           AddUploadedLogInfoToUploadListFile);

  // Sets up a multipart body to be uploaded. The body is produced according
  // to RFC 2046.
  void SetupMultipart(std::string* post_data, uint8* log_buffer,
                      uint32 log_buffer_length,
                      const std::string& app_session_id,
                      const std::string& app_url);

  void AddLogData(std::string* post_data, uint8* log_buffer,
                  uint32 log_buffer_length);
  void CompressLog(std::string* post_data, uint8* input, uint32 input_size);
  void ResizeForNextOutput(std::string* post_data, z_stream* stream);
  void DecreaseLogCount();

  // Append information (time and report ID) about this uploaded log to a log
  // list file, limited to |kLogListLimitLines| entries. This list is used for
  // viewing the uploaded logs under chrome://webrtc-logs, see
  // WebRtcLogUploadList. The list has the format
  // time,id
  // time,id
  // etc.
  // where each line represents an uploaded log and "time" is Unix time.
  void AddUploadedLogInfoToUploadListFile(const std::string& report_id);

  void SetUploadPathForTesting(const base::FilePath& path) {
    upload_list_path_ = path;
  }

  int log_count_;
  base::FilePath upload_list_path_;

  // For testing purposes, see OverrideUploadWithBufferForTesting. Only accessed
  // on the FILE thread.
  std::string* post_data_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcLogUploader);
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_LOG_UPLOADER_H_
