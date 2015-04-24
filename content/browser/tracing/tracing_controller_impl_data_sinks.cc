// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/browser/tracing/tracing_controller_impl.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/zlib/zlib.h"

namespace content {

namespace {

class FileTraceDataSink : public TracingController::TraceDataSink {
 public:
  explicit FileTraceDataSink(const base::FilePath& trace_file_path,
                             const base::Closure& callback)
      : file_path_(trace_file_path),
        completion_callback_(callback),
        file_(NULL) {}

  void AddTraceChunk(const std::string& chunk) override {
    std::string tmp = chunk;
    scoped_refptr<base::RefCountedString> chunk_ptr =
        base::RefCountedString::TakeString(&tmp);
    BrowserThread::PostTask(
        BrowserThread::FILE,
        FROM_HERE,
        base::Bind(
            &FileTraceDataSink::AddTraceChunkOnFileThread, this, chunk_ptr));
  }
  void SetSystemTrace(const std::string& data) override {
    system_trace_ = data;
  }
  void Close() override {
    BrowserThread::PostTask(
        BrowserThread::FILE,
        FROM_HERE,
        base::Bind(&FileTraceDataSink::CloseOnFileThread, this));
  }

 private:
  ~FileTraceDataSink() override { DCHECK(file_ == NULL); }

  void AddTraceChunkOnFileThread(
      const scoped_refptr<base::RefCountedString> chunk) {
    if (file_ != NULL)
      fputc(',', file_);
    else if (!OpenFileIfNeededOnFileThread())
      return;
    ignore_result(fwrite(chunk->data().c_str(), strlen(chunk->data().c_str()),
        1, file_));
  }

  bool OpenFileIfNeededOnFileThread() {
    if (file_ != NULL)
      return true;
    file_ = base::OpenFile(file_path_, "w");
    if (file_ == NULL) {
      LOG(ERROR) << "Failed to open " << file_path_.value();
      return false;
    }
    const char preamble[] = "{\"traceEvents\": [";
    ignore_result(fwrite(preamble, strlen(preamble), 1, file_));
    return true;
  }

  void CloseOnFileThread() {
    if (OpenFileIfNeededOnFileThread()) {
      fputc(']', file_);
      if (!system_trace_.empty()) {
        const char systemTraceEvents[] = ",\"systemTraceEvents\": ";
        ignore_result(fwrite(systemTraceEvents, strlen(systemTraceEvents),
            1, file_));
        ignore_result(fwrite(system_trace_.c_str(),
            strlen(system_trace_.c_str()), 1, file_));
      }
      fputc('}', file_);
      base::CloseFile(file_);
      file_ = NULL;
    }
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&FileTraceDataSink::FinalizeOnUIThread, this));
  }

  void FinalizeOnUIThread() { completion_callback_.Run(); }

  base::FilePath file_path_;
  base::Closure completion_callback_;
  FILE* file_;
  std::string system_trace_;

  DISALLOW_COPY_AND_ASSIGN(FileTraceDataSink);
};

class StringTraceDataSink : public TracingController::TraceDataSink {
 public:
  typedef base::Callback<void(base::RefCountedString*)> CompletionCallback;

  explicit StringTraceDataSink(CompletionCallback callback)
      : completion_callback_(callback) {}

  // TracingController::TraceDataSink implementation
  void AddTraceChunk(const std::string& chunk) override {
    if (!trace_.empty())
      trace_ += ",";
    trace_ += chunk;
  }
  void SetSystemTrace(const std::string& data) override {
    system_trace_ = data;
  }
  void Close() override {
    std::string result = "{\"traceEvents\":[" + trace_ + "]";
    if (!system_trace_.empty())
      result += ",\"systemTraceEvents\": " + system_trace_;
    result += "}";

    scoped_refptr<base::RefCountedString> str =
        base::RefCountedString::TakeString(&result);
    completion_callback_.Run(str.get());
  }

 private:
  ~StringTraceDataSink() override {}

  std::string trace_;
  std::string system_trace_;
  CompletionCallback completion_callback_;

  DISALLOW_COPY_AND_ASSIGN(StringTraceDataSink);
};

class CompressedStringTraceDataSink : public TracingController::TraceDataSink {
 public:
  explicit CompressedStringTraceDataSink(
      scoped_refptr<TracingController::TraceDataEndpoint> endpoint)
      : endpoint_(endpoint), already_tried_open_(false) {}

