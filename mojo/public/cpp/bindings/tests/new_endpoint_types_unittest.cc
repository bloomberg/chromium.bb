// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/interfaces/bindings/tests/new_endpoint_types.test-mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace new_endpoint_types {

class FactoryImpl;

class WidgetImpl : public mojom::Widget {
 public:
  WidgetImpl(FactoryImpl* factory,
             mojo::PendingReceiver<mojom::Widget> receiver,
             mojo::PendingRemote<mojom::WidgetClient> client)
      : factory_(factory),
        receiver_(this, std::move(receiver)),
        client_(std::move(client)) {
    client_.rpc(FROM_HERE)->OnInitialized();
    receiver_.set_disconnect_handler(
        base::BindOnce(&WidgetImpl::OnDisconnect, base::Unretained(this)));
  }

  ~WidgetImpl() override = default;

  // mojom::Widget:
  void Click() override {
    for (auto& observer : observers_)
      observer.rpc(FROM_HERE)->OnClick();
  }

  void AddObserver(
      mojo::PendingRemote<mojom::WidgetObserver> observer) override {
    observers_.emplace_back(std::move(observer));
  }

 private:
  void OnDisconnect();

  FactoryImpl* const factory_;
  mojo::Receiver<mojom::Widget> receiver_;
  mojo::Remote<mojom::WidgetClient> client_;
  std::vector<mojo::Remote<mojom::WidgetObserver>> observers_;

  DISALLOW_COPY_AND_ASSIGN(WidgetImpl);
};

class FactoryImpl : public mojom::WidgetFactory {
 public:
  explicit FactoryImpl(mojo::PendingReceiver<mojom::WidgetFactory> receiver)
      : receiver_(this, std::move(receiver)) {}
  ~FactoryImpl() override = default;

  // mojom::WidgetFactory:
  void CreateWidget(mojo::PendingReceiver<mojom::Widget> receiver,
                    mojo::PendingRemote<mojom::WidgetClient> client) override {
    widgets_.push_back(std::make_unique<WidgetImpl>(this, std::move(receiver),
                                                    std::move(client)));
  }

  void DestroyWidget(WidgetImpl* widget) {
    for (auto it = widgets_.begin(); it != widgets_.end(); ++it) {
      if (it->get() == widget) {
        widgets_.erase(it);
        return;
      }
    }
  }

 private:
  mojo::Receiver<mojom::WidgetFactory> receiver_;
  std::vector<std::unique_ptr<WidgetImpl>> widgets_;

  DISALLOW_COPY_AND_ASSIGN(FactoryImpl);
};

void WidgetImpl::OnDisconnect() {
  // Deletes |this|.
  factory_->DestroyWidget(this);
}

class ClientImpl : public mojom::WidgetClient {
 public:
  ClientImpl() = default;
  ~ClientImpl() override = default;

  mojo::PendingRemote<mojom::WidgetClient> BindNewRemote() {
    return receiver_.BindNewRemote();
  }

  void WaitForInitialize() { wait_loop_.Run(); }

  // mojom::WidgetClient:
  void OnInitialized() override { wait_loop_.Quit(); }

 private:
  mojo::Receiver<mojom::WidgetClient> receiver_{this};
  base::RunLoop wait_loop_;

  DISALLOW_COPY_AND_ASSIGN(ClientImpl);
};

class ObserverImpl : public mojom::WidgetObserver {
 public:
  ObserverImpl() = default;
  ~ObserverImpl() override = default;

  mojo::PendingRemote<mojom::WidgetObserver> BindNewRemote() {
    auto remote = receiver_.BindNewRemote();
    receiver_.set_disconnect_handler(
        base::BindOnce(&ObserverImpl::OnDisconnect, base::Unretained(this)));
    return remote;
  }

  void WaitForClick() { click_loop_.Run(); }
  void WaitForDisconnect() { disconnect_loop_.Run(); }

  // mojom::WidgetObserver:
  void OnClick() override { click_loop_.Quit(); }

 private:
  void OnDisconnect() { disconnect_loop_.Quit(); }

  mojo::Receiver<mojom::WidgetObserver> receiver_{this};
  base::RunLoop click_loop_;
  base::RunLoop disconnect_loop_;

  DISALLOW_COPY_AND_ASSIGN(ObserverImpl);
};

