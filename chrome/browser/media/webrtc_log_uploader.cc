// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc_log_uploader.h"

#include "base/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/webrtc_log_list.h"
#include "chrome/browser/media/webrtc_log_util.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/partial_circular_buffer.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/mime_util.h"
#include "net/url_request/url_fetcher.h"
#include "third_party/zlib/zlib.h"

namespace {

const int kLogCountLimit = 5;
const uint32 kIntermediateCompressionBufferBytes = 256 * 1024;  // 256 KB
const int kLogListLimitLines = 50;

const char kUploadURL[] = "https://clients2.google.com/cr/report";
const char kUploadContentType[] = "multipart/form-data";
const char kMultipartBoundary[] =
    "----**--yradnuoBgoLtrapitluMklaTelgooG--**----";

const int kHttpResponseOk = 200;

// Adds the header section for a gzip file to the multipart |post_data|.
void AddMultipartFileContentHeader(std::string* post_data,
                                   const std::string& content_name) {
  post_data->append("--");
  post_data->append(kMultipartBoundary);
  post_data->append("\r\nContent-Disposition: form-data; name=\"");
  post_data->append(content_name);
  post_data->append("\"; filename=\"");
  post_data->append(content_name + ".gz");
  post_data->append("\"\r\nContent-Type: application/gzip\r\n\r\n");
}

// Adds |compressed_log| to |post_data|.
void AddLogData(std::string* post_data,
                const std::vector<uint8>& compressed_log) {
  AddMultipartFileContentHeader(post_data, "webrtc_log");
  post_data->append(reinterpret_cast<const char*>(&compressed_log[0]),
                    compressed_log.size());
  post_data->append("\r\n");
}

// Adds the RTP dump data to |post_data|.
void AddRtpDumpData(std::string* post_data,
                    const std::string& name,
                    const std::string& dump_data) {
  AddMultipartFileContentHeader(post_data, name);
  post_data->append(dump_data.data(), dump_data.size());
  post_data->append("\r\n");
}

}  // namespace

WebRtcLogUploadDoneData::WebRtcLogUploadDoneData() {}

WebRtcLogUploadDoneData::~WebRtcLogUploadDoneData() {}

WebRtcLogUploader::WebRtcLogUploader()
    : log_count_(0),
      post_data_(NULL),
      shutting_down_(false) {
  file_thread_checker_.DetachFromThread();
}

WebRtcLogUploader::~WebRtcLogUploader() {
  DCHECK(create_thread_checker_.CalledOnValidThread());
  DCHECK(upload_done_data_.empty());
  DCHECK(shutting_down_);
}

void WebRtcLogUploader::OnURLFetchComplete(
    const net::URLFetcher* source) {
  DCHECK(create_thread_checker_.CalledOnValidThread());
  DCHECK(upload_done_data_.find(source) != upload_done_data_.end());
  DCHECK(!shutting_down_);
  int response_code = source->GetResponseCode();
  UploadDoneDataMap::iterator it = upload_done_data_.find(source);
  if (it != upload_done_data_.end()) {
    // The log path can be empty here if we failed getting it before. We still
    // upload the log if that's the case.
    std::string report_id;
    if (response_code == kHttpResponseOk &&
        source->GetResponseAsString(&report_id) &&
        !it->second.log_path.empty()) {
      // TODO(jiayl): Add the RTP dump records to chrome://webrtc-logs.
      base::FilePath log_list_path =
          WebRtcLogList::GetWebRtcLogListFileForDirectory(it->second.log_path);
      content::BrowserThread::PostTask(
          content::BrowserThread::FILE,
          FROM_HERE,
          base::Bind(&WebRtcLogUploader::AddUploadedLogInfoToUploadListFile,
                     base::Unretained(this),
                     log_list_path,
                     it->second.local_log_id,
                     report_id));
    }
    NotifyUploadDone(response_code, report_id, it->second);
    upload_done_data_.erase(it);
  }

  delete source;
}

