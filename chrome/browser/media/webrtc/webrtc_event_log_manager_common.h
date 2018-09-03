// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_EVENT_LOG_MANAGER_COMMON_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_EVENT_LOG_MANAGER_COMMON_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace webrtc_event_logging {

// This file is intended for:
// 1. Code shared between WebRtcEventLogManager, WebRtcLocalEventLogManager
//    and WebRtcRemoteEventLogManager.
// 2. Code specific to either of the above classes, but which also needs
//    to be seen by unit tests (such as constants).

extern const size_t kWebRtcEventLogManagerUnlimitedFileSize;

extern const size_t kDefaultMaxLocalLogFileSizeBytes;
extern const size_t kMaxNumberLocalWebRtcEventLogFiles;

extern const size_t kMaxRemoteLogFileSizeBytes;

// Maximum size for a response from Crash, which is the upload ID.
extern const size_t kWebRtcEventLogMaxUploadIdBytes;

// Limit over the number of concurrently active (currently being written to
// disk) remote-bound log files. This limits IO operations, and so it is
// applied globally (all browser contexts are limited together).
extern const size_t kMaxActiveRemoteBoundWebRtcEventLogs;

// Limit over the number of pending logs (logs stored on disk and awaiting to
// be uploaded to a remote server). This limit avoids excessive storage. If a
// user chooses to have multiple profiles (and hence browser contexts) on a
// system, it is assumed that the user has enough storage to accommodate
// the increased storage consumption that comes with it. Therefore, this
// limit is applied per browser context.
extern const size_t kMaxPendingRemoteBoundWebRtcEventLogs;

// Max number of history files that may be kept; after this number is exceeded,
// the oldest logs should be pruned.
extern const size_t kMaxWebRtcEventLogHistoryFiles;

// Overhead incurred by GZIP due to its header and footer.
extern const size_t kGzipOverheadBytes;

// Remote-bound log files' names will be of the format [prefix]_[log_id].[ext],
// where |prefix| is equal to kRemoteBoundWebRtcEventLogFileNamePrefix,
// |log_id| is composed of 32 random characters from '0'-'9' and 'A'-'F',
// and |ext| is the extension determined by the used LogCompressor::Factory,
// which will be either kWebRtcEventLogUncompressedExtension or
// kWebRtcEventLogGzippedExtension.
extern const base::FilePath::CharType
    kRemoteBoundWebRtcEventLogFileNamePrefix[];
extern const base::FilePath::CharType kWebRtcEventLogUncompressedExtension[];
extern const base::FilePath::CharType kWebRtcEventLogGzippedExtension[];

// Logs themselves are kept on disk for kRemoteBoundWebRtcEventLogsMaxRetention,
// or until uploaded. Smaller history files are kept for a longer time, allowing
// Chrome to display on chrome://webrtc-logs/ that these files were captured
// and later uploaded.
extern const base::FilePath::CharType kWebRtcEventLogHistoryExtension[];

// Remote-bound event logs will not be uploaded if the time since their last
// modification (meaning the time when they were completed) exceeds this value.
// Such expired files will be purged from disk when examined.
extern const base::TimeDelta kRemoteBoundWebRtcEventLogsMaxRetention;

// StartRemoteLogging could fail for several reasons, but we only report
// individually those failures that relate to either bad parameters, or calls
// at a time that makes no sense. Anything else  would leak information to
// the JS application (too many pending logs, etc.), and is not actionable
// anyhow.
// These are made globally visible so that unit tests may check for them.
extern const char kStartRemoteLoggingFailureFeatureDisabled[];
extern const char kStartRemoteLoggingFailureUnlimitedSizeDisallowed[];
extern const char kStartRemoteLoggingFailureMaxSizeTooSmall[];
extern const char kStartRemoteLoggingFailureMaxSizeTooLarge[];
extern const char kStartRemoteLoggingFailureUnknownOrInactivePeerConnection[];
extern const char kStartRemoteLoggingFailureAlreadyLogging[];
extern const char kStartRemoteLoggingFailureGeneric[];

// For a given Chrome session, this is a unique key for PeerConnections.
// It's not, however, unique between sessions (after Chrome is restarted).
struct WebRtcEventLogPeerConnectionKey {
  using BrowserContextId = uintptr_t;

  constexpr WebRtcEventLogPeerConnectionKey()
      : WebRtcEventLogPeerConnectionKey(
            /* render_process_id = */ 0,
            /* lid = */ 0,
            reinterpret_cast<BrowserContextId>(nullptr)) {}

