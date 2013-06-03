// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc_logging_handler_host.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "base/shared_memory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/cros_settings_names.h"
#include "chrome/browser/media/webrtc_log_uploader.h"
#include "chrome/common/media/webrtc_logging_messages.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserThread;


#if defined(OS_ANDROID)
const size_t kWebRtcLogSize = 1 * 1024 * 1024;  // 1 MB
#else
const size_t kWebRtcLogSize = 6 * 1024 * 1024;  // 6 MB
#endif

WebRtcLoggingHandlerHost::WebRtcLoggingHandlerHost() {}

WebRtcLoggingHandlerHost::~WebRtcLoggingHandlerHost() {}

void WebRtcLoggingHandlerHost::OnChannelClosing() {
  UploadLog();
  content::BrowserMessageFilter::OnChannelClosing();
}

void WebRtcLoggingHandlerHost::OnDestruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

bool WebRtcLoggingHandlerHost::OnMessageReceived(const IPC::Message& message,
                                                 bool* message_was_ok) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(WebRtcLoggingHandlerHost, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(WebRtcLoggingMsg_OpenLog, OnOpenLog)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()

  return handled;
}

void WebRtcLoggingHandlerHost::OnOpenLog(const std::string& app_session_id,
                                         const std::string& app_url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  app_session_id_ = app_session_id;
  app_url_ = app_url;
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(
      &WebRtcLoggingHandlerHost::OpenLogIfAllowed, this));
}

void WebRtcLoggingHandlerHost::OpenLogIfAllowed() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  bool enabled = false;

  // If the user permits metrics reporting / crash uploading with the checkbox
  // in the prefs, we allow uploading automatically. We disable uploading
  // completely for non-official builds.
#if defined(GOOGLE_CHROME_BUILD)
#if defined(OS_CHROMEOS)
  chromeos::CrosSettings::Get()->GetBoolean(chromeos::kStatsReportingPref,
                                            &enabled);
#else
  enabled = g_browser_process->local_state()->GetBoolean(
      prefs::kMetricsReportingEnabled);
#endif  // #if defined(OS_CHROMEOS)
#endif  // defined(GOOGLE_CHROME_BUILD)
  if (!enabled)
    return;

  if (!g_browser_process->webrtc_log_uploader()->ApplyForStartLogging())
    return;

  system_request_context_ = g_browser_process->system_request_context();
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, base::Bind(
      &WebRtcLoggingHandlerHost::DoOpenLog, this));
}

void WebRtcLoggingHandlerHost::DoOpenLog() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!shared_memory_);

  shared_memory_.reset(new base::SharedMemory());

  if (!shared_memory_->CreateAndMapAnonymous(kWebRtcLogSize)) {
    DLOG(ERROR) << "Failed to create shared memory.";
    Send(new WebRtcLoggingMsg_OpenLogFailed());
    return;
  }

  base::SharedMemoryHandle foreign_memory_handle;
  if (!shared_memory_->ShareToProcess(peer_handle(),
                                     &foreign_memory_handle)) {
    Send(new WebRtcLoggingMsg_OpenLogFailed());
    return;
  }

  Send(new WebRtcLoggingMsg_LogOpened(foreign_memory_handle, kWebRtcLogSize));
}

void WebRtcLoggingHandlerHost::UploadLog() {
  if (!shared_memory_)
    return;

  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE, base::Bind(
      &WebRtcLogUploader::UploadLog,
      base::Unretained(g_browser_process->webrtc_log_uploader()),
      system_request_context_,
      Passed(&shared_memory_),
      kWebRtcLogSize,
      app_session_id_,
      app_url_));
}