void WebRtcLogUploader::OnURLFetchUploadProgress(
    const net::URLFetcher* source, int64 current, int64 total) {
}

bool WebRtcLogUploader::ApplyForStartLogging() {
  DCHECK(create_thread_checker_.CalledOnValidThread());
  if (log_count_ < kLogCountLimit && !shutting_down_) {
    ++log_count_;
    return true;
  }
  return false;
}

void WebRtcLogUploader::LoggingStoppedDontUpload() {
  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
      base::Bind(&WebRtcLogUploader::DecreaseLogCount, base::Unretained(this)));
}

void WebRtcLogUploader::LoggingStoppedDoUpload(
    scoped_ptr<unsigned char[]> log_buffer,
    uint32 length,
    const std::map<std::string, std::string>& meta_data,
    const WebRtcLogUploadDoneData& upload_done_data) {
  DCHECK(file_thread_checker_.CalledOnValidThread());
  DCHECK(log_buffer.get());
  DCHECK(!upload_done_data.log_path.empty());

  std::vector<uint8> compressed_log;
  CompressLog(
      &compressed_log, reinterpret_cast<uint8*>(&log_buffer[0]), length);

  std::string local_log_id;

  if (base::PathExists(upload_done_data.log_path)) {
    WebRtcLogUtil::DeleteOldWebRtcLogFiles(upload_done_data.log_path);

    local_log_id = base::DoubleToString(base::Time::Now().ToDoubleT());
    base::FilePath log_file_path =
        upload_done_data.log_path.AppendASCII(local_log_id)
            .AddExtension(FILE_PATH_LITERAL(".gz"));
    WriteCompressedLogToFile(compressed_log, log_file_path);

    base::FilePath log_list_path =
        WebRtcLogList::GetWebRtcLogListFileForDirectory(
            upload_done_data.log_path);
    AddLocallyStoredLogInfoToUploadListFile(log_list_path, local_log_id);
  }

  WebRtcLogUploadDoneData upload_done_data_with_log_id = upload_done_data;
  upload_done_data_with_log_id.local_log_id = local_log_id;

  scoped_ptr<std::string> post_data(new std::string());
  SetupMultipart(post_data.get(),
                 compressed_log,
                 upload_done_data.incoming_rtp_dump,
                 upload_done_data.outgoing_rtp_dump,
                 meta_data);

  // If a test has set the test string pointer, write to it and skip uploading.
  // Still fire the upload callback so that we can run an extension API test
  // using the test framework for that without hanging.
  // TODO(grunell): Remove this when the api test for this feature is fully
  // implemented according to the test plan. http://crbug.com/257329.
  if (post_data_) {
    *post_data_ = *post_data;
    NotifyUploadDone(kHttpResponseOk, "", upload_done_data_with_log_id);
    return;
  }

  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&WebRtcLogUploader::CreateAndStartURLFetcher,
                 base::Unretained(this),
                 upload_done_data_with_log_id,
                 Passed(&post_data)));

  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
      base::Bind(&WebRtcLogUploader::DecreaseLogCount, base::Unretained(this)));
}

void WebRtcLogUploader::StartShutdown() {
  DCHECK(create_thread_checker_.CalledOnValidThread());
  DCHECK(!shutting_down_);

  // Delete all URLFetchers first and clear the upload done map.
  for (UploadDoneDataMap::iterator it = upload_done_data_.begin();
       it != upload_done_data_.end();
       ++it) {
    delete it->first;
  }
  upload_done_data_.clear();
  shutting_down_ = true;
}

