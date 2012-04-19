// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/input_tag_speech_dispatcher_host.h"

#include "base/lazy_instance.h"
#include "content/browser/speech/speech_recognition_manager_impl.h"
#include "content/browser/speech/speech_recognizer_impl.h"
#include "content/common/speech_recognition_messages.h"
#include "content/public/browser/speech_recognition_preferences.h"

using content::BrowserThread;

namespace speech {

//----------------------------- Sessions -----------------------------

// TODO(primiano) Remove session handling from here in the next CL. The manager
// shall be the only one in charge of keeping all the context information for
// all recognition sessions.

// A singleton class to map the tuple
// (render-process-id, render-view-id, requestid) to a single ID which is passed
// through rest of the speech code.
class InputTagSpeechDispatcherHost::Sessions {
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
  struct SessionInfo {
    int render_process_id;
    int render_view_id;
    int request_id;
  };
  friend struct base::DefaultLazyInstanceTraits<Sessions>;

  Sessions();

  std::map<int, SessionInfo> sessions_;
  int next_id_;
};

static base::LazyInstance<InputTagSpeechDispatcherHost::Sessions>
    g_sessions = LAZY_INSTANCE_INITIALIZER;

InputTagSpeechDispatcherHost::Sessions::Sessions()
    : next_id_(1) {
}

int InputTagSpeechDispatcherHost::Sessions::GetId(int render_process_id,
                                                 int render_view_id,
                                                 int request_id) {
  for (std::map<int, SessionInfo>::iterator it = sessions_.begin();
      it != sessions_.end(); it++) {
    const SessionInfo& item = it->second;
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

int InputTagSpeechDispatcherHost::Sessions::CreateId(int render_process_id,
                                                    int render_view_id,
                                                    int request_id) {
  SessionInfo info;
  info.render_process_id = render_process_id;
  info.render_view_id = render_view_id;
  info.request_id = request_id;
  sessions_[next_id_] = info;
  return next_id_++;
}

void InputTagSpeechDispatcherHost::Sessions::RemoveId(int id) {
  sessions_.erase(id);
}

int InputTagSpeechDispatcherHost::Sessions::render_process_id(
    int id) {
  return sessions_[id].render_process_id;
}

int InputTagSpeechDispatcherHost::Sessions::render_view_id(
    int id) {
  return sessions_[id].render_view_id;
}

int InputTagSpeechDispatcherHost::Sessions::request_id(int id) {
  return sessions_[id].request_id;
}

//----------------------- InputTagSpeechDispatcherHost ----------------------

SpeechRecognitionManagerImpl* InputTagSpeechDispatcherHost::manager_;

void InputTagSpeechDispatcherHost::set_manager(
    SpeechRecognitionManagerImpl* manager) {
  manager_ = manager;
}

InputTagSpeechDispatcherHost::InputTagSpeechDispatcherHost(
    int render_process_id,
    net::URLRequestContextGetter* context_getter,
    content::SpeechRecognitionPreferences* recognition_preferences)
    : render_process_id_(render_process_id),
      may_have_pending_requests_(false),
      context_getter_(context_getter),
      recognition_preferences_(recognition_preferences) {
  // This is initialized by Browser. Do not add any non-trivial
  // initialization here, instead do it lazily when required (e.g. see the
  // method |manager()|) or add an Init() method.
}

InputTagSpeechDispatcherHost::~InputTagSpeechDispatcherHost() {
  // If the renderer crashed for some reason or if we didn't receive a proper
  // Cancel/Stop call for an existing session, cancel such active sessions now.
  // We first check if this dispatcher received any speech IPC requst so that
  // we don't end up creating the speech input manager for web pages which don't
  // use speech input.
  if (may_have_pending_requests_)
    manager()->CancelAllRequestsWithDelegate(this);
}

SpeechRecognitionManagerImpl* InputTagSpeechDispatcherHost::manager() {
  if (manager_)
    return manager_;
#if defined(ENABLE_INPUT_SPEECH)
  return SpeechRecognitionManagerImpl::GetInstance();
#else
  return NULL;
#endif
}

bool InputTagSpeechDispatcherHost::OnMessageReceived(
    const IPC::Message& message, bool* message_was_ok) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(InputTagSpeechDispatcherHost, message,
                           *message_was_ok)
    IPC_MESSAGE_HANDLER(InputTagSpeechHostMsg_StartRecognition,
                        OnStartRecognition)
    IPC_MESSAGE_HANDLER(InputTagSpeechHostMsg_CancelRecognition,
                        OnCancelRecognition)
    IPC_MESSAGE_HANDLER(InputTagSpeechHostMsg_StopRecording,
                        OnStopRecording)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  if (handled)
    may_have_pending_requests_ = true;
  return handled;
}

void InputTagSpeechDispatcherHost::OnStartRecognition(
    const InputTagSpeechHostMsg_StartRecognition_Params &params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  int session_id = g_sessions.Get().CreateId(
      render_process_id_, params.render_view_id, params.request_id);
  manager()->StartRecognition(this, session_id,
                              render_process_id_,
                              params.render_view_id, params.element_rect,
                              params.language, params.grammar,
                              params.origin_url,
                              context_getter_.get(),
                              recognition_preferences_.get());
}

void InputTagSpeechDispatcherHost::OnCancelRecognition(int render_view_id,
                                                       int request_id) {
  int session_id = g_sessions.Get().GetId(
      render_process_id_, render_view_id, request_id);
  if (session_id) {
    manager()->CancelRecognition(session_id);
    // Request sequence ended so remove mapping.
    g_sessions.Get().RemoveId(session_id);
  }
}

void InputTagSpeechDispatcherHost::OnStopRecording(int render_view_id,
                                                   int request_id) {
  int session_id = g_sessions.Get().GetId(
      render_process_id_, render_view_id, request_id);
  if (session_id)
    manager()->StopRecording(session_id);
}

void InputTagSpeechDispatcherHost::SetRecognitionResult(
    int session_id, const content::SpeechRecognitionResult& result) {
  VLOG(1) << "InputTagSpeechDispatcherHost::SetRecognitionResult enter";
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  int session_render_view_id = g_sessions.Get().render_view_id(session_id);
  int session_request_id = g_sessions.Get().request_id(session_id);
  Send(new InputTagSpeechMsg_SetRecognitionResult(session_render_view_id,
                                                  session_request_id,
                                                  result));
  VLOG(1) << "InputTagSpeechDispatcherHost::SetRecognitionResult exit";
}

void InputTagSpeechDispatcherHost::DidCompleteRecording(int session_id) {
  VLOG(1) << "InputTagSpeechDispatcherHost::DidCompleteRecording enter";
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  int session_render_view_id = g_sessions.Get().render_view_id(session_id);
  int session_request_id = g_sessions.Get().request_id(session_id);
  Send(new InputTagSpeechMsg_RecordingComplete(session_render_view_id,
                                               session_request_id));
  VLOG(1) << "InputTagSpeechDispatcherHost::DidCompleteRecording exit";
}

void InputTagSpeechDispatcherHost::DidCompleteRecognition(int session_id) {
  VLOG(1) << "InputTagSpeechDispatcherHost::DidCompleteRecognition enter";
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  int session_render_view_id =
    g_sessions.Get().render_view_id(session_id);
  int session_request_id = g_sessions.Get().request_id(session_id);
  Send(new InputTagSpeechMsg_RecognitionComplete(session_render_view_id,
                                                 session_request_id));
  // Request sequence ended, so remove mapping.
  g_sessions.Get().RemoveId(session_id);
  VLOG(1) << "InputTagSpeechDispatcherHost::DidCompleteRecognition exit";
}

}  // namespace speech
