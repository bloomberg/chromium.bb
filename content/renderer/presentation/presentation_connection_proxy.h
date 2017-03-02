// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PRESENTATION_PRESENTATION_CONNECTION_PROXY_H_
#define CONTENT_RENDERER_PRESENTATION_PRESENTATION_CONNECTION_PROXY_H_

#include "base/callback.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationConnectionProxy.h"
#include "third_party/WebKit/public/platform/modules/presentation/presentation.mojom.h"

namespace blink {
class WebPresentationConnection;
}  // namespace blink

namespace content {

// This class connects PresentationConnection owned by one frame with
// PresentationConnection owned by a different frame.
//
// PresentationConnectionProxy's ctor sets |source_connection_| to
// PresentationConnection object owned by one frame.
//
// To connect |controller_connection_proxy| on controlling frame with
// |receiver_connection_proxy| on receiver frame:
// 1. On controlling frame, create PresentationConnection object and
//    ControllerConnectionProxy object |controller_connection_proxy|;
// 2. Create |target_connection_request|, an interface request of
//    controller proxy's |target_connection_| by invoking:
//       ControllerConnectionProxy::MakeRemoteRequest() {
//           return mojo::MakeRequest(&target_connection_);
//       }
// 3. Pass |controller_connection_proxy|'s interface pointer and
//    |target_connection_request| to browser;
// 4. Browser passes |controller_connection_proxy|'s interface pointer and
//    |target_connection_request| to receiver frame;
// 5. On receiver frame, create PresentationConnection object and
//    ReceiverConnectionProxy object |receiver_connection_proxy|;
// 6. Bind |target_connection_request| with |receiver_connection_proxy| by
//    invoking:
//        ReceiverConnectionProxy::Bind(target_connection_request);
// 7. Set receiver proxy's |target_connection_| to
//    |controller_connection_proxy|'s interface pointer:
//        ReceiverConnectionProxy::BindTargetConnection(
//            blink::mojom::PresentationConnectionPtr connection) {
//          target_connection_ = std::move(connection);
//          ...
//        }
//
// When both |source_connection_| and |target_connection_| are set,
// we can send message or notify state changes between controller and receiver.
//
// To send message from controlling frame to receiver frame:
// 1. Controlling frame invokes connection.sendMessage();
// 2. This call is delegated to controller presentation connection proxy's
//    SendConnectionMessage().
//      ControllerConnectionProxy::SendConnectionMessage() {
//         target_connection_->OnMessage();
//      }
// 3. ReceiverConnectionProxy::OnMessage() is invoked.
// 4. connection.onmessage event on receiver frame is fired.
//
// Sending message from receiver frame to controlling frame and notifying state
// changes works the same.
//
// Instance of this class is created for both offscreen and non offscreen
// presentations.
class CONTENT_EXPORT PresentationConnectionProxy
    : public NON_EXPORTED_BASE(blink::WebPresentationConnectionProxy),
      public NON_EXPORTED_BASE(blink::mojom::PresentationConnection) {
 public:
  using OnMessageCallback = base::Callback<void(bool)>;

  ~PresentationConnectionProxy() override;

  virtual void SendConnectionMessage(
      blink::mojom::ConnectionMessagePtr connection_message,
      const OnMessageCallback& callback) const;

  // blink::mojom::PresentationConnection implementation
  void OnMessage(blink::mojom::ConnectionMessagePtr message,
                 const OnMessageCallback& callback) override;
  void DidChangeState(content::PresentationConnectionState state) override;
  void OnClose() override;

  // blink::WebPresentationConnectionProxy implementation.
  void close() const override;

 protected:
  explicit PresentationConnectionProxy(
      blink::WebPresentationConnection* source_connection);
  mojo::Binding<blink::mojom::PresentationConnection> binding_;
  mojo::InterfacePtr<blink::mojom::PresentationConnection>
      target_connection_ptr_;

 private:
  // Raw pointer to Blink connection object owning this proxy object. Does not
  // take ownership.
  blink::WebPresentationConnection* const source_connection_;
};

// Represents PresentationConnectionProxy object on controlling frame.
class CONTENT_EXPORT ControllerConnectionProxy
    : public PresentationConnectionProxy {
 public:
  explicit ControllerConnectionProxy(
      blink::WebPresentationConnection* controller_connection);
  ~ControllerConnectionProxy() override;

  blink::mojom::PresentationConnectionPtr Bind();
  blink::mojom::PresentationConnectionRequest MakeRemoteRequest();
};

// Represents PresentationConnectionProxy object on receiver frame.
class CONTENT_EXPORT ReceiverConnectionProxy
    : public PresentationConnectionProxy {
 public:
  explicit ReceiverConnectionProxy(
      blink::WebPresentationConnection* receiver_connection);
  ~ReceiverConnectionProxy() override;

  void Bind(
      blink::mojom::PresentationConnectionRequest receiver_connection_request);

  // Sets |target_connection_ptr_| to |controller_connection_ptr|. Should be
  // called only once.
  void BindControllerConnection(
      blink::mojom::PresentationConnectionPtr controller_connection_ptr);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PRESENTATION_PRESENTATION_CONNECTION_PROXY_H_