void WebRtcLogUploader::SetupMultipart(
    std::string* post_data,
    const std::vector<uint8>& compressed_log,
    const base::FilePath& incoming_rtp_dump,
    const base::FilePath& outgoing_rtp_dump,
    const std::map<std::string, std::string>& meta_data) {
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
#error Platform not supported.
#endif
  net::AddMultipartValueForUpload("prod", product, kMultipartBoundary,
                                  "", post_data);
  chrome::VersionInfo version_info;
  net::AddMultipartValueForUpload("ver", version_info.Version() + "-webrtc",
                                  kMultipartBoundary, "", post_data);
  net::AddMultipartValueForUpload("guid", "0", kMultipartBoundary,
                                  "", post_data);
  net::AddMultipartValueForUpload("type", "webrtc_log", kMultipartBoundary,
                                  "", post_data);

  // Add custom meta data.
  std::map<std::string, std::string>::const_iterator it = meta_data.begin();
  for (; it != meta_data.end(); ++it) {
    net::AddMultipartValueForUpload(it->first, it->second, kMultipartBoundary,
                                    "", post_data);
  }

  AddLogData(post_data, compressed_log);

  // Add the rtp dumps if they exist.
  base::FilePath rtp_dumps[2] = {incoming_rtp_dump, outgoing_rtp_dump};
  static const char* kRtpDumpNames[2] = {"rtpdump_recv", "rtpdump_send"};

  for (size_t i = 0; i < 2; ++i) {
    if (!rtp_dumps[i].empty() && base::PathExists(rtp_dumps[i])) {
      std::string dump_data;
      if (base::ReadFileToString(rtp_dumps[i], &dump_data))
        AddRtpDumpData(post_data, kRtpDumpNames[i], dump_data);
    }
  }

  net::AddMultipartFinalDelimiterForUpload(kMultipartBoundary, post_data);
}

void WebRtcLogUploader::CompressLog(std::vector<uint8>* compressed_log,
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
  ResizeForNextOutput(compressed_log, &stream);
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
      ResizeForNextOutput(compressed_log, &stream);
  } while (true);

  // Ensure we have enough room in the output buffer. Easier to always just do a
  // resize than looping around and resize if needed.
  if (stream.avail_out < kIntermediateCompressionBufferBytes)
    ResizeForNextOutput(compressed_log, &stream);

  result = deflate(&stream, Z_FINISH);
  DCHECK_EQ(Z_STREAM_END, result);
  result = deflateEnd(&stream);
  DCHECK_EQ(Z_OK, result);

  compressed_log->resize(compressed_log->size() - stream.avail_out);
}

void WebRtcLogUploader::ResizeForNextOutput(std::vector<uint8>* compressed_log,
                                            z_stream* stream) {
  size_t old_size = compressed_log->size() - stream->avail_out;
  compressed_log->resize(old_size + kIntermediateCompressionBufferBytes);
  stream->next_out = &(*compressed_log)[old_size];
  stream->avail_out = kIntermediateCompressionBufferBytes;
}

void WebRtcLogUploader::CreateAndStartURLFetcher(
    const WebRtcLogUploadDoneData& upload_done_data,
    scoped_ptr<std::string> post_data) {
  DCHECK(create_thread_checker_.CalledOnValidThread());

  if (shutting_down_)
    return;

  std::string content_type = kUploadContentType;
  content_type.append("; boundary=");
  content_type.append(kMultipartBoundary);

  net::URLFetcher* url_fetcher =
      net::URLFetcher::Create(GURL(kUploadURL), net::URLFetcher::POST, this);
  url_fetcher->SetRequestContext(g_browser_process->system_request_context());
  url_fetcher->SetUploadData(content_type, *post_data);
  url_fetcher->Start();
  upload_done_data_[url_fetcher] = upload_done_data;
}

void WebRtcLogUploader::DecreaseLogCount() {
  DCHECK(create_thread_checker_.CalledOnValidThread());
  --log_count_;
}

