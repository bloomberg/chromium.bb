// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_PUBLIC_PRESENTATION_PRESENTATION_RECEIVER_H_
#define API_PUBLIC_PRESENTATION_PRESENTATION_RECEIVER_H_

#include <memory>
#include <string>

#include "api/public/presentation/presentation_common.h"

namespace openscreen {
namespace presentation {

enum class ResponseResult {
  kSuccess = 0,
  kInvalidUrl,
  kRequestTimedOut,
  kRequestFailedTransient,
  kRequestFailedPermanent,
  kHttpError,
  kUnknown,
};

class ReceiverDelegate {
 public:
  virtual ~ReceiverDelegate() = default;

  // Called when a new presentation is requested by a controller.  This should
  // return true if the presentation was accepted, false otherwise.
  virtual bool StartPresentation(const Connection::Info& info,
                                 const std::string& http_headers) = 0;

  // Called when a presentation is requested to be terminated by a controller.
  virtual void TerminatePresentation(const Connection::Info& info,
                                     TerminationReason reason) = 0;

  // Called when a new connection is being requested of the receiver.  The
  // delegate should return a suitable delegate object for the new connection if
  // it accepts the connection and nullptr if it does not.  If it returns a
  // valid pointer, OnConnection will then be called with the new
  // PresentationConnection object.
  // TODO(btolsch): This would be simpler if the embedder could simply construct
  // a PresentationConnection itself and just return a bool here.  It's not
  // clear whether that's a good idea yet though, so revisit this after the
  // PresentationConnection code lands.
  virtual Connection::Delegate* CreateConnectionDelegate(
      const Connection::Info& info) = 0;

  // Called when a new connection to the receiver is created, where the delegate
  // came from OnConnectionAvailable.
  virtual void OnConnectionCreated(std::unique_ptr<Connection> connection) = 0;
};

class Receiver {
 public:
  // Returns the single instance.
  static Receiver* Get();

  // Sets the object to call when a new receiver connection is available.
  // |delegate| must either outlive PresentationReceiver or live until a new
  // delegate (possibly nullptr) is set.  Setting the delegate to nullptr will
  // automatically reject all future receiver requests.
  void SetReceiverDelegate(ReceiverDelegate* delegate);

  // Called by the embedder to report its response to OnPresentationRequested.
  void OnPresentationStarted(const std::string& presentation_id,
                             ResponseResult result);

  // Called by the embedder to report that a presentation has been terminated.
  void OnPresentationTerminated(const std::string& presentation_id,
                                TerminationReason reason);
};

}  // namespace presentation
}  // namespace openscreen

#endif  // API_PUBLIC_PRESENTATION_PRESENTATION_RECEIVER_H_
