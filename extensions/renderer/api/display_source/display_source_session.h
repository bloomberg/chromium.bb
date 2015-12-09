// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_DISPLAY_SOURCE_DISPLAY_SOURCE_SESSION_H_
#define EXTENSIONS_RENDERER_API_DISPLAY_SOURCE_DISPLAY_SOURCE_SESSION_H_

#include "base/callback.h"
#include "base/macros.h"
#include "extensions/common/api/display_source.h"
#include "third_party/WebKit/public/web/WebDOMMediaStreamTrack.h"

namespace extensions {

using DisplaySourceAuthInfo = api::display_source::AuthenticationInfo;
using DisplaySourceErrorType = api::display_source::ErrorType;

// This class represents a generic display source session interface.
class DisplaySourceSession {
 public:
  using SinkIdCallback = base::Callback<void(int sink_id)>;
  using ErrorCallback =
      base::Callback<void(int sink_id,
                          DisplaySourceErrorType error_type,
                          const std::string& error_description)>;

  // State flow is ether:
  // 'Idle' -> 'Establishing' -> 'Established' -> 'Terminating' -> 'Idle'
  // (terminated by Terminate() call)
  //  or
  // 'Idle' -> 'Establishing' -> 'Established' -> 'Idle'
  // (terminated from sink device or due to an error)
  enum State {
    Idle,
    Establishing,
    Established,
    Terminating
  };

  virtual ~DisplaySourceSession();

  // Starts the session.
  // The session state should be set to 'Establishing' immediately after this
  // method is called.
  virtual void Start() = 0;

  // Terminates the session.
  // The session state should be set to 'Terminating' immediately after this
  // method is called.
  virtual void Terminate() = 0;

  State state() const { return state_; }

  // Sets the callbacks invoked to inform about the session's state changes.
  // It is required to set the callbacks before the session is started.
  // |started_callback| : Called when the session was actually started (state
  //                      should be set to 'Established')
  // |terminated_callback| : Called when the session was actually started (state
  //                         should be set to 'Idle')
  // |error_callback| : Called if a fatal error has occured and the session
  //                    either cannot be started (if was invoked in
  //                    'Establishing' state) or will be terminated soon for
  //                    emergency reasons (if was invoked in 'Established'
  //                    state).
  void SetCallbacks(const SinkIdCallback& started_callback,
                    const SinkIdCallback& terminated_callback,
                    const ErrorCallback& error_callback);

 protected:
  DisplaySourceSession();

  State state_;
  SinkIdCallback started_callback_;
  SinkIdCallback terminated_callback_;
  ErrorCallback error_callback_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DisplaySourceSession);
};

class DisplaySourceSessionFactory {
 public:
  static scoped_ptr<DisplaySourceSession> CreateSession(
      int sink_id,
      const blink::WebMediaStreamTrack& video_track,
      const blink::WebMediaStreamTrack& audio_track,
      scoped_ptr<DisplaySourceAuthInfo> auth_info);
 private:
  DISALLOW_COPY_AND_ASSIGN(DisplaySourceSessionFactory);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_DISPLAY_SOURCE_DISPLAY_SOURCE_SESSION_H_
