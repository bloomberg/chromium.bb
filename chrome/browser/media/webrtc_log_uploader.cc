// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc_log_uploader.h"

#include "base/logging.h"
#include "base/shared_memory.h"
#include "base/strings/stringprintf.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/partial_circular_buffer.h"
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

namespace {

const int kLogCountLimit = 5;
const uint32 kIntermediateCompressionBufferBytes = 256 * 1024;  // 256 KB

const char kUploadURL[] = "https://clients2.google.com/cr/report";
const char kUploadContentType[] = "multipart/form-data";
const char kMultipartBoundary[] =
    "----**--yradnuoBgoLtrapitluMklaTelgooG--**----";

}  // namespace

WebRtcLogUploader::WebRtcLogUploader()
    : log_count_(0) {
}

WebRtcLogUploader::~WebRtcLogUploader() {
}

void WebRtcLogUploader::OnURLFetchComplete(
    const net::URLFetcher* source) {
}

void WebRtcLogUploader::OnURLFetchUploadProgress(
    const net::URLFetcher* source, int64 current, int64 total) {
}

bool WebRtcLogUploader::ApplyForStartLogging() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (log_count_ < kLogCountLimit) {
    ++log_count_;
    return true;
  }
  return false;
}

void WebRtcLogUploader::UploadLog(net::URLRequestContextGetter* request_context,
                                  scoped_ptr<base::SharedMemory> shared_memory,
                                  uint32 length,
                                  const std::string& app_session_id,
                                  const std::string& app_url) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  DCHECK(shared_memory);
  DCHECK(shared_memory->memory());

  std::string post_data;
  SetupMultipart(&post_data, reinterpret_cast<uint8*>(shared_memory->memory()),
                 length, app_session_id, app_url);

  std::string content_type = kUploadContentType;
  content_type.append("; boundary=");
  content_type.append(kMultipartBoundary);

  net::URLFetcher* url_fetcher =
      net::URLFetcher::Create(GURL(kUploadURL), net::URLFetcher::POST, this);
  url_fetcher->SetRequestContext(request_context);
  url_fetcher->SetUploadData(content_type, post_data);
  url_fetcher->Start();

  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
      base::Bind(&WebRtcLogUploader::DecreaseLogCount, base::Unretained(this)));
}

void WebRtcLogUploader::SetupMultipart(std::string* post_data,
                                       uint8* log_buffer,
                                       uint32 log_buffer_length,
                                       const std::string& app_session_id,
                                       const std::string& app_url) {
#if defined(OS_WIN)
  const char product[] = "Chrome";
#elif defined(OS_MACOSX)
  const char product[] = "Chrome_Mac";
#elif defined(OS_LINUX)
#if !defined(ADDRESS_SANITIZER)
  const char product[] = "Chrome_Linux";
#else
  const char product[] = "Chrome_Linux_ASan";
#endif
#elif defined(OS_ANDROID)
  const char product[] = "Chrome_Android";
#elif defined(OS_CHROMEOS)
  const char product[] = "Chrome_ChromeOS";
#else
  // This file should not be compiled for other platforms.
  COMPILE_ASSERT(false);
#endif
  net::AddMultipartValueForUpload("prod", product, kMultipartBoundary,
                                  "", post_data);
  chrome::VersionInfo version_info;
  net::AddMultipartValueForUpload("ver", version_info.Version(),
                                  kMultipartBoundary, "", post_data);
  net::AddMultipartValueForUpload("guid", "0", kMultipartBoundary,
                                  "", post_data);
  net::AddMultipartValueForUpload("type", "webrtc_log", kMultipartBoundary,
                                  "", post_data);
  net::AddMultipartValueForUpload("app_session_id", app_session_id,
                                  kMultipartBoundary, "", post_data);
  net::AddMultipartValueForUpload("url", app_url, kMultipartBoundary,
                                  "", post_data);
  AddLogData(post_data, log_buffer, log_buffer_length);
  net::AddMultipartFinalDelimiterForUpload(kMultipartBoundary, post_data);
}

void WebRtcLogUploader::AddLogData(std::string* post_data,
                                   uint8* log_buffer,
                                   uint32 log_buffer_length) {
  post_data->append("--");
  post_data->append(kMultipartBoundary);
  post_data->append("\r\n");
  post_data->append("Content-Disposition: form-data; name=\"webrtc_log\"");
  post_data->append("; filename=\"webrtc_log.gz\"\r\n");
  post_data->append("Content-Type: application/gzip\r\n\r\n");

  CompressLog(post_data, log_buffer, log_buffer_length);

  post_data->append("\r\n");
}

void WebRtcLogUploader::CompressLog(std::string* post_data,
                                    uint8* input,
                                    uint32 input_size) {
  PartialCircularBuffer read_pcb(input, input_size);

  z_stream stream = {0};
  int result = deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                            // windowBits = 15 is default, 16 is added to
                            // produce a gzip header + trailer.
                            15 + 16,
                            8,  // memLevel = 8 is default.
                            Z_DEFAULT_STRATEGY);
  DCHECK_EQ(Z_OK, result);

  uint8 intermediate_buffer[kIntermediateCompressionBufferBytes] = {0};
  ResizeForNextOutput(post_data, &stream);
  uint32 read = 0;

  do {
    if (stream.avail_in == 0) {
      read = read_pcb.Read(&intermediate_buffer[0],
                           kIntermediateCompressionBufferBytes);
      stream.next_in = &intermediate_buffer[0];
      stream.avail_in = read;
      if (read != kIntermediateCompressionBufferBytes)
        break;
    }
    result = deflate(&stream, Z_SYNC_FLUSH);
    DCHECK_EQ(Z_OK, result);
    if (stream.avail_out == 0)
      ResizeForNextOutput(post_data, &stream);
  } while (true);

  // Ensure we have enough room in the output buffer. Easier to always just do a
  // resize than looping around and resize if needed.
  if (stream.avail_out < kIntermediateCompressionBufferBytes)
    ResizeForNextOutput(post_data, &stream);

  result = deflate(&stream, Z_FINISH);
  DCHECK_EQ(Z_STREAM_END, result);
  result = deflateEnd(&stream);
  DCHECK_EQ(Z_OK, result);

  post_data->resize(post_data->size() - stream.avail_out);
}

void WebRtcLogUploader::ResizeForNextOutput(std::string* post_data,
                                            z_stream* stream) {
  size_t old_size = post_data->size() - stream->avail_out;
  post_data->resize(old_size + kIntermediateCompressionBufferBytes);
  stream->next_out = reinterpret_cast<uint8*>(&(*post_data)[old_size]);
  stream->avail_out = kIntermediateCompressionBufferBytes;
}

void WebRtcLogUploader::DecreaseLogCount() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  --log_count_;
}
