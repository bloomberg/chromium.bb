// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/display_source_custom_bindings.h"

#include <stdint.h>

#include "base/bind.h"
#include "content/public/child/v8_value_converter.h"
#include "extensions/renderer/script_context.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/web/WebDOMMediaStreamTrack.h"
#include "v8/include/v8.h"

namespace extensions {

using content::V8ValueConverter;

namespace {
const char kErrorNotSupported[] = "Not supported";
const char kInvalidStreamArgs[] = "Invalid stream arguments";
const char kSessionAlreadyStarted[] = "The session has been already started";
const char kSessionAlreadyTerminating[] = "The session is already terminating";
const char kSessionNotFound[] = "Session not found";
}  // namespace

DisplaySourceCustomBindings::DisplaySourceCustomBindings(ScriptContext* context)
    : ObjectBackedNativeHandler(context),
      weak_factory_(this) {
  RouteFunction("StartSession",
                base::Bind(&DisplaySourceCustomBindings::StartSession,
                           weak_factory_.GetWeakPtr()));
  RouteFunction("TerminateSession",
                base::Bind(&DisplaySourceCustomBindings::TerminateSession,
                           weak_factory_.GetWeakPtr()));
}

DisplaySourceCustomBindings::~DisplaySourceCustomBindings() {
}

void DisplaySourceCustomBindings::Invalidate() {
  session_map_.clear();
  weak_factory_.InvalidateWeakPtrs();
  ObjectBackedNativeHandler::Invalidate();
}

namespace {

v8::Local<v8::Value> GetChildValue(v8::Local<v8::Object> value,
                                   const std::string& key_name,
                                   v8::Isolate* isolate) {
  v8::Local<v8::Array> property_names(value->GetOwnPropertyNames());
  for (uint32_t i = 0; i < property_names->Length(); ++i) {
    v8::Local<v8::Value> key(property_names->Get(i));
    if (key_name == *v8::String::Utf8Value(key)) {
      v8::TryCatch try_catch(isolate);
      v8::Local<v8::Value> child_v8 = value->Get(key);
      if (try_catch.HasCaught()) {
        return v8::Null(isolate);
      }
      return child_v8;
    }
  }

  return v8::Null(isolate);
}

}  // namespace

void DisplaySourceCustomBindings::StartSession(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(2, args.Length());
  CHECK(args[0]->IsObject());
  CHECK(args[1]->IsFunction());

  v8::Isolate* isolate = context()->isolate();
  v8::Local<v8::Object> start_info = args[0].As<v8::Object>();

  v8::Local<v8::Value> sink_id_val =
      GetChildValue(start_info, "sinkId", isolate);
  CHECK(sink_id_val->IsInt32());
  const int sink_id = sink_id_val->ToInt32(isolate)->Value();
  if (GetDisplaySession(sink_id)) {
    isolate->ThrowException(v8::Exception::Error(v8::String::NewFromUtf8(
        isolate, kSessionAlreadyStarted)));
    return;
  }

  v8::Local<v8::Value> video_stream_val =
      GetChildValue(start_info, "videoTrack", isolate);
  v8::Local<v8::Value> audio_stream_val =
      GetChildValue(start_info, "audioTrack", isolate);

  if ((video_stream_val->IsNull() || video_stream_val->IsUndefined()) &&
      (audio_stream_val->IsNull() || audio_stream_val->IsUndefined())) {
    isolate->ThrowException(v8::Exception::Error(
        v8::String::NewFromUtf8(isolate, kInvalidStreamArgs)));
    return;
  }

  blink::WebMediaStreamTrack audio_track, video_track;

  if (!video_stream_val->IsNull() && !video_stream_val->IsUndefined()) {
    CHECK(video_stream_val->IsObject());
    video_track =
        blink::WebDOMMediaStreamTrack::fromV8Value(
            video_stream_val).component();
    if (video_track.isNull()) {
      isolate->ThrowException(v8::Exception::Error(
          v8::String::NewFromUtf8(isolate, kInvalidStreamArgs)));
      return;
    }
  }
  if (!audio_stream_val->IsNull() && !audio_stream_val->IsUndefined()) {
    CHECK(audio_stream_val->IsObject());
    audio_track =
        blink::WebDOMMediaStreamTrack::fromV8Value(
            audio_stream_val).component();
    if (audio_track.isNull()) {
      isolate->ThrowException(v8::Exception::Error(
          v8::String::NewFromUtf8(isolate, kInvalidStreamArgs)));
      return;
    }
  }

  scoped_ptr<DisplaySourceAuthInfo> auth_info;
  v8::Local<v8::Value> auth_info_v8_val =
      GetChildValue(start_info, "authenticationInfo", isolate);
  if (!auth_info_v8_val->IsNull()) {
    CHECK(auth_info_v8_val->IsObject());
    scoped_ptr<V8ValueConverter> converter(V8ValueConverter::create());
    scoped_ptr<base::Value> auth_info_val(
        converter->FromV8Value(auth_info_v8_val, context()->v8_context()));
    CHECK(auth_info_val);
    auth_info = DisplaySourceAuthInfo::FromValue(*auth_info_val);
  }

  DisplaySourceSessionParams session_params;
  session_params.sink_id = sink_id;
  session_params.video_track = video_track;
  session_params.audio_track = audio_track;
  session_params.render_frame = context()->GetRenderFrame();
  if (auth_info) {
    session_params.auth_method = auth_info->method;
    session_params.auth_data = auth_info->data ? *auth_info->data : "";
  }
  scoped_ptr<DisplaySourceSession> session =
      DisplaySourceSessionFactory::CreateSession(session_params);
  if (!session) {
    isolate->ThrowException(v8::Exception::Error(v8::String::NewFromUtf8(
        isolate, kErrorNotSupported)));
    return;
  }

  auto on_started_callback = base::Bind(
      &DisplaySourceCustomBindings::OnSessionStarted,
      weak_factory_.GetWeakPtr());
  auto on_terminated_callback = base::Bind(
      &DisplaySourceCustomBindings::OnSessionTerminated,
      weak_factory_.GetWeakPtr());
  auto on_error_callback = base::Bind(
      &DisplaySourceCustomBindings::OnSessionError,
      weak_factory_.GetWeakPtr());
  session->SetCallbacks(on_started_callback,
                        on_terminated_callback,
                        on_error_callback);

  CallbackInfo cb_info = GetCallbackInfo(kStarted, sink_id);
  args.GetReturnValue().Set(static_cast<int32_t>(cb_info.call_id));
  callbacks_.push_back(cb_info);

  session->Start();
  session_map_.insert(std::make_pair(sink_id, std::move(session)));
}

void DisplaySourceCustomBindings::TerminateSession(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(2, args.Length());
  CHECK(args[0]->IsInt32());
  CHECK(args[1]->IsFunction());

  v8::Isolate* isolate = context()->isolate();
  int sink_id = args[0]->ToInt32(args.GetIsolate())->Value();
  DisplaySourceSession* session = GetDisplaySession(sink_id);
  if (!session) {
    isolate->ThrowException(v8::Exception::Error(v8::String::NewFromUtf8(
        isolate, kSessionNotFound)));
    return;
  }

  if (session->state() == DisplaySourceSession::Terminating) {
    isolate->ThrowException(v8::Exception::Error(v8::String::NewFromUtf8(
        isolate, kSessionAlreadyTerminating)));
    return;
  }

  CallbackInfo cb_info = GetCallbackInfo(kTerminated, sink_id);
  args.GetReturnValue().Set(static_cast<int32_t>(cb_info.call_id));
  callbacks_.push_back(cb_info);

  // The session will get removed from session_map_ in OnSessionTerminated.
  session->Terminate();
}

void DisplaySourceCustomBindings::CallCompletionCallback(
    int sink_id,
    CallbackType type,
    const std::string& error_message) {
  auto predicate = [sink_id, type](const CallbackInfo& info) -> bool {
    return info.sink_id == sink_id && info.type == type;
  };
  auto it = std::find_if(callbacks_.begin(), callbacks_.end(), predicate);
  if (it == callbacks_.end())
    return;

  v8::Isolate* isolate = context()->isolate();
  ModuleSystem* module_system = context()->module_system();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(context()->v8_context());

  v8::Local<v8::Value> callback_args[2];
  callback_args[0] = v8::Integer::New(isolate, it->call_id);
  if (error_message.empty())
    callback_args[1] = v8::Null(isolate);
  else
    callback_args[1] = v8::String::NewFromUtf8(isolate, error_message.c_str());

  module_system->CallModuleMethod("displaySource", "callCompletionCallback", 2,
                                  callback_args);
}

void DisplaySourceCustomBindings::DispatchSessionTerminated(int sink_id) const {
  v8::Isolate* isolate = context()->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(context()->v8_context());
  v8::Local<v8::Array> event_args = v8::Array::New(isolate, 1);
  event_args->Set(0, v8::Integer::New(isolate, sink_id));
  context()->DispatchEvent("displaySource.onSessionTerminated", event_args);
}

void DisplaySourceCustomBindings::DispatchSessionError(
    int sink_id,
    DisplaySourceErrorType type,
    const std::string& message) const {
  v8::Isolate* isolate = context()->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(context()->v8_context());

  api::display_source::ErrorInfo error_info;
  error_info.type = type;
  if (!message.empty())
    error_info.description.reset(new std::string(message));

  scoped_ptr<V8ValueConverter> converter(V8ValueConverter::create());
  v8::Local<v8::Value> info_arg =
      converter->ToV8Value(error_info.ToValue().get(),
                           context()->v8_context());

  v8::Local<v8::Array> event_args = v8::Array::New(isolate, 2);
  event_args->Set(0, v8::Integer::New(isolate, sink_id));
  event_args->Set(1, info_arg);
  context()->DispatchEvent("displaySource.onSessionErrorOccured", event_args);
}

DisplaySourceSession* DisplaySourceCustomBindings::GetDisplaySession(
    int sink_id) const {
  auto iter = session_map_.find(sink_id);
  if (iter != session_map_.end())
    return iter->second.get();
  return nullptr;
}

void DisplaySourceCustomBindings::OnSessionStarted(int sink_id) {
  CallCompletionCallback(sink_id, kStarted);
}

void DisplaySourceCustomBindings::OnSessionTerminated(int sink_id) {
  DisplaySourceSession* session = GetDisplaySession(sink_id);
  CHECK(session);
  session_map_.erase(sink_id);
  DispatchSessionTerminated(sink_id);
  CallCompletionCallback(sink_id, kTerminated);
}

void DisplaySourceCustomBindings::OnSessionError(int sink_id,
                                                 DisplaySourceErrorType type,
                                                 const std::string& message) {
  DisplaySourceSession* session = GetDisplaySession(sink_id);
  CHECK(session);
  DispatchSessionError(sink_id, type, message);
}

DisplaySourceCustomBindings::CallbackInfo
DisplaySourceCustomBindings::GetCallbackInfo(
    DisplaySourceCustomBindings::CallbackType type,
    int sink_id) const {
  static int sCallId = 0;
  return {type, sink_id, ++sCallId};
}

}  // extensions
