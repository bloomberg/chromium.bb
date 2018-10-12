// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_PUBLIC_PRESENTATION_PRESENTATION_COMMON_H_
#define API_PUBLIC_PRESENTATION_PRESENTATION_COMMON_H_

#include <cstdint>
#include <string>
#include <vector>

namespace openscreen {
namespace presentation {

// TODO(btolsch): Can any of these enums be included from code generated from
// CDDL?
enum class TerminationReason {
  kReceiverTerminateCalled = 0,
  kReceiverUserTerminated,
  kControllerTerminateCalled,
  kControllerUserTerminated,
  kReceiverPresentationReplaced,
  kReceiverIdleTooLong,
  kReceiverPresentationUnloaded,
  kReceiverShuttingDown,
  kReceiverError,
};

class Connection {
 public:
  enum class CloseReason {
    kClosed = 0,
    kDiscarded,
    kError,
  };

  enum class State {
    // The library is currently attempting to connect to the presentation.
    kConnecting,
    // The connection to the presentation is open and communication is possible.
    kConnected,
    // The connection is closed or could not be opened.  No communication is
    // possible but it may be possible to reopen the connection via
    // ReconnectPresentation.
    kClosed,
    // The connection is closed and the receiver has been terminated.
    kTerminated,
  };

  // An object to receive callbacks related to a single Connection.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // State changes.
    virtual void OnConnected() = 0;

    // Explicit close by other endpoint.
    virtual void OnClosedByRemote() = 0;

    // Closed because the script connection object was discarded.
    virtual void OnDiscarded() = 0;

    // Closed because of an error.
    virtual void OnError(const std::string& message) = 0;

    // Terminated through a different connection.
    virtual void OnTerminated() = 0;

    // TODO(btolsch): Use a string view class to minimize copies when we have a
    // string view or span implementation.

    // A UTF-8 string message was received.
    virtual void OnStringMessage(std::string message) = 0;

    // A binary message was received.
    virtual void OnBinaryMessage(std::vector<uint8_t> data) = 0;
  };

  struct Info {
    std::string id;
    std::string url;
  };

  // Constructs a new connection using |delegate| for callbacks.
  Connection(const std::string& id, const std::string& url, Delegate* delegate);

  // Returns the ID and URL of this presentation.
  const std::string& id() const { return id_; }
  const std::string& url() const { return url_; }
  State state() const { return state_; }

  // Sends a UTF-8 string message.
  void SendString(std::string&& message);

  // Sends a binary message.
  void SendBinary(std::vector<uint8_t>&& data);

  // Closes the connection.  This can be based on an explicit request from the
  // embedder or because the connection object is being discarded (page
  // navigated, object GC'd, etc.).
  void Close(CloseReason reason);

  // Terminates the presentation associated with this connection.
  // TODO(btolsch): Should this co-exist with the Controller/Receiver terminate
  // methods?
  void Terminate(TerminationReason reason);

 private:
  std::string id_;
  std::string url_;
  State state_;
  Delegate* delegate_;
};

}  // namespace presentation
}  // namespace openscreen

#endif  // API_PUBLIC_PRESENTATION_PRESENTATION_COMMON_H_
