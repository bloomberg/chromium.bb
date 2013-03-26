// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_URL_REQUEST_JOB_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_URL_REQUEST_JOB_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/string_piece.h"
#include "chrome/browser/chromeos/drive/drive_file_error.h"
#include "chrome/browser/chromeos/drive/drive_resource_metadata.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"
#include "net/url_request/url_request_job.h"

namespace net {
class FileStream;
class HttpResponseInfo;
class IOBuffer;
class NetworkDelegate;
class URLRequest;
}  // namespace net

namespace drive {

class DriveEntryProto;
class DriveFileSystemInterface;

// DriveURLRequesetJob is the gateway between network-level drive://...
// requests for drive resources and DriveFileSytem.  It exposes content URLs
// formatted as drive://<resource-id>.
// The methods should be run on IO thread.
class DriveURLRequestJob : public net::URLRequestJob {
 public:

  // Callback to return the DriveFileSystemInterface instance. This is an
  // injecting point for testing.
  // Note that the callback will be copied between threads (IO and UI), and
  // will be called on UI thread.
  typedef base::Callback<DriveFileSystemInterface*()> DriveFileSystemGetter;

  DriveURLRequestJob(
      const DriveFileSystemGetter& file_system_getter,
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate);

  // net::URLRequestJob overrides:
  virtual void Start() OVERRIDE;
  virtual void Kill() OVERRIDE;
  virtual bool GetMimeType(std::string* mime_type) const OVERRIDE;
  virtual void GetResponseInfo(net::HttpResponseInfo* info) OVERRIDE;
  virtual int GetResponseCode() const OVERRIDE;
  virtual bool ReadRawData(net::IOBuffer* buf,
                           int buf_size,
                           int* bytes_read) OVERRIDE;

 protected:
  virtual ~DriveURLRequestJob();

 private:
  // Helper methods for Delegate::OnUrlFetchDownloadData and ReadRawData to
  // receive download data and copy to response buffer.
  // For detailed description of logic, refer to comments in definitions of
  // Start() and ReadRawData().

  void OnUrlFetchDownloadData(google_apis::GDataErrorCode error,
                              scoped_ptr<std::string> download_data);
  // Called from ReadRawData, returns true if data is ready, false otherwise.
  bool ContinueReadFromDownloadData(int* bytes_read);
  // Copies from download buffer into response buffer.
  bool ReadFromDownloadData();

  // Helper callback for handling async responses from
  // DriveFileSystem::GetFileByResourceId().
  void OnGetFileByResourceId(DriveFileError error,
                             const base::FilePath& local_file_path,
                             const std::string& mime_type,
                             DriveFileType file_type);

  // Helper callback for handling result of file_util::GetFileSize().
  void OnGetFileSize(int64 *file_size, bool success);

  // Helper callback for GetEntryInfoByResourceId invoked by Start().
  void OnGetEntryInfoByResourceId(const std::string& resource_id,
                                  DriveFileError error,
                                  const base::FilePath& drive_file_path,
                                  scoped_ptr<DriveEntryProto> entry_proto);

  // Helper methods for ReadRawData to open file and read from its corresponding
  // stream in a streaming fashion.
  bool ContinueReadFromFile(int* bytes_read);
  void ReadFromFile();
  void ReadFileStream();

  // Helper callback for handling async responses from FileStream::Open().
  void OnFileOpen(int open_result);

    // Helper callback for handling async responses from FileStream::Read().
  void OnReadFileStream(int bytes_read);

  // Helper methods to handle |reamining_bytes_| and |read_buf_|.
  int BytesReadCompleted();
  void RecordBytesRead(int bytes_read);

  // Helper methods to formulate and notify about response status, info and
  // headers.
  void NotifySuccess();
  void NotifyFailure(int);
  void HeadersCompleted(int status_code, const std::string& status_txt);

  // Helper method to close |stream_|.
  void CloseFileStream();

  DriveFileSystemGetter file_system_getter_;

  bool error_;  // True if we've encountered an error.
  bool headers_set_;  // True if headers have been set.

  base::FilePath local_file_path_;
  base::FilePath drive_file_path_;
  std::string mime_type_;
  int64 initial_file_size_;
  int64 remaining_bytes_;
  scoped_ptr<net::FileStream> stream_;
  scoped_ptr<net::HttpResponseInfo> response_info_;
  bool streaming_download_;
  scoped_refptr<net::IOBuffer> read_buf_;
  base::StringPiece read_buf_remaining_;
  std::string download_buf_;
  base::StringPiece download_buf_remaining_;

  // This should remain the last member so it'll be destroyed first and
  // invalidate its weak pointers before other members are destroyed.
  base::WeakPtrFactory<DriveURLRequestJob> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(DriveURLRequestJob);
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_URL_REQUEST_JOB_H_
