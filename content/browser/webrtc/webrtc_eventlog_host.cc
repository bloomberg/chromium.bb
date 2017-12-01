// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webrtc/webrtc_eventlog_host.h"

#include <algorithm>
#include <string>

#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task_scheduler/post_task.h"
#include "base/task_scheduler/task_traits.h"
#include "base/threading/thread_restrictions.h"
#include "content/browser/webrtc/webrtc_internals.h"
#include "content/common/media/peer_connection_tracker_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"

#if defined(OS_WIN)
#define IntToStringType base::IntToString16
#else
#define IntToStringType base::IntToString
#endif

namespace content {

namespace {

// In addition to the limit to the number of files given below, the size of the
// files is also capped, see content/renderer/media/peer_connection_tracker.cc.
#if defined(OS_ANDROID)
const int kMaxNumberLogFiles = 3;
#else
const int kMaxNumberLogFiles = 5;
#endif

// Appends the IDs to the RTC event log file name.
base::FilePath GetWebRtcEventLogPath(const base::FilePath& base_file,
                                     int render_process_id,
                                     int connection_id) {
  return base_file.AddExtension(IntToStringType(render_process_id))
      .AddExtension(IntToStringType(connection_id));
}

// Opens a logfile to pass on to the renderer.
IPC::PlatformFileForTransit CreateEventLogFileForChildProcess(
    const base::FilePath& base_path,
    int render_process_id,
    int connection_id) {
  base::AssertBlockingAllowed();
  base::FilePath file_path =
      GetWebRtcEventLogPath(base_path, render_process_id, connection_id);
  base::File event_log_file(
      file_path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  if (!event_log_file.IsValid()) {
    PLOG(ERROR) << "Could not open WebRTC event log file, error="
                << event_log_file.error_details();
    return IPC::InvalidPlatformFileForTransit();
  }
  return IPC::TakePlatformFileForTransit(std::move(event_log_file));
}

}  // namespace

WebRTCEventLogHost::WebRTCEventLogHost(int render_process_id)
    : render_process_id_(render_process_id),
      rtc_event_logging_enabled_(false),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* webrtc_internals = WebRTCInternals::GetInstance();
  if (webrtc_internals->IsEventLogRecordingsEnabled())
    StartWebRTCEventLog(webrtc_internals->GetEventLogFilePath());
}

WebRTCEventLogHost::~WebRTCEventLogHost() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (int lid : active_peer_connection_local_ids_) {
    RtcEventLogRemoved(lid);
  }
}

void WebRTCEventLogHost::PeerConnectionAdded(int peer_connection_local_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (std::find(active_peer_connection_local_ids_.begin(),
                active_peer_connection_local_ids_.end(),
                peer_connection_local_id) ==
      active_peer_connection_local_ids_.end()) {
    active_peer_connection_local_ids_.push_back(peer_connection_local_id);
    if (rtc_event_logging_enabled_ &&
        ActivePeerConnectionsWithLogFiles().size() < kMaxNumberLogFiles) {
      StartEventLogForPeerConnection(peer_connection_local_id);
    }
  }
}

void WebRTCEventLogHost::PeerConnectionRemoved(int peer_connection_local_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const auto lid = std::find(active_peer_connection_local_ids_.begin(),
                             active_peer_connection_local_ids_.end(),
                             peer_connection_local_id);
  if (lid != active_peer_connection_local_ids_.end()) {
    active_peer_connection_local_ids_.erase(lid);
  }
  RtcEventLogRemoved(peer_connection_local_id);
}

bool WebRTCEventLogHost::StartWebRTCEventLog(const base::FilePath& file_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (rtc_event_logging_enabled_)
    return false;
  rtc_event_logging_enabled_ = true;
  base_file_path_ = file_path;
  for (int local_id : active_peer_connection_local_ids_)
    StartEventLogForPeerConnection(local_id);
  return true;
}

bool WebRTCEventLogHost::StopWebRTCEventLog() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!rtc_event_logging_enabled_) {
    DCHECK(ActivePeerConnectionsWithLogFiles().empty());
    return false;
  }
  rtc_event_logging_enabled_ = false;
  RenderProcessHost* host = RenderProcessHost::FromID(render_process_id_);
  if (host) {
    for (int local_id : active_peer_connection_local_ids_)
      host->Send(new PeerConnectionTracker_StopEventLog(local_id));
  }
  ActivePeerConnectionsWithLogFiles().clear();
  return true;
}

base::WeakPtr<WebRTCEventLogHost> WebRTCEventLogHost::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

std::vector<WebRTCEventLogHost::PeerConnectionKey>&
WebRTCEventLogHost::ActivePeerConnectionsWithLogFiles() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CR_DEFINE_STATIC_LOCAL(std::vector<WebRTCEventLogHost::PeerConnectionKey>,
                         vector, ());
  return vector;
}

void WebRTCEventLogHost::StartEventLogForPeerConnection(
    int peer_connection_local_id) {
  if (ActivePeerConnectionsWithLogFiles().size() < kMaxNumberLogFiles) {
    RtcEventLogAdded(peer_connection_local_id);
    base::PostTaskWithTraitsAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BACKGROUND},
        base::Bind(&CreateEventLogFileForChildProcess, base_file_path_,
                   render_process_id_, peer_connection_local_id),
        base::Bind(&WebRTCEventLogHost::SendEventLogFileToRenderer,
                   weak_ptr_factory_.GetWeakPtr(), peer_connection_local_id));
  }
}

void WebRTCEventLogHost::SendEventLogFileToRenderer(
    int peer_connection_local_id,
    IPC::PlatformFileForTransit file_for_transit) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (file_for_transit == IPC::InvalidPlatformFileForTransit()) {
    bool removed = RtcEventLogRemoved(peer_connection_local_id);
    DCHECK(removed);
    return;
  }
  RenderProcessHost* rph = RenderProcessHost::FromID(render_process_id_);
  if (rph) {
    rph->Send(new PeerConnectionTracker_StartEventLog(peer_connection_local_id,
                                                      file_for_transit));
  } else {
    bool removed = RtcEventLogRemoved(peer_connection_local_id);
    DCHECK(removed);
    IPC::PlatformFileForTransitToFile(file_for_transit).Close();
  }
}

void WebRTCEventLogHost::RtcEventLogAdded(int peer_connection_local_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto& active_peer_connections_with_log_files =
      ActivePeerConnectionsWithLogFiles();
  PeerConnectionKey pc_key = {render_process_id_, peer_connection_local_id};
  DCHECK(std::find(active_peer_connections_with_log_files.begin(),
                   active_peer_connections_with_log_files.end(),
                   pc_key) == active_peer_connections_with_log_files.end());
  active_peer_connections_with_log_files.push_back(pc_key);
}

bool WebRTCEventLogHost::RtcEventLogRemoved(int peer_connection_local_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto& active_peer_connections_with_log_files =
      ActivePeerConnectionsWithLogFiles();
  PeerConnectionKey pc_key = {render_process_id_, peer_connection_local_id};
  const auto it =
      std::find(active_peer_connections_with_log_files.begin(),
                active_peer_connections_with_log_files.end(), pc_key);
  if (it != active_peer_connections_with_log_files.end()) {
    active_peer_connections_with_log_files.erase(it);
    return true;
  }
  return false;
}

}  // namespace content
