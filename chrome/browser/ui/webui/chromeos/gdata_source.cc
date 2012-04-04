// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/gdata_source.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/gdata/gdata_file_system.h"
#include "chrome/browser/chromeos/gdata/gdata_system_service.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/escape.h"
#include "net/base/net_errors.h"

using content::BrowserThread;

namespace {

const int kBufferSize = 8*1024;
const char kMimeTypeOctetStream[] = "application/octet-stream";
const net::UnescapeRule::Type kUrlPathUnescapeMask =
    net::UnescapeRule::SPACES |
    net::UnescapeRule::URL_SPECIAL_CHARS |
    net::UnescapeRule::CONTROL_CHARS;

// Helper function that reads file size.
void GetFileSizeOnIOThreadPool(const FilePath& file_path,
                               int64* file_size) {
  if (!file_util::GetFileSize(file_path, file_size))
    *file_size = 0;
}

bool ParseGDataUrlPath(const std::string& path,
                       std::string* resource_id,
                       std::string* file_name) {
  std::vector<std::string> components;
  FilePath url_path= FilePath::FromUTF8Unsafe(path);
  url_path.GetComponents(&components);
  if (components.size() != 2) {
    LOG(WARNING) << "Invalid path: " << url_path.value();
    return false;
  }

  *resource_id = net::UnescapeURLComponent(components[0], kUrlPathUnescapeMask);
  *file_name = net::UnescapeURLComponent(components[1], kUrlPathUnescapeMask);
  return true;
}

}  // namespace

namespace gdata {

struct FileReadContext : public base::RefCountedThreadSafe<FileReadContext> {
 public:
  FileReadContext(int request_id, const FilePath& file_path, int64 file_size)
      : request_id(request_id),
        file_path(file_path),
        file_size(file_size),
        total_read(0),
        stream(new net::FileStream(NULL)),
        buffer(new net::IOBufferWithSize(kBufferSize)),
        raw_data(new std::vector<unsigned char>()) {
    raw_data->resize(static_cast<size_t>(file_size));
  }
  ~FileReadContext() {}

  int request_id;
  FilePath file_path;
  int64 file_size;
  int total_read;
  scoped_ptr<net::FileStream> stream;
  scoped_refptr<net::IOBufferWithSize> buffer;
  scoped_ptr< std::vector<unsigned char> > raw_data;
};

class GetFileMimeTypeDelegate : public gdata::FindFileDelegate {
 public:
  GetFileMimeTypeDelegate() {}
  virtual ~GetFileMimeTypeDelegate() {}

  const std::string& mime_type() const { return mime_type_; }
  const std::string& file_name() const { return file_name_; }
 private:
  // GDataFileSystem::FindFileDelegate overrides.
  virtual void OnDone(base::PlatformFileError error,
                      const FilePath& directory_path,
                      gdata::GDataFileBase* file) OVERRIDE {
    if (error == base::PLATFORM_FILE_OK && file && file->AsGDataFile()) {
      mime_type_ = file->AsGDataFile()->content_mime_type();
      file_name_ = file->AsGDataFile()->file_name();
    }
  }

  std::string mime_type_;
  std::string file_name_;
};

GDataSource::GDataSource(Profile* profile)
    : DataSource(chrome::kChromeUIGDataHost, MessageLoop::current()),
      profile_(profile),
      system_service_(GDataSystemServiceFactory::GetForProfile(profile)) {
}

GDataSource::~GDataSource() {
}

void GDataSource::StartDataRequest(const std::string& path,
                                   bool is_incognito,
                                   int request_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (is_incognito || !system_service_) {
    SendResponse(request_id, reinterpret_cast<RefCountedMemory*>(NULL));
    return;
  }

  std::string resource_id;
  std::string file_name;
  if (!ParseGDataUrlPath(path, &resource_id, &file_name)) {
    SendResponse(request_id, reinterpret_cast<RefCountedMemory*>(NULL));
    return;
  }

  // Check if file metadata is matching our expectations first.
  GetFileMimeTypeDelegate delegate;
  system_service_->file_system()->FindFileByResourceIdSync(resource_id,
                                                           &delegate);
  if (delegate.file_name() != file_name) {
    SendResponse(request_id, reinterpret_cast<RefCountedMemory*>(NULL));
    return;
  }

  system_service_->file_system()->GetFileForResourceId(
      resource_id,
      base::Bind(&GDataSource::OnGetFileForResourceId,
                 this,
                 request_id));
}

std::string GDataSource::GetMimeType(const std::string& path) const {
  if (!system_service_)
    return kMimeTypeOctetStream;

  std::string resource_id;
  std::string unused_file_name;
  if (!ParseGDataUrlPath(path, &resource_id, &unused_file_name))
    return kMimeTypeOctetStream;

  GetFileMimeTypeDelegate delegate;
  system_service_->file_system()->FindFileByResourceIdSync(resource_id,
                                                          &delegate);
  if (delegate.mime_type().empty())
    return kMimeTypeOctetStream;

  return delegate.mime_type();
}


void GDataSource::OnGetFileForResourceId(
    int request_id,
    base::PlatformFileError error,
    const FilePath& file_path,
    const std::string& mime_type,
    GDataFileType file_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error != base::PLATFORM_FILE_OK || file_type != REGULAR_FILE) {
    SendResponse(request_id, reinterpret_cast<RefCountedMemory*>(NULL));
    return;
  }

