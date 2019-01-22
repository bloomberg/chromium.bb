// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_PUBLIC_PRESENTATION_PRESENTATION_CONTROLLER_H_
#define API_PUBLIC_PRESENTATION_PRESENTATION_CONTROLLER_H_

#include <memory>
#include <string>

#include "api/public/presentation/presentation_common.h"
#include "base/error.h"

namespace openscreen {
namespace presentation {

class RequestDelegate {
 public:
  virtual ~RequestDelegate() = default;

  virtual void OnConnection(std::unique_ptr<Connection> connection) = 0;
  virtual void OnError(const Error& error) = 0;
};

class ReceiverObserver {
 public:
  virtual ~ReceiverObserver() = default;

  // Called when there is an unrecoverable error in requesting availability.
  // This means the availability is unknown and there is no further response to
  // wait for.
  virtual void OnRequestFailed(const std::string& presentation_url,
                               const std::string& service_id) = 0;

  // Called when receivers compatible with |presentation_url| are known to be
  // available.
  virtual void OnReceiverAvailable(const std::string& presentation_url,
                                   const std::string& service_id) = 0;
  // Called when receivers compatible with |presentation_url| are known to be
  // unavailable.
  virtual void OnReceiverUnavailable(const std::string& presentation_url,
                                     const std::string& service_id) = 0;
};

class Controller {
 public:
  // Returns the single instance.
  static Controller* Get();

  class ReceiverWatch {
   public:
    explicit ReceiverWatch(uint64_t watch_id);
    ReceiverWatch(ReceiverWatch&&);
    ~ReceiverWatch();

    ReceiverWatch& operator=(ReceiverWatch&&);

   private:
    uint64_t watch_id_;
  };

  class ConnectRequest {
   public:
    explicit ConnectRequest(uint64_t request_id);
    ConnectRequest(ConnectRequest&&);
    ~ConnectRequest();

    ConnectRequest& operator=(ConnectRequest&&);

   private:
    uint64_t request_id_;
  };

  // Requests receivers compatible with |url| and registers |observer| for
  // availability changes.  The receivers will be a subset of the receiver list
  // maintained by the ServiceListener.  Returns a positive integer id that
  // tracks the registration. If |url| is already being watched for receivers,
  // then the id of the previous registration is returned and |observer|
  // replaces the previous registration.
  ReceiverWatch RegisterReceiverWatch(const std::string& url,
                                      ReceiverObserver* observer);

  // Requests that a new presentation be created on |service_id| using
  // |presentation_url|, with the result passed to |delegate|.
  // |conn_delegate| is passed to the resulting connection.  The returned
  // ConnectRequest object may be destroyed before any |delegate| methods are
  // called to cancel the request.
  ConnectRequest StartPresentation(const std::string& url,
                                   const std::string& service_id,
                                   RequestDelegate* delegate,
                                   Connection::Delegate* conn_delegate);

  // Requests reconnection to the presentation with the given id and URL running
  // on the receiver with |service_id|, with the result passed to |delegate|.
  // |conn_delegate| is passed to the resulting connection.
  // The returned ConnectRequest object may be destroyed before any |delegate|
  // methods are called to cancel the request.
  ConnectRequest ReconnectPresentation(const std::string& presentation_id,
                                       const std::string& service_id,
                                       RequestDelegate* delegate,
                                       Connection::Delegate* conn_delegate);

  // Called by the embedder to report that a presentation has been terminated.
  void OnPresentationTerminated(const std::string& presentation_id,
                                TerminationReason reason);

 private:
  // Cancels compatible receiver monitoring for the given |watch_id|.
  void CancelReceiverWatch(uint64_t watch_id);

  // Cancels a presentation connect request for the given |request_id| if one is
  // pending.
  void CancelConnectRequest(uint64_t request_id);
};

}  // namespace presentation
}  // namespace openscreen

#endif  // API_PUBLIC_PRESENTATION_PRESENTATION_CONTROLLER_H_
