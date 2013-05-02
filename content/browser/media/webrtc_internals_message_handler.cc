// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/webrtc_internals_message_handler.h"

#include "content/browser/media/webrtc_internals.h"
#include "content/common/media/peer_connection_tracker_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"

using base::ListValue;

namespace content {

WebRTCInternalsMessageHandler::WebRTCInternalsMessageHandler() {
  WebRTCInternals::GetInstance()->AddObserver(this);
}

WebRTCInternalsMessageHandler::~WebRTCInternalsMessageHandler() {
  WebRTCInternals::GetInstance()->RemoveObserver(this);
}

void WebRTCInternalsMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("getAllUpdates",
      base::Bind(&WebRTCInternalsMessageHandler::OnGetAllUpdates,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback("getAllStats",
      base::Bind(&WebRTCInternalsMessageHandler::OnGetAllStats,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback("startRtpRecording",
      base::Bind(&WebRTCInternalsMessageHandler::OnStartRtpRecording,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback("stopRtpRecording",
      base::Bind(&WebRTCInternalsMessageHandler::OnStopRtpRecording,
                 base::Unretained(this)));
}

void WebRTCInternalsMessageHandler::OnGetAllUpdates(
    const base::ListValue* list) {
  WebRTCInternals::GetInstance()->SendAllUpdates();
}

void WebRTCInternalsMessageHandler::OnGetAllStats(const ListValue* list) {
  for (RenderProcessHost::iterator i(
       content::RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    i.GetCurrentValue()->Send(new PeerConnectionTracker_GetAllStats());
  }
}

void WebRTCInternalsMessageHandler::OnStartRtpRecording(
    const base::ListValue* list) {
  WebRTCInternals::GetInstance()->StartRtpRecording();
}

void WebRTCInternalsMessageHandler::OnStopRtpRecording(
    const base::ListValue* list) {
  WebRTCInternals::GetInstance()->StopRtpRecording();
}

void WebRTCInternalsMessageHandler::OnUpdate(const std::string& command,
                                            const base::Value* args) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::vector<const Value*> args_vector;
  args_vector.push_back(args);
  string16 update = WebUI::GetJavascriptCall(command, args_vector);

  RenderViewHost* host = web_ui()->GetWebContents()->GetRenderViewHost();
  if (host)
    host->ExecuteJavascriptInWebFrame(string16(), update);
}

}  // namespace content