void WebRtcLogUploader::WriteCompressedLogToFile(
    const std::vector<uint8>& compressed_log,
    const base::FilePath& log_file_path) {
  DCHECK(file_thread_checker_.CalledOnValidThread());
  DCHECK(!compressed_log.empty());
  base::WriteFile(log_file_path,
                  reinterpret_cast<const char*>(&compressed_log[0]),
                  compressed_log.size());
}

void WebRtcLogUploader::AddLocallyStoredLogInfoToUploadListFile(
    const base::FilePath& upload_list_path,
    const std::string& local_log_id) {
  DCHECK(file_thread_checker_.CalledOnValidThread());
  DCHECK(!upload_list_path.empty());
  DCHECK(!local_log_id.empty());

  std::string contents;

  if (base::PathExists(upload_list_path)) {
    if (!base::ReadFileToString(upload_list_path, &contents)) {
      DPLOG(WARNING) << "Could not read WebRTC log list file.";
      return;
    }

    // Limit the number of log entries to |kLogListLimitLines| - 1, to make room
    // for the new entry. Each line including the last ends with a '\n', so hit
    // n will be before line n-1 (from the back).
    int lf_count = 0;
    int i = contents.size() - 1;
    for (; i >= 0 && lf_count < kLogListLimitLines; --i) {
      if (contents[i] == '\n')
        ++lf_count;
    }
    if (lf_count >= kLogListLimitLines) {
      // + 1 to compensate for the for loop decrease before the conditional
      // check and + 1 to get the length.
      contents.erase(0, i + 2);
    }
  }

  // Write the log ID to the log list file. Leave the upload time and report ID
  // empty.
  contents += ",," + local_log_id + '\n';

  int written =
      base::WriteFile(upload_list_path, &contents[0], contents.size());
  if (written != static_cast<int>(contents.size())) {
    DPLOG(WARNING) << "Could not write all data to WebRTC log list file: "
                   << written;
  }
}

void WebRtcLogUploader::AddUploadedLogInfoToUploadListFile(
    const base::FilePath& upload_list_path,
    const std::string& local_log_id,
    const std::string& report_id) {
  DCHECK(file_thread_checker_.CalledOnValidThread());
  DCHECK(!upload_list_path.empty());
  DCHECK(!local_log_id.empty());
  DCHECK(!report_id.empty());

  std::string contents;

  if (base::PathExists(upload_list_path)) {
    if (!base::ReadFileToString(upload_list_path, &contents)) {
      DPLOG(WARNING) << "Could not read WebRTC log list file.";
      return;
    }
  }

  // Write the Unix time and report ID to the log list file. We should be able
  // to find the local log ID, in that case insert the data into the existing
  // line. Otherwise add it in the end.
  base::Time time_now = base::Time::Now();
  std::string time_now_str = base::DoubleToString(time_now.ToDoubleT());
  size_t pos = contents.find(",," + local_log_id);
  if (pos != std::string::npos) {
    contents.insert(pos, time_now_str);
    contents.insert(pos + time_now_str.length() + 1, report_id);
  } else {
    contents += time_now_str + "," + report_id + ",\n";
  }

  int written =
      base::WriteFile(upload_list_path, &contents[0], contents.size());
  if (written != static_cast<int>(contents.size())) {
    DPLOG(WARNING) << "Could not write all data to WebRTC log list file: "
                   << written;
  }
}

void WebRtcLogUploader::NotifyUploadDone(
    int response_code,
    const std::string& report_id,
    const WebRtcLogUploadDoneData& upload_done_data) {
  content::BrowserThread::PostTask(content::BrowserThread::IO, FROM_HERE,
      base::Bind(&WebRtcLoggingHandlerHost::UploadLogDone,
                 upload_done_data.host));
  if (!upload_done_data.callback.is_null()) {
    bool success = response_code == kHttpResponseOk;
    std::string error_message;
    if (!success) {
      error_message = "Uploading failed, response code: " +
                      base::IntToString(response_code);
    }
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(upload_done_data.callback, success, report_id,
                   error_message));
  }
}
