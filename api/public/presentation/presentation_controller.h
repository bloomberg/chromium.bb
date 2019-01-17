// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_PUBLIC_PRESENTATION_PRESENTATION_CONTROLLER_H_
#define API_PUBLIC_PRESENTATION_PRESENTATION_CONTROLLER_H_

#include <map>
#include <memory>
#include <string>

#include "api/public/presentation/presentation_connection.h"
#include "api/public/protocol_connection.h"
#include "api/public/service_listener.h"
#include "base/error.h"

namespace openscreen {
namespace presentation {

class UrlAvailabilityListener;

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
  // Only called for |service_id| values previously advertised as available.
  virtual void OnReceiverUnavailable(const std::string& presentation_url,
                                     const std::string& service_id) = 0;
};

class Controller final : public ServiceListener::Observer {
 public:
  // Returns the single instance.
  static Controller* Get();

  class ReceiverWatch {
   public:
    ReceiverWatch();
    ReceiverWatch(const std::vector<std::string>& urls,
                  ProtocolConnectionServiceObserver* observer);
    ReceiverWatch(ReceiverWatch&&);
    ~ReceiverWatch();

    ReceiverWatch& operator=(ReceiverWatch&&);

   private:
    std::vector<std::string> urls_;
    ProtocolConnectionServiceObserver* observer_ = nullptr;
  };

  class ConnectRequest {
   public:
    ConnectRequest();
    ConnectRequest(const std::string& service_id,
                   bool is_reconnect,
                   uint64_t request_id);
    ConnectRequest(ConnectRequest&&);
    ~ConnectRequest();

    ConnectRequest& operator=(ConnectRequest&&);

   private:
    std::string service_id_;
    bool is_reconnect_;
    uint64_t request_id_;
  };

  // TODO(issue/31): Remove singletons in the embedder API and protocol
  // implementation layers
  void Init();
  void Deinit();

  // Requests receivers compatible with all urls in |urls| and registers
  // |observer| for availability changes.  The screens will be a subset of the
  // screen list maintained by the ServiceListener.  Returns an RAII object that
  // tracks the registration.
  ReceiverWatch RegisterReceiverWatch(
      const std::vector<std::string>& urls,
      ProtocolConnectionServiceObserver* observer);

  // Requests that a new presentation be created on |service_id| using
  // |presentation_url|, with the result passed to |delegate|.
  // |conn_delegate| is passed to the resulting connection.  The returned
  // ConnectRequest object may be destroyed before any |delegate| methods are
  // called to cancel the request.
  ConnectRequest StartPresentation(const std::string& url,
                                   const std::string& service_id,
                                   RequestDelegate* delegate,
                                   Connection::Delegate* conn_delegate);

  // TODO(issue/31): Remove singletons in the embedder API and protocol
  // implementation layers. Esp. for any case where we need to segregate
  // information on the receiver for privacy reasons (e.g. receiver status only
  // shows title/description to original controlling context)?
  ConnectRequest ReconnectPresentation(const std::vector<std::string>& urls,
                                       const std::string& presentation_id,
                                       const std::string& service_id,
                                       RequestDelegate* delegate,
                                       Connection::Delegate* conn_delegate);

  // Called by the embedder to report that a presentation has been terminated.
  void OnPresentationTerminated(const std::string& presentation_id,
                                TerminationReason reason);

  std::string GetServiceIdForPresentationId(
      const std::string& presentation_id) const;
  ProtocolConnection* GetConnectionRequestGroupStream(
      const std::string& service_id);

  void SetConnectionRequestGroupStreamForTest(
      const std::string& service_id,
      std::unique_ptr<ProtocolConnection> stream);

  // TODO(btolsch): By endpoint.
  uint64_t GetNextRequestId();

  void OnConnectionDestroyed(Connection* connection);

 private:
  struct TerminateListener;
  struct InitiationRequest;
  struct ConnectionRequest;
  struct TerminationRequest;
  struct MessageGroupStreams;

  struct ControlledPresentation {
    std::string service_id;
    std::string url;
    std::vector<Connection*> connections;
  };

  static std::string MakePresentationId(const std::string& url,
                                        const std::string& service_id);

  Controller();
  ~Controller();

  uint64_t GetNextConnectionId(const std::string& id);

  void OpenConnection(uint64_t connection_id,
                      uint64_t endpoint_id,
                      const std::string& service_id,
                      RequestDelegate* request_delegate,
                      std::unique_ptr<Connection> connection,
                      std::unique_ptr<ProtocolConnection> stream);

  // Cancels compatible receiver monitoring for the given |urls|, |observer|
  // pair.
  void CancelReceiverWatch(const std::vector<std::string>& urls,
                           ProtocolConnectionServiceObserver* observer);

  // Cancels a presentation connect request for the given |request_id| if one is
  // pending.
  void CancelConnectRequest(const std::string& service_id,
                            bool is_reconnect,
                            uint64_t request_id);

  // ServiceListener::Observer overrides.
  void OnStarted() override;
  void OnStopped() override;
  void OnSuspended() override;
  void OnSearching() override;
  void OnReceiverAdded(const ServiceInfo& info) override;
  void OnReceiverChanged(const ServiceInfo& info) override;
  void OnReceiverRemoved(const ServiceInfo& info) override;
  void OnAllReceiversRemoved() override;
  void OnError(ServiceListenerError) override;
  void OnMetrics(ServiceListener::Metrics) override;

  uint64_t next_request_id_ = 1;
  std::map<std::string, uint64_t> next_connection_id_;

  std::map<std::string, ControlledPresentation> presentations_;

  std::unique_ptr<ConnectionManager> connection_manager_;

  std::unique_ptr<UrlAvailabilityListener> availability_listener_;
  std::map<std::string, IPEndpoint> receiver_endpoints_;

  std::map<std::string, std::unique_ptr<MessageGroupStreams>> group_streams_;
  std::map<std::string, std::unique_ptr<TerminateListener>>
      terminate_listeners_;
};

}  // namespace presentation
}  // namespace openscreen

#endif  // API_PUBLIC_PRESENTATION_PRESENTATION_CONTROLLER_H_