  void AddTraceChunk(const std::string& chunk) override {
    std::string tmp = chunk;
    scoped_refptr<base::RefCountedString> chunk_ptr =
        base::RefCountedString::TakeString(&tmp);
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&CompressedStringTraceDataSink::AddTraceChunkOnFileThread,
                   this, chunk_ptr));
  }

  void SetSystemTrace(const std::string& data) override {
    system_trace_ = data;
  }

  void Close() override {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&CompressedStringTraceDataSink::CloseOnFileThread, this));
  }

 private:
  ~CompressedStringTraceDataSink() override {}

  bool OpenZStreamOnFileThread() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    if (stream_)
      return true;

    if (already_tried_open_)
      return false;

    already_tried_open_ = true;
    stream_.reset(new z_stream);
    *stream_ = {0};
    stream_->zalloc = Z_NULL;
    stream_->zfree = Z_NULL;
    stream_->opaque = Z_NULL;

    int result = deflateInit2(stream_.get(), Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                              // 16 is added to produce a gzip header + trailer.
                              MAX_WBITS + 16,
                              8,  // memLevel = 8 is default.
                              Z_DEFAULT_STRATEGY);
    return result == 0;
  }

  void AddTraceChunkOnFileThread(
      const scoped_refptr<base::RefCountedString> chunk_ptr) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    std::string trace;
    if (compressed_trace_data_.empty())
      trace = "{\"traceEvents\":[";
    else
      trace = ",";
    trace += chunk_ptr->data();
    AddTraceChunkAndCompressOnFileThread(trace, false);
  }

  void AddTraceChunkAndCompressOnFileThread(const std::string& chunk,
                                            bool finished) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    if (!OpenZStreamOnFileThread())
      return;

    const int kChunkSize = 0x4000;

    char buffer[kChunkSize];
    int err;
    stream_->avail_in = chunk.size();
    stream_->next_in = (unsigned char*)chunk.data();
    do {
      stream_->avail_out = kChunkSize;
      stream_->next_out = (unsigned char*)buffer;
      err = deflate(stream_.get(), finished ? Z_FINISH : Z_NO_FLUSH);
      if (err != Z_OK && (err != Z_STREAM_END && finished)) {
        stream_.reset();
        return;
      }

      int bytes = kChunkSize - stream_->avail_out;
      if (bytes) {
        std::string compressed_chunk = std::string(buffer, bytes);
        compressed_trace_data_ += compressed_chunk;
        endpoint_->ReceiveTraceChunk(compressed_chunk);
      }
    } while (stream_->avail_out == 0);
  }

  void CloseOnFileThread() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    if (!OpenZStreamOnFileThread())
      return;

    if (compressed_trace_data_.empty())
      AddTraceChunkAndCompressOnFileThread("{\"traceEvents\":[", false);

    AddTraceChunkAndCompressOnFileThread("]", false);
    if (!system_trace_.empty()) {
      AddTraceChunkAndCompressOnFileThread(
          ",\"systemTraceEvents\": " + system_trace_, false);
    }
    AddTraceChunkAndCompressOnFileThread("}", true);

    deflateEnd(stream_.get());
    stream_.reset();

    endpoint_->ReceiveTraceFinalContents(compressed_trace_data_);
  }

  scoped_refptr<TracingController::TraceDataEndpoint> endpoint_;
  scoped_ptr<z_stream> stream_;
  bool already_tried_open_;
  std::string compressed_trace_data_;
  std::string system_trace_;

  DISALLOW_COPY_AND_ASSIGN(CompressedStringTraceDataSink);
};

}  // namespace

scoped_refptr<TracingController::TraceDataSink>
TracingController::CreateStringSink(
    const base::Callback<void(base::RefCountedString*)>& callback) {
  return new StringTraceDataSink(callback);
}

scoped_refptr<TracingController::TraceDataSink>
TracingController::CreateCompressedStringSink(
    scoped_refptr<TracingController::TraceDataEndpoint> endpoint) {
  return new CompressedStringTraceDataSink(endpoint);
}

scoped_refptr<TracingController::TraceDataSink>
TracingController::CreateFileSink(const base::FilePath& file_path,
                                  const base::Closure& callback) {
  return new FileTraceDataSink(file_path, callback);
}

}  // namespace content
