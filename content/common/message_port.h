// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MESSAGE_PORT_H_
#define CONTENT_COMMON_MESSAGE_PORT_H_

#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace content {

// MessagePort corresponds to a HTML MessagePort. It is a thin wrapper around a
// Mojo MessagePipeHandle and provides methods for reading and writing messages.
//
// A MessagePort is only actively listening for incoming messages once
// SetCallback has been called with a valid callback. If ClearCallback is
// called (or if SetCallback is called with a null callback), then the
// MessagePort will stop listening for incoming messages. The callback runs on
// an unspecified background thread.
//
// Upon destruction, if the MessagePort is listening for incoming messages,
// then the destructor will first synchronize with the background thread,
// waiting for it to finish any in-process callback before closing the
// underlying MessagePipeHandle. This synchronization ensures that any code
// running in the callback can be sure to not worry about the MessagePort
// becoming invalid during callback execution.
//
// MessagePort methods may be used from any thread; however, care must be taken
// when using ReleaseHandle, ReleaseHandles or when destroying a MessagePort
// instance. The MessagePort class does not synchronize those methods with
// methods like PostMessage, GetMessage and SetCallback that use the underlying
// MessagePipeHandle.
//
// TODO(darin): Make this class move-only once no longer used with Chrome IPC.
//
class CONTENT_EXPORT MessagePort {
 public:
  ~MessagePort();
  MessagePort();

  // Shallow copy, resulting in multiple references to the same port.
  MessagePort(const MessagePort& other);
  MessagePort& operator=(const MessagePort& other);

  explicit MessagePort(mojo::ScopedMessagePipeHandle handle);

  const mojo::ScopedMessagePipeHandle& GetHandle() const;
  mojo::ScopedMessagePipeHandle ReleaseHandle() const;

  static std::vector<mojo::ScopedMessagePipeHandle> ReleaseHandles(
      const std::vector<MessagePort>& ports);

  // Sends an encoded message (along with ports to transfer) to this port's
  // peer.
  void PostMessage(const base::string16& encoded_message,
                   std::vector<MessagePort> ports);

  // Get the next available encoded message if any. Returns true if a message
  // was read.
  bool GetMessage(base::string16* encoded_message,
                  std::vector<MessagePort>* ports);

  // This callback will be invoked on a background thread when messages are
  // available to be read via GetMessage.
  void SetCallback(const base::Closure& callback);

  // Clears any callback specified by a prior call to SetCallback.
  void ClearCallback();

 private:
  class State : public base::RefCountedThreadSafe<State> {
   public:
    State();
    State(mojo::ScopedMessagePipeHandle handle);

    void AddWatch();
    void CancelWatch();
    static void OnHandleReady(uintptr_t context,
                              MojoResult result,
                              MojoHandleSignalsState signals_state,
                              MojoWatchNotificationFlags flags);

    mojo::ScopedMessagePipeHandle handle_;
    base::Closure callback_;

   private:
    friend class base::RefCountedThreadSafe<State>;
    ~State();
  };
  mutable scoped_refptr<State> state_;
};

}  // namespace content

#endif  // CONTENT_COMMON_MESSAGE_PORT_H_
