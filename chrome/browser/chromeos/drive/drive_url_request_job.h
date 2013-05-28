// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_URL_REQUEST_JOB_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_URL_REQUEST_JOB_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "net/http/http_byte_range.h"
#include "net/url_request/url_request_job.h"

namespace base {
class SequencedTaskRunner;
}  // namespace

namespace net {
class IOBuffer;
class NetworkDelegate;
class URLRequest;
}  // namespace net

namespace drive {

class DriveFileStreamReader;
class FileSystemInterface;
class ResourceEntry;

// DriveURLRequestJob is the gateway between network-level drive:...
// requests for drive resources and FileSystem.  It exposes content URLs
// formatted as drive:<drive-file-path>.
// The methods should be run on IO thread, and the operations to communicate
// with a locally cached file will run on |file_task_runner|.
class DriveURLRequestJob : public net::URLRequestJob {
 public:

  // Callback to return the FileSystemInterface instance. This is an
  // injecting point for testing.
  // Note that the callback will be copied between threads (IO and UI), and
  // will be called on UI thread.
  typedef base::Callback<FileSystemInterface*()> FileSystemGetter;

  DriveURLRequestJob(const FileSystemGetter& file_system_getter,
                     base::SequencedTaskRunner* file_task_runner,
                     net::URLRequest* request,
                     net::NetworkDelegate* network_delegate);

  // net::URLRequestJob overrides:
  virtual void SetExtraRequestHeaders(const net::HttpRequestHeaders& headers)
      OVERRIDE;
  virtual void Start() OVERRIDE;
  virtual void Kill() OVERRIDE;
  virtual bool GetMimeType(std::string* mime_type) const OVERRIDE;
  virtual bool IsRedirectResponse(
      GURL* location, int* http_status_code) OVERRIDE;
  virtual bool ReadRawData(
      net::IOBuffer* buf, int buf_size, int* bytes_read) OVERRIDE;

 protected:
  virtual ~DriveURLRequestJob();

 private:
  // Called when the initialization of DriveFileStreamReader is completed.
  void OnDriveFileStreamReaderInitialized(
      int error, scoped_ptr<ResourceEntry> entry);

  // Called when DriveFileStreamReader::Read is completed.
  void OnReadCompleted(int read_result);

  const FileSystemGetter file_system_getter_;
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  // The range of the file to be returned.
  net::HttpByteRange byte_range_;

  scoped_ptr<DriveFileStreamReader> stream_reader_;
  scoped_ptr<ResourceEntry> entry_;

  // This should remain the last member so it'll be destroyed first and
  // invalidate its weak pointers before other members are destroyed.
  base::WeakPtrFactory<DriveURLRequestJob> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(DriveURLRequestJob);
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_URL_REQUEST_JOB_H_
