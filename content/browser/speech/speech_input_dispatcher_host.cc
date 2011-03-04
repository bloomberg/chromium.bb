// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/speech_input_dispatcher_host.h"

#include "base/lazy_instance.h"
#include "chrome/common/speech_input_messages.h"

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
  friend struct base::DefaultLazyInstanceTraits<SpeechInputCallers>;

  SpeechInputCallers();

  std::map<int, CallerInfo> callers_;
  int next_id_;
};

static base::LazyInstance<SpeechInputDispatcherHost::SpeechInputCallers>
    g_speech_input_callers(base::LINKER_INITIALIZED);

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

SpeechInputDispatcherHost::SpeechInputDispatcherHost(int render_process_id)
    : render_process_id_(render_process_id),
      may_have_pending_requests_(false) {
  // This is initialized by Browser. Do not add any non-trivial
  // initialization here, instead do it lazily when required (e.g. see the
  // method |manager()|) or add an Init() method.
}

SpeechInputDispatcherHost::~SpeechInputDispatcherHost() {
  // If the renderer crashed for some reason or if we didn't receive a proper
  // Cancel/Stop call for an existing session, cancel such active sessions now.
  // We first check if this dispatcher received any speech IPC requst so that
  // we don't end up creating the speech input manager for web pages which don't
  // use speech input.
  if (may_have_pending_requests_)
    manager()->CancelAllRequestsWithDelegate(this);
}

SpeechInputManager* SpeechInputDispatcherHost::manager() {
  return (*manager_accessor_)();
}

bool SpeechInputDispatcherHost::OnMessageReceived(
    const IPC::Message& message, bool* message_was_ok) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(SpeechInputDispatcherHost, message,
                           *message_was_ok)
    IPC_MESSAGE_HANDLER(SpeechInputHostMsg_StartRecognition,
                        OnStartRecognition)
    IPC_MESSAGE_HANDLER(SpeechInputHostMsg_CancelRecognition,
                        OnCancelRecognition)
    IPC_MESSAGE_HANDLER(SpeechInputHostMsg_StopRecording,
                        OnStopRecording)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void SpeechInputDispatcherHost::OnStartRecognition(
    const SpeechInputHostMsg_StartRecognition_Params &params) {
  int caller_id = g_speech_input_callers.Get().CreateId(
      render_process_id_, params.render_view_id, params.request_id);
  manager()->StartRecognition(this, caller_id,
                              render_process_id_,
                              params.render_view_id, params.element_rect,
                              params.language, params.grammar,
                              params.origin_url);
}

void SpeechInputDispatcherHost::OnCancelRecognition(int render_view_id,
                                                    int request_id) {
  int caller_id = g_speech_input_callers.Get().GetId(
      render_process_id_, render_view_id, request_id);
  if (caller_id) {
    manager()->CancelRecognition(caller_id);
    // Request sequence ended so remove mapping.
    g_speech_input_callers.Get().RemoveId(caller_id);
  }
}

void SpeechInputDispatcherHost::OnStopRecording(int render_view_id,
                                                int request_id) {
  int caller_id = g_speech_input_callers.Get().GetId(
      render_process_id_, render_view_id, request_id);
  if (caller_id)
    manager()->StopRecording(caller_id);
}

void SpeechInputDispatcherHost::SetRecognitionResult(
    int caller_id, const SpeechInputResultArray& result) {
  VLOG(1) << "SpeechInputDispatcherHost::SetRecognitionResult enter";
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  int caller_render_view_id =
      g_speech_input_callers.Get().render_view_id(caller_id);
  int caller_request_id = g_speech_input_callers.Get().request_id(caller_id);
  Send(new SpeechInputMsg_SetRecognitionResult(caller_render_view_id,
                                               caller_request_id,
                                               result));
  VLOG(1) << "SpeechInputDispatcherHost::SetRecognitionResult exit";
}

void SpeechInputDispatcherHost::DidCompleteRecording(int caller_id) {
  VLOG(1) << "SpeechInputDispatcherHost::DidCompleteRecording enter";
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  int caller_render_view_id =
    g_speech_input_callers.Get().render_view_id(caller_id);
  int caller_request_id = g_speech_input_callers.Get().request_id(caller_id);
  Send(new SpeechInputMsg_RecordingComplete(caller_render_view_id,
                                            caller_request_id));
  VLOG(1) << "SpeechInputDispatcherHost::DidCompleteRecording exit";
}

void SpeechInputDispatcherHost::DidCompleteRecognition(int caller_id) {
  VLOG(1) << "SpeechInputDispatcherHost::DidCompleteRecognition enter";
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  int caller_render_view_id =
    g_speech_input_callers.Get().render_view_id(caller_id);
  int caller_request_id = g_speech_input_callers.Get().request_id(caller_id);
  Send(new SpeechInputMsg_RecognitionComplete(caller_render_view_id,
                                              caller_request_id));
  // Request sequence ended, so remove mapping.
  g_speech_input_callers.Get().RemoveId(caller_id);
  VLOG(1) << "SpeechInputDispatcherHost::DidCompleteRecognition exit";
}

}  // namespace speech_input
