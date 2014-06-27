// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/trace_uploader.h"

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/memory/shared_memory.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/mime_util.h"
#include "net/base/network_delegate.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_config_service.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_context_getter.h"
#include "third_party/zlib/zlib.h"
#include "url/gurl.h"

namespace content {
namespace {
const char kUploadContentType[] = "multipart/form-data";
const char kMultipartBoundary[] =
    "----**--yradnuoBgoLtrapitluMklaTelgooG--**----";

const int kHttpResponseOk = 200;

}  // namespace

TraceUploader::TraceUploader(const std::string& product,
                             const std::string& version,
                             const std::string& upload_url,
                             net::URLRequestContextGetter* request_context)
    : product_(product),
      version_(version),
      upload_url_(upload_url),
      request_context_(request_context) {
  DCHECK(!product_.empty());
  DCHECK(!version_.empty());
  DCHECK(!upload_url_.empty());
}

TraceUploader::~TraceUploader() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void TraceUploader::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(source, url_fetcher_.get());
  int response_code = source->GetResponseCode();
  string report_id;
  string error_message;
  bool success = (response_code == kHttpResponseOk);
  if (success) {
    source->GetResponseAsString(&report_id);
  } else {
    error_message = "Uploading failed, response code: " +
                    base::IntToString(response_code);
  }

  BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(done_callback_, success, report_id, error_message));
  url_fetcher_.reset();
}

void TraceUploader::OnURLFetchUploadProgress(
    const net::URLFetcher* source, int64 current, int64 total) {
  DCHECK(url_fetcher_.get());

  LOG(WARNING) << "Upload progress: " << current << " of " << total;
  BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(progress_callback_, current, total));
}

void TraceUploader::DoUpload(
    const std::string& file_contents,
    UploadProgressCallback progress_callback,
    UploadDoneCallback done_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  DCHECK(!url_fetcher_.get());

  progress_callback_ = progress_callback;
  done_callback_ = done_callback;

  if (url_fetcher_.get()) {
    OnUploadError("Already uploading.");
  }

  scoped_ptr<char[]> compressed_contents(new char[kMaxUploadBytes]);
  int compressed_bytes;
  if (!Compress(file_contents, kMaxUploadBytes, compressed_contents.get(),
                &compressed_bytes)) {
    OnUploadError("Compressing file failed.");
    return;
  }

  std::string post_data;
  SetupMultipart("trace.json.gz",
                 std::string(compressed_contents.get(), compressed_bytes),
                 &post_data);

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&TraceUploader::CreateAndStartURLFetcher,
                 base::Unretained(this),
                 post_data));
}

void TraceUploader::OnUploadError(std::string error_message) {
  LOG(ERROR) << error_message;
  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(done_callback_, false, "", error_message));
}

void TraceUploader::SetupMultipart(const std::string& trace_filename,
                                   const std::string& trace_contents,
                                   std::string* post_data) {
  net::AddMultipartValueForUpload("prod", product_, kMultipartBoundary, "",
                                  post_data);
  net::AddMultipartValueForUpload("ver", version_ + "-trace",
                                  kMultipartBoundary, "", post_data);
  net::AddMultipartValueForUpload("guid", "0", kMultipartBoundary,
                                  "", post_data);
  net::AddMultipartValueForUpload("type", "trace", kMultipartBoundary,
                                  "", post_data);
  // No minidump means no need for crash to process the report.
  net::AddMultipartValueForUpload("should_process", "false", kMultipartBoundary,
                                  "", post_data);

  AddTraceFile(trace_filename, trace_contents, post_data);

  net::AddMultipartFinalDelimiterForUpload(kMultipartBoundary, post_data);
}

void TraceUploader::AddTraceFile(const std::string& trace_filename,
                                 const std::string& trace_contents,
                                 std::string* post_data) {
  post_data->append("--");
  post_data->append(kMultipartBoundary);
  post_data->append("\r\n");
  post_data->append("Content-Disposition: form-data; name=\"trace\"");
  post_data->append("; filename=\"");
  post_data->append(trace_filename);
  post_data->append("\"\r\n");
  post_data->append("Content-Type: application/gzip\r\n\r\n");
  post_data->append(trace_contents);
  post_data->append("\r\n");
}

bool TraceUploader::Compress(std::string input,
                             int max_compressed_bytes,
                             char* compressed,
                             int* compressed_bytes) {
  DCHECK(compressed);
  DCHECK(compressed_bytes);
  z_stream stream = {0};
  int result = deflateInit2(&stream,
                            Z_DEFAULT_COMPRESSION,
                            Z_DEFLATED,
                            // 16 is added to produce a gzip header + trailer.
                            MAX_WBITS + 16,
                            8,  // memLevel = 8 is default.
                            Z_DEFAULT_STRATEGY);
  DCHECK_EQ(Z_OK, result);
  stream.next_in = reinterpret_cast<uint8*>(&input[0]);
  stream.avail_in = input.size();
  stream.next_out = reinterpret_cast<uint8*>(compressed);
  stream.avail_out = max_compressed_bytes;
  // Do a one-shot compression. This will return Z_STREAM_END only if |output|
  // is large enough to hold all compressed data.
  result = deflate(&stream, Z_FINISH);
  bool success = (result == Z_STREAM_END);
  result = deflateEnd(&stream);
  DCHECK(result == Z_OK || result == Z_DATA_ERROR);

  if (success)
    *compressed_bytes = max_compressed_bytes - stream.avail_out;

  LOG(WARNING) << "input size: " << input.size()
               << ", output size: " << *compressed_bytes;
  return success;
}

void TraceUploader::CreateAndStartURLFetcher(const std::string& post_data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!url_fetcher_.get());

  std::string content_type = kUploadContentType;
  content_type.append("; boundary=");
  content_type.append(kMultipartBoundary);

  url_fetcher_.reset(
      net::URLFetcher::Create(GURL(upload_url_), net::URLFetcher::POST, this));
  url_fetcher_->SetRequestContext(request_context_);
  url_fetcher_->SetUploadData(content_type, post_data);
  url_fetcher_->Start();
}

}  // namespace content
