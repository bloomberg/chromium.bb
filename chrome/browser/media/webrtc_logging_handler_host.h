// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_LOGGING_HANDLER_HOST_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_LOGGING_HANDLER_HOST_H_

#include "base/basictypes.h"
#include "base/memory/shared_memory.h"
#include "content/public/browser/browser_message_filter.h"

namespace net {
class URLRequestContextGetter;
}  // namespace net

class RenderProcessHost;

// WebRtcLoggingHandlerHost handles operations regarding the WebRTC logging:
// - Opens a shared memory buffer that the handler in the render process
//   writes to.
// - Writes basic machine info to the log.
// - Informs the handler in the render process when to stop logging.
// - Closes the shared memory (and thereby discarding it) or triggers uploading
//   of the log.
// - Detects when channel, i.e. renderer, is going away and possibly triggers
//   uploading the log.
class WebRtcLoggingHandlerHost : public content::BrowserMessageFilter {
 public:
  typedef base::Callback<void(bool, const std::string&)> GenericDoneCallback;
  typedef base::Callback<void(bool, const std::string&, const std::string&)>
      UploadDoneCallback;

  WebRtcLoggingHandlerHost();

  // Sets meta data that will be uploaded along with the log and also written
  // in the beginning of the log. Must be called on the IO thread before calling
  // StartLogging.
  void SetMetaData(const std::map<std::string, std::string>& meta_data,
                   const GenericDoneCallback& callback);

  // Opens a log and starts logging. Must be called on the IO thread.
  void StartLogging(const GenericDoneCallback& callback);

  // Stops logging. Log will remain open until UploadLog or DiscardLog is
  // called. Must be called on the IO thread.
  void StopLogging(const GenericDoneCallback& callback);

  // Uploads the log and discards the local copy. May only be called after
  // logging has stopped. Must be called on the IO thread.
  void UploadLog(const UploadDoneCallback& callback);

  // Called by WebRtcLogUploader when uploading has finished. Must be called on
  // the IO thread.
  void UploadLogDone();

  // Discards the log. May only be called after logging has stopped. Must be
  // called on the IO thread.
  void DiscardLog(const GenericDoneCallback& callback);

  // May be called on any thread. |upload_log_on_render_close_| is used
  // for decision making and it's OK if it changes before the execution based
  // on that decision has finished.
  void set_upload_log_on_render_close(bool should_upload) {
    upload_log_on_render_close_ = should_upload;
  }

 private:
  // States used for protecting from function calls made at non-allowed points
  // in time. For example, StartLogging() is only allowed in CLOSED state.
  // Transitions: SetMetaData(): CLOSED -> CLOSED.
  //              StartLogging(): CLOSED -> STARTING.
  //              Start done: STARTING -> STARTED.
  //              StopLogging(): STARTED -> STOPPING.
  //              Stop done: STOPPING -> STOPPED.
  //              UploadLog(): STOPPED -> UPLOADING.
  //              Upload done: UPLOADING -> CLOSED.
  //              DiscardLog(): STOPPED -> CLOSED.
  enum LoggingState {
    CLOSED,    // Logging not started, no log in memory.
    STARTING,  // Start logging is in progress.
    STARTED,   // Logging started.
    STOPPING,  // Stop logging is in progress.
    STOPPED,   // Logging has been stopped, log still open in memory.
    UPLOADING  // Uploading log is in progress.
  };

  friend class content::BrowserThread;
  friend class base::DeleteHelper<WebRtcLoggingHandlerHost>;

  virtual ~WebRtcLoggingHandlerHost();

  // BrowserMessageFilter implementation.
  virtual void OnChannelClosing() OVERRIDE;
  virtual void OnDestruct() const OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

  void OnLoggingStoppedInRenderer();

  void StartLoggingIfAllowed();
  void DoStartLogging();
  void LogMachineInfo();
  void NotifyLoggingStarted();

  void TriggerUploadLog();

  void FireGenericDoneCallback(GenericDoneCallback* callback,
                               bool success,
                               const std::string& error_message);

  scoped_refptr<net::URLRequestContextGetter> system_request_context_;
  scoped_ptr<base::SharedMemory> shared_memory_;

  // These are only accessed on the IO thread, except when in STARTING state. In
  // this state we are protected since entering any function that alters the
  // state is not allowed.
  std::map<std::string, std::string> meta_data_;

  // These are only accessed on the IO thread.
  GenericDoneCallback start_callback_;
  GenericDoneCallback stop_callback_;
  UploadDoneCallback upload_callback_;

  // Only accessed on the IO thread, except when in STARTING, STOPPING or
  // UPLOADING state if the action fails and the state must be reset. In these
  // states however, we are protected since entering any function that alters
  // the state is not allowed.
  LoggingState logging_state_;

  // Only accessed on the IO thread.
  bool upload_log_on_render_close_;

  // This is the handle to be passed to the render process. It's stored so that
  // it doesn't have to be passed on when posting messages between threads.
  // It's only accessed on the IO thread.
  base::SharedMemoryHandle foreign_memory_handle_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcLoggingHandlerHost);
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_LOGGING_HANDLER_HOST_H_