  constexpr WebRtcEventLogPeerConnectionKey(int render_process_id,
                                            int lid,
                                            BrowserContextId browser_context_id)
      : render_process_id(render_process_id),
        lid(lid),
        browser_context_id(browser_context_id) {}

  bool operator==(const WebRtcEventLogPeerConnectionKey& other) const {
    // Each RPH is associated with exactly one BrowserContext.
    DCHECK(render_process_id != other.render_process_id ||
           browser_context_id == other.browser_context_id);

    const bool equal = std::tie(render_process_id, lid) ==
                       std::tie(other.render_process_id, other.lid);
    return equal;
  }

  bool operator<(const WebRtcEventLogPeerConnectionKey& other) const {
    // Each RPH is associated with exactly one BrowserContext.
    DCHECK(render_process_id != other.render_process_id ||
           browser_context_id == other.browser_context_id);

    return std::tie(render_process_id, lid) <
           std::tie(other.render_process_id, other.lid);
  }

  // These two fields are the actual key; any peer connection is uniquely
  // identifiable by the renderer process in which it lives, and its ID within
  // that process.
  int render_process_id;
  int lid;  // Renderer-local PeerConnection ID.

  // The BrowserContext is not actually part of the key, but each PeerConnection
  // is associated with a BrowserContext, and that BrowserContext is almost
  // always necessary, so it makes sense to remember it along with the key.
  BrowserContextId browser_context_id;
};

// Sentinel value for an unknown BrowserContext.
extern const WebRtcEventLogPeerConnectionKey::BrowserContextId
    kNullBrowserContextId;

// Holds housekeeping information about log files.
struct WebRtcLogFileInfo {
  WebRtcLogFileInfo(
      WebRtcEventLogPeerConnectionKey::BrowserContextId browser_context_id,
      const base::FilePath& path,
      base::Time last_modified)
      : browser_context_id(browser_context_id),
        path(path),
        last_modified(last_modified) {}

  WebRtcLogFileInfo(const WebRtcLogFileInfo& other)
      : browser_context_id(other.browser_context_id),
        path(other.path),
        last_modified(other.last_modified) {}

  bool operator<(const WebRtcLogFileInfo& other) const {
    if (last_modified != other.last_modified) {
      return last_modified < other.last_modified;
    }
    return path < other.path;  // Break ties arbitrarily, but consistently.
  }

  // The BrowserContext which produced this file.
  const WebRtcEventLogPeerConnectionKey::BrowserContextId browser_context_id;

  // The path to the log file itself.
  const base::FilePath path;

  // |last_modified| recorded at BrowserContext initialization. Chrome will
  // not modify it afterwards, and neither should the user.
  const base::Time last_modified;
};

// An observer for notifications of local log files being started/stopped, and
// the paths which will be used for these logs.
class WebRtcLocalEventLogsObserver {
 public:
  virtual void OnLocalLogStarted(WebRtcEventLogPeerConnectionKey key,
                                 const base::FilePath& file_path) = 0;
  virtual void OnLocalLogStopped(WebRtcEventLogPeerConnectionKey key) = 0;

 protected:
  virtual ~WebRtcLocalEventLogsObserver() = default;
};

// An observer for notifications of remote-bound log files being
// started/stopped. The start event would likely only interest unit tests
// (because it exposes the randomized filename to them). The stop event is of
// general interest, because it would often mean that WebRTC can stop sending
// us event logs for this peer connection.
// Some cases where OnRemoteLogStopped would be called include:
// 1. The PeerConnection has become inactive.
// 2. The file's maximum size has been reached.
// 3. Any type of error while writing to the file.
class WebRtcRemoteEventLogsObserver {
 public:
  virtual void OnRemoteLogStarted(WebRtcEventLogPeerConnectionKey key,
                                  const base::FilePath& file_path) = 0;
  virtual void OnRemoteLogStopped(WebRtcEventLogPeerConnectionKey key) = 0;

 protected:
  virtual ~WebRtcRemoteEventLogsObserver() = default;
};

// Writes a log to a file while observing a maximum size.
class LogFileWriter {
 public:
  class Factory {
   public:
    virtual ~Factory() = default;

    // The smallest size a log file of this type may assume.
    virtual size_t MinFileSizeBytes() const = 0;

    // The extension type associated with this type of log files.
    virtual base::FilePath::StringPieceType Extension() const = 0;