  int64* file_size = new int64(0);
  BrowserThread::GetBlockingPool()->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&GetFileSizeOnIOThreadPool,
                 file_path,
                 base::Unretained(file_size)),
      base::Bind(&GDataSource::StartFileRead,
                 this,
                 request_id,
                 file_path,
                 base::Owned(file_size)));
}

void GDataSource::StartFileRead(int request_id,
                                const FilePath& file_path,
                                int64* file_size) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  scoped_refptr<FileReadContext> context(
      new FileReadContext(request_id, file_path, *file_size));
  int rv = context->stream->Open(
      file_path,
      base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ |
      base::PLATFORM_FILE_ASYNC,
      base::Bind(&GDataSource::OnFileOpen,
                 this,
                 context));

  if (rv != net::ERR_IO_PENDING) {
    LOG(WARNING) << "Failed to open " << file_path.value();
    SendResponse(request_id,
                 reinterpret_cast<RefCountedMemory*>(NULL));
    return;
  }
}


void GDataSource::OnFileOpen(scoped_refptr<FileReadContext> context,
                             int open_result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (open_result != net::OK) {
    LOG(WARNING) << "Failed to open " << context->file_path.value();
    SendResponse(context->request_id,
                 reinterpret_cast<RefCountedMemory*>(NULL));
    return;
  }

  int rv = context->stream->Read(context->buffer.get(), context->buffer->size(),
      base::Bind(&GDataSource::OnFileRead,
                 this,
                 context));

  if (rv != net::ERR_IO_PENDING) {
    LOG(WARNING) << "Failed reading " << context->file_path.value();
    SendResponse(context->request_id,
                 reinterpret_cast<RefCountedMemory*>(NULL));
    return;
  }
}

void GDataSource::OnFileRead(scoped_refptr<FileReadContext> context,
                             int bytes_read) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (bytes_read < 0) {
    LOG(WARNING) << "Failed reading " << context->file_path.value();
    SendResponse(context->request_id,
                 reinterpret_cast<RefCountedMemory*>(NULL));
    return;
  } else if (bytes_read == 0) {
    scoped_refptr<RefCountedBytes> response_bytes =
        RefCountedBytes::TakeVector(context->raw_data.release());
    SendResponse(context->request_id, response_bytes.get());
    return;
  }

  // Copy read chunk in the buffer.
  if (context->raw_data->size() <
      static_cast<size_t>(bytes_read + context->total_read)) {
    LOG(WARNING) << "Reading more data than what we allocated for file "
                 << context->file_path.value();
    SendResponse(context->request_id,
                 reinterpret_cast<RefCountedMemory*>(NULL));
    return;
  }

  char* data = reinterpret_cast<char*>(&(context->raw_data->front()));
  memcpy(data + context->total_read,
         context->buffer->data(),
         bytes_read);
  context->total_read += bytes_read;

  int read_value = context->stream->Read(context->buffer.get(),
                                         context->buffer->size(),
                                         base::Bind(&GDataSource::OnFileRead,
                                                    this,
                                                    context));

  if (read_value != net::ERR_IO_PENDING) {
    LOG(WARNING) << "Failed to read file for request "
                 << context->file_path.value();
    SendResponse(context->request_id,
                 reinterpret_cast<RefCountedMemory*>(NULL));
    return;
  }
}

}  // namespace gdata
