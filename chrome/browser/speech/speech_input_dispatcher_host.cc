// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/speech_input_dispatcher_host.h"

#include "base/singleton.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_view_host_notification_task.h"
#include "chrome/browser/renderer_host/resource_message_filter.h"
#include "chrome/common/render_messages.h"

namespace speech_input {

//----------------------------- SpeechInputCallers -----------------------------

// A singleton class to map the tuple
// (render-process-id, render-view-id, requestid) to a single ID which is passed
// through rest of the speech code.
class SpeechInputDispatcherHost::SpeechInputCallers {
 public:
  // Creates a new ID for a given tuple.
  int CreateId(int render_process_id, int render_view_id, int request_id);

  // Returns the ID for a tuple assuming the ID was created earlier.
  int GetId(int render_process_id, int render_view_id, int request_id);

  // Removes the ID and associated tuple from the map.
  void RemoveId(int id);

  // Getters for the various tuple elements for the given ID.
  int render_process_id(int id);
  int render_view_id(int id);
  int request_id(int id);

 private:
  struct CallerInfo {
    int render_process_id;
    int render_view_id;
    int request_id;
  };
  friend struct DefaultSingletonTraits<SpeechInputCallers>;

  SpeechInputCallers();

  std::map<int, CallerInfo> callers_;
  int next_id_;
};

SpeechInputDispatcherHost::SpeechInputCallers::SpeechInputCallers()
    : next_id_(1) {
}

int SpeechInputDispatcherHost::SpeechInputCallers::GetId(int render_process_id,
                                                         int render_view_id,
                                                         int request_id) {
  for (std::map<int, CallerInfo>::iterator it = callers_.begin();
      it != callers_.end(); it++) {
    const CallerInfo& item = it->second;
    if (item.render_process_id == render_process_id &&
        item.render_view_id == render_view_id &&
        item.request_id == request_id) {
      return it->first;
    }
  }

  // Not finding an entry here is valid since a cancel/stop may have been issued
  // by the renderer and before it received our response the user may have
  // clicked the button to stop again. The caller of this method should take
  // care of this case.
  return 0;
}

int SpeechInputDispatcherHost::SpeechInputCallers::CreateId(
    int render_process_id,
    int render_view_id,
    int request_id) {
  CallerInfo info;
  info.render_process_id = render_process_id;
  info.render_view_id = render_view_id;
  info.request_id = request_id;
  callers_[next_id_] = info;
  return next_id_++;
}

void SpeechInputDispatcherHost::SpeechInputCallers::RemoveId(int id) {
  callers_.erase(id);
}

int SpeechInputDispatcherHost::SpeechInputCallers::render_process_id(int id) {
  return callers_[id].render_process_id;
}

int SpeechInputDispatcherHost::SpeechInputCallers::render_view_id(int id) {
  return callers_[id].render_view_id;
}

int SpeechInputDispatcherHost::SpeechInputCallers::request_id(int id) {
  return callers_[id].request_id;
}

//-------------------------- SpeechInputDispatcherHost -------------------------

SpeechInputManager::AccessorMethod*
    SpeechInputDispatcherHost::manager_accessor_ = &SpeechInputManager::Get;

SpeechInputDispatcherHost::SpeechInputDispatcherHost(
    int resource_message_filter_process_id)
    : resource_message_filter_process_id_(resource_message_filter_process_id),
      callers_(Singleton<SpeechInputCallers>::get()) {
  // This is initialized by ResourceMessageFilter. Do not add any non-trivial
  // initialization here, instead do it lazily when required (e.g. see the
  // method |manager()|) or add an Init() method.
}

SpeechInputDispatcherHost::~SpeechInputDispatcherHost() {
}

SpeechInputManager* SpeechInputDispatcherHost::manager() {
  return (*manager_accessor_)();
}

bool SpeechInputDispatcherHost::OnMessageReceived(
    const IPC::Message& msg, bool* msg_was_ok) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(SpeechInputDispatcherHost, msg, *msg_was_ok)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SpeechInput_StartRecognition,
                        OnStartRecognition)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SpeechInput_CancelRecognition,
                        OnCancelRecognition)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SpeechInput_StopRecording,
                        OnStopRecording)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void SpeechInputDispatcherHost::OnStartRecognition(
    int render_view_id,
    int request_id,
    const gfx::Rect& element_rect,
    const std::string& language,
    const std::string& grammar) {
  int caller_id = callers_->CreateId(resource_message_filter_process_id_,
                                     render_view_id, request_id);
  manager()->StartRecognition(this, caller_id,
                              resource_message_filter_process_id_,
                              render_view_id, element_rect,
                              language, grammar);
}

void SpeechInputDispatcherHost::OnCancelRecognition(int render_view_id,
                                                    int request_id) {
  int caller_id = callers_->GetId(resource_message_filter_process_id_,
                                 render_view_id, request_id);
  if (caller_id) {
    manager()->CancelRecognition(caller_id);
    callers_->RemoveId(caller_id);  // Request sequence ended so remove mapping.
  }
}

void SpeechInputDispatcherHost::OnStopRecording(int render_view_id,
                                                int request_id) {
  int caller_id = callers_->GetId(resource_message_filter_process_id_,
                                  render_view_id, request_id);
  if (caller_id)
    manager()->StopRecording(caller_id);
}

void SpeechInputDispatcherHost::SendMessageToRenderView(IPC::Message* message,
                                                        int render_view_id) {
  CallRenderViewHost(
      resource_message_filter_process_id_, render_view_id,
      &RenderViewHost::Send, message);
}

void SpeechInputDispatcherHost::SetRecognitionResult(
    int caller_id, const SpeechInputResultArray& result) {
  VLOG(1) << "SpeechInputDispatcherHost::SetRecognitionResult enter";
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  int caller_render_view_id = callers_->render_view_id(caller_id);
  int caller_request_id = callers_->request_id(caller_id);
  SendMessageToRenderView(
      new ViewMsg_SpeechInput_SetRecognitionResult(caller_render_view_id,
                                                   caller_request_id,
                                                   result),
      caller_render_view_id);
  VLOG(1) << "SpeechInputDispatcherHost::SetRecognitionResult exit";
}

void SpeechInputDispatcherHost::DidCompleteRecording(int caller_id) {
  VLOG(1) << "SpeechInputDispatcherHost::DidCompleteRecording enter";
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  int caller_render_view_id = callers_->render_view_id(caller_id);
  int caller_request_id = callers_->request_id(caller_id);
  SendMessageToRenderView(
      new ViewMsg_SpeechInput_RecordingComplete(caller_render_view_id,
                                                caller_request_id),
      caller_render_view_id);
  VLOG(1) << "SpeechInputDispatcherHost::DidCompleteRecording exit";
}

void SpeechInputDispatcherHost::DidCompleteRecognition(int caller_id) {
  VLOG(1) << "SpeechInputDispatcherHost::DidCompleteRecognition enter";
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  int caller_render_view_id = callers_->render_view_id(caller_id);
  int caller_request_id = callers_->request_id(caller_id);
  SendMessageToRenderView(
      new ViewMsg_SpeechInput_RecognitionComplete(caller_render_view_id,
                                                  caller_request_id),
      caller_render_view_id);
  callers_->RemoveId(caller_id);  // Request sequence ended, so remove mapping.
  VLOG(1) << "SpeechInputDispatcherHost::DidCompleteRecognition exit";
}

}  // namespace speech_input