    // Instantiate and initialize a LogFileWriter.
    // If creation or initialization fail, an empty unique_ptr will be returned,
    // and it will be guaranteed that the file itself is not created. (If |path|
    // had pointed to an existing file, that file will be deleted.)
    // If !max_file_size_bytes.has_value(), the LogFileWriter is unlimited.
    virtual std::unique_ptr<LogFileWriter> Create(
        const base::FilePath& path,
        base::Optional<size_t> max_file_size_bytes) const = 0;
  };

  virtual ~LogFileWriter() = default;

  // Init() must be called on each LogFileWriter exactly once, before it's used.
  // If initialization fails, no further actions may be performed on the object
  // other than Close() and Delete().
  virtual bool Init() = 0;

  // Getter for the path of the file |this| wraps.
  virtual const base::FilePath& path() const = 0;

  // Whether the maximum file size was reached.
  virtual bool MaxSizeReached() const = 0;

  // Writes to the log file while respecting the file's size limit.
  // True is returned if and only if the message was written to the file in
  // it entirety. That is, |false| is returned either if a genuine error
  // occurs, or when the budget does not allow the next write.
  // If |false| is ever returned, only Close() and Delete() may subsequently
  // be called.
  // The function does *not* close the file.
  // The function may not be called if MaxSizeReached().
  virtual bool Write(const std::string& input) = 0;

  // If the file was successfully closed, true is returned, and the file may
  // now be used. Otherwise, the file is deleted, and false is returned.
  virtual bool Close() = 0;

  // Delete the file from disk.
  virtual void Delete() = 0;
};

// Produces LogFileWriter instances that perform no compression.
class BaseLogFileWriterFactory : public LogFileWriter::Factory {
 public:
  ~BaseLogFileWriterFactory() override = default;

  size_t MinFileSizeBytes() const override;

  base::FilePath::StringPieceType Extension() const override;

  std::unique_ptr<LogFileWriter> Create(
      const base::FilePath& path,
      base::Optional<size_t> max_file_size_bytes) const override;
};

// Interface for a class that provides compression of a stream, while attempting
// to observe a limit on the size.
//
// One should note that:
// * For compressors that use a footer, to guarantee proper decompression,
//   the footer must be written to the file.
// * In such a case, usually, nothing can be omitted from the file, or the
//   footer's CRC (if used) would be wrong.
// * Determining a string's size pre-compression, without performing the actual
//   compression, is heuristic in nature.
//
// Therefore, compression might terminate (FULL) earlier than it
// must, or even in theory (which we attempt to avoid in practice) exceed the
// size allowed it, in which case the file will be discarded (ERROR).
class LogCompressor {
 public:
  // By subclassing this factory, concrete implementations of LogCompressor can
  // be produced by unit tests, while keeping their definition in the .cc file.
  // (Only the factory needs to be declared in the header.)
  class Factory {
   public:
    virtual ~Factory() = default;

    // The smallest size a log file of this type may assume.
    virtual size_t MinSizeBytes() const = 0;

    // Returns a LogCompressor if the parameters are valid and all
    // initializations are successful; en empty unique_ptr otherwise.
    // If !max_size_bytes.has_value(), an unlimited compressor is created.
    virtual std::unique_ptr<LogCompressor> Create(
        base::Optional<size_t> max_size_bytes) const = 0;
  };

  // Result of a call to Compress().
  // * OK and ERROR_ENCOUNTERED are self-explanatory.
  // * DISALLOWED means that, due to budget constraints, the input could
  //   not be compressed. The stream is still in a legal state, but only
  //   a call to CreateFooter() is now allowed.
  enum class Result { OK, DISALLOWED, ERROR_ENCOUNTERED };

  virtual ~LogCompressor() = default;

  // Produces a compression header and writes it to |output|.
  // The size does not count towards the max size limit.
  // Guaranteed not to fail (nothing can realistically go wrong).
  virtual void CreateHeader(std::string* output) = 0;

  // Compresses |input| into |output|.
  // * If compression succeeded, and the budget was observed, OK is returned.
  // * If the compressor thinks the string, once compressed, will exceed the
  //   maximum size (when combined with previously compressed strings),
  //   compression will not be done, and DISALLOWED will be returned.
  //   This allows producing a valid footer without exceeding the size limit.
  // * Unexpected errors in the underlying compressor (e.g. zlib, etc.),
  //   or unexpectedly getting a compressed string which exceeds the budget,
  //   will return ERROR_ENCOUNTERED.
  // This function may not be called again if DISALLOWED or ERROR_ENCOUNTERED
  // were ever returned before, or after CreateFooter() was called.
  virtual Result Compress(const std::string& input, std::string* output) = 0;