TEST(NewEndpointTypesTest, BasicUsage) {
  // A simple smoke/compile test for new bindings endpoint types. Used to
  // demonstrate look & feel as well as to ensure basic completeness and
  // correctness.

  base::test::ScopedTaskEnvironment task_environment;

  // A Remote<T> exposes a callable T interface which sends messages to a remote
  // implementation of T. Here we create a new unbound Remote which will control
  // a remote implementation of |mojom::WidgetFactory|.
  mojo::Remote<mojom::WidgetFactory> factory;
  EXPECT_FALSE(factory.is_bound());

  // |factory_impl| is a concrete implementation of |mojom::WidgetFactory|. With
  // Mojo interfaces, the implementation can live in the same process as the
  // Remote<T> calling it, or it can live in another process. For simplicity in
  // this test we have the implementation living in the test process.
  //
  // |BindNewReceiver()| creates a new message pipe to carry
  // |mojom:WidgetFactory| interface messages. It binds one end to the
  // |factory| Remote above, and the other end is passed to |factory_impl| so
  // it can receive messages.
  FactoryImpl factory_impl(factory.BindNewReceiver());
  EXPECT_TRUE(factory.is_bound());

  // Similar to above, we create another Remote. this time to control a
  // |mojom::Widget| implementation somewhere.
  mojo::Remote<mojom::Widget> widget;

  // |client| is an implementation of |mojom::WidgetClient|. This is a common
  // pattern for Mojo interfaces -- to have a Remote for some Foo interface
  // living alongside a corresponding implementation of a FooClient interface.
  // The pattern allows for two-way communication using separate but
  // closely-related types of endpoints.
  ClientImpl client;

  // Here we send two message pipes to the remote factory. This |CreateWidget|
  // call will be dispatched asynchronously to |factory_impl| via Mojo. Notice
  // that, inline, we create a new |mojom::Widget| pipe as well as a new
  // |mojom::WidgetClient| pipe. The Widget's Receiver endpoint is passed to
  // the factory implementation, as is the WidgetClient's Remote endpoint.
  // This allows the factory to bind and begin receiving Widget messages on
  // one pipe, and to bind and begin sending WidgetClient messages on the other.
  factory.rpc(FROM_HERE)->CreateWidget(widget.BindNewReceiver(),
                                       client.BindNewRemote());

  // Similar to |client| above, we create some implementations of
  // |mojom::WidgetObserver| here to receive messages from Remote
  // WidgetObserver caller on the factory implementation's side of the world.
  ObserverImpl observer1, observer2;

  // Similar to the |CreateWidget| call above, here we create new WidgetObserver
  // pipes (one for each impl object) and pass their Remote ends to the remote
  // Widget implementation to bind and use. This allows the remote Widget
  // implementation to send messages to both |observer1| and |observer2|.
  widget.rpc(FROM_HERE)->AddObserver(observer1.BindNewRemote());
  widget.rpc(FROM_HERE)->AddObserver(observer2.BindNewRemote());

  // When the FactoryImpl asynchronously receives our |CreateWidget| call, it
  // will send back a |mojom::WidgetClient::Initialize()| message to our
  // |client| object using the Remote passed to |CreateWidget|. This waits for
  // that message.
  client.WaitForInitialize();

  // Send another message, this time to the remote Widget implementation.
  widget.rpc(FROM_HERE)->Click();

  // When the remote Widget implementation receives a |Click()| message, it
  // broadcasts a |mojom::WidgetObserver::OnClick()| event to all registered
  // WidgetObservers on the Widget. We wait for each of our observers to
  // receive that message here.
  observer1.WaitForClick();
  observer2.WaitForClick();

  // Remotes (and Receivers, for that matter) remain bound until explicitly
  // unbound by their owner.
  widget.reset();
  EXPECT_FALSE(widget.is_bound());

  // Resetting the Remote<Widget> above eventually triggers the remote Widget
  // implementation's disconnection handler. That handler in turn tears down
  // the Widget implementation, including the Remote<WidgetObserver> endpoints
  // it owns. This in turn will eventually trigger our local WidgetObserver
  // instances' disconnection handlers. We wait for that to happen here.
  observer1.WaitForDisconnect();
  observer2.WaitForDisconnect();
}

}  // namespace new_endpoint_types
}  // namespace test
}  // namespace mojo