  // Produces a compression footer and writes it to |output|.
  // The footer does not count towards the max size limit.
  // May not be called more than once, or if Compress() returned ERROR.
  virtual bool CreateFooter(std::string* output) = 0;
};

// Estimates the compressed size, without performing compression (except in
// unit tests, where performance is of lesser importance).
// This interface allows unit tests to simulate specific cases, such as
// over/under-estimation, and show that the code using the LogCompressor
// deals with them correctly. (E.g., if the estimation expects the compression
// to not go over-budget, but then it does.)
// The estimator is expected to be stateful. That is, the order of calls to
// EstimateCompressedSize() should correspond to the order of calls
// to Compress().
class CompressedSizeEstimator {
 public:
  class Factory {
   public:
    virtual ~Factory() = default;
    virtual std::unique_ptr<CompressedSizeEstimator> Create() const = 0;
  };

  virtual ~CompressedSizeEstimator() = default;

  virtual size_t EstimateCompressedSize(const std::string& input) const = 0;
};

// Provides a conservative estimation of the number of bytes required to
// compress a string using GZIP. This estimation is not expected to ever
// be overly optimistic, but the code using it should nevertheless be prepared
// to deal with that theoretical possibility.
class DefaultGzippedSizeEstimator : public CompressedSizeEstimator {
 public:
  class Factory : public CompressedSizeEstimator::Factory {
   public:
    ~Factory() override = default;

    std::unique_ptr<CompressedSizeEstimator> Create() const override;
  };

  ~DefaultGzippedSizeEstimator() override = default;

  size_t EstimateCompressedSize(const std::string& input) const override;
};

// Interface for producing LogCompressorGzip objects.
class GzipLogCompressorFactory : public LogCompressor::Factory {
 public:
  explicit GzipLogCompressorFactory(
      std::unique_ptr<CompressedSizeEstimator::Factory> estimator_factory);
  ~GzipLogCompressorFactory() override;

  size_t MinSizeBytes() const override;

  std::unique_ptr<LogCompressor> Create(
      base::Optional<size_t> max_size_bytes) const override;

 private:
  std::unique_ptr<CompressedSizeEstimator::Factory> estimator_factory_;
};

// Produces LogFileWriter instances that perform compression using GZIP.
class GzippedLogFileWriterFactory : public LogFileWriter::Factory {
 public:
  explicit GzippedLogFileWriterFactory(
      std::unique_ptr<GzipLogCompressorFactory> gzip_compressor_factory);

  ~GzippedLogFileWriterFactory() override;

  size_t MinFileSizeBytes() const override;

  base::FilePath::StringPieceType Extension() const override;

  std::unique_ptr<LogFileWriter> Create(
      const base::FilePath& path,
      base::Optional<size_t> max_file_size_bytes) const override;

 private:
  std::unique_ptr<GzipLogCompressorFactory> gzip_compressor_factory_;
};

// Translate a BrowserContext into an ID. This lets us associate PeerConnections
// with BrowserContexts, while making sure that we never call the
// BrowserContext's methods outside of the UI thread (because we can't call them
// at all without a cast that would alert us to the danger).
WebRtcEventLogPeerConnectionKey::BrowserContextId GetBrowserContextId(
    const content::BrowserContext* browser_context);

// Fetches the BrowserContext associated with the render process ID, then
// returns its BrowserContextId. (If the render process has already died,
// it would have no BrowserContext associated, so the ID associated with a
// null BrowserContext will be returned.)
WebRtcEventLogPeerConnectionKey::BrowserContextId GetBrowserContextId(
    int render_process_id);

// Given a BrowserContext's directory, return the path to the directory where
// we store the pending remote-bound logs associated with this BrowserContext.
// This function may be called on any task queue.
base::FilePath GetRemoteBoundWebRtcEventLogsDir(
    const base::FilePath& browser_context_dir);

// Given WebRTC event log's path, return the path to the history file that
// is, or would be, associated with it.
base::FilePath GetWebRtcEventLogHistoryFilePath(const base::FilePath& log);

// Attempts to extract the local ID from the file's path. Returns the empty
// string in case of an error.
std::string ExtractRemoteBoundWebRtcEventLogLocalIdFromPath(
    const base::FilePath& path);

}  // namespace webrtc_event_logging

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_EVENT_LOG_MANAGER_COMMON_H_
