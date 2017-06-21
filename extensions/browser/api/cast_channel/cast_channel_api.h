// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_CAST_CHANNEL_CAST_CHANNEL_API_H_
#define EXTENSIONS_BROWSER_API_CAST_CHANNEL_CAST_CHANNEL_API_H_

#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "components/cast_channel/cast_channel_enum.h"
#include "components/cast_channel/cast_socket.h"
#include "extensions/browser/api/async_api_function.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/common/api/cast_channel.h"

class CastChannelAPITest;

namespace content {
class BrowserContext;
}

namespace net {
class IPEndPoint;
}

namespace cast_channel {
class CastSocketService;
}  // namespace cast_channel

namespace extensions {

struct Event;

class CastChannelAPI : public BrowserContextKeyedAPI,
                       public base::SupportsWeakPtr<CastChannelAPI> {
 public:
  explicit CastChannelAPI(content::BrowserContext* context);

  static CastChannelAPI* Get(content::BrowserContext* context);

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<CastChannelAPI>* GetFactoryInstance();

  // Returns a pointer to the Logger member variable.
  // TODO(imcheng): Consider whether it is possible for this class to own the
  // CastSockets and make this class the sole owner of Logger.
  // Alternatively,
  // consider making Logger not ref-counted by passing a weak
  // reference of Logger to the CastSockets instead.
  scoped_refptr<cast_channel::Logger> GetLogger();

  // Sets the CastSocket instance to be used for testing.
  void SetSocketForTest(
      std::unique_ptr<cast_channel::CastSocket> socket_for_test);

  // Returns a test CastSocket instance, if it is defined.
  // Otherwise returns a scoped_ptr with a nullptr value.
  std::unique_ptr<cast_channel::CastSocket> GetSocketForTest();

  // Returns the API browser context.
  content::BrowserContext* GetBrowserContext() const;

  // Sends an event to the extension's EventRouter, if it exists.
  void SendEvent(const std::string& extension_id, std::unique_ptr<Event> event);

 private:
  friend class BrowserContextKeyedAPIFactory<CastChannelAPI>;
  friend class ::CastChannelAPITest;
  friend class CastTransportDelegate;

  ~CastChannelAPI() override;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "CastChannelAPI"; }

  content::BrowserContext* const browser_context_;
  scoped_refptr<cast_channel::Logger> logger_;
  std::unique_ptr<cast_channel::CastSocket> socket_for_test_;

  DISALLOW_COPY_AND_ASSIGN(CastChannelAPI);
};

class CastChannelAsyncApiFunction : public AsyncApiFunction {
 public:
  CastChannelAsyncApiFunction();

 protected:
  ~CastChannelAsyncApiFunction() override;

  // AsyncApiFunction:
  bool PrePrepare() override;
  bool Respond() override;

  // Sets the function result to a ChannelInfo obtained from the state of
  // |socket|.
  void SetResultFromSocket(const cast_channel::CastSocket& socket);

  // Sets the function result to a ChannelInfo populated with |channel_id| and
  // |error|.
  void SetResultFromError(int channel_id,
                          api::cast_channel::ChannelError error);

  // Manages creating and removing Cast sockets.
  scoped_refptr<cast_channel::CastSocketService> cast_socket_service_;

 private:
  // Sets the function result from |channel_info|.
  void SetResultFromChannelInfo(
      const api::cast_channel::ChannelInfo& channel_info);
};

class CastChannelOpenFunction : public CastChannelAsyncApiFunction {
 public:
  CastChannelOpenFunction();

 protected:
  ~CastChannelOpenFunction() override;

  // AsyncApiFunction:
  bool PrePrepare() override;
  bool Prepare() override;
  void AsyncWorkStart() override;

 private:
  DECLARE_EXTENSION_FUNCTION("cast.channel.open", CAST_CHANNEL_OPEN)

  // Defines a callback used to send events to the extension's
  // EventRouter.
  //     Parameter #0 is a scoped pointer to the event payload.
  using EventDispatchCallback = base::Callback<void(std::unique_ptr<Event>)>;

  // Receives incoming messages and errors and provides additional API context.
  class CastMessageHandler : public cast_channel::CastSocket::Observer {
   public:
    CastMessageHandler(const EventDispatchCallback& ui_dispatch_cb,
                       scoped_refptr<cast_channel::Logger> logger);
    ~CastMessageHandler() override;

    // CastSocket::Observer implementation.
    void OnError(const cast_channel::CastSocket& socket,
                 cast_channel::ChannelError error_state) override;
    void OnMessage(const cast_channel::CastSocket& socket,
                   const cast_channel::CastMessage& message) override;

   private:
    // Callback for sending events to the extension.
    // Should be bound to a weak pointer, to prevent any use-after-free
    // conditions.
    EventDispatchCallback const ui_dispatch_cb_;
    // Logger object for reporting error details.
    scoped_refptr<cast_channel::Logger> logger_;

    DISALLOW_COPY_AND_ASSIGN(CastMessageHandler);
  };

  // Validates that |connect_info| represents a valid IP end point and returns a
  // new IPEndPoint if so.  Otherwise returns nullptr.
  static net::IPEndPoint* ParseConnectInfo(
      const api::cast_channel::ConnectInfo& connect_info);

  void OnOpen(cast_channel::ChannelError result);

  std::unique_ptr<api::cast_channel::Open::Params> params_;
  // The id of the newly opened socket.
  int new_channel_id_;
  CastChannelAPI* api_;
  std::unique_ptr<net::IPEndPoint> ip_endpoint_;
  base::TimeDelta liveness_timeout_;
  base::TimeDelta ping_interval_;

  FRIEND_TEST_ALL_PREFIXES(CastChannelOpenFunctionTest, TestParseConnectInfo);
  DISALLOW_COPY_AND_ASSIGN(CastChannelOpenFunction);
};

class CastChannelSendFunction : public CastChannelAsyncApiFunction {
 public:
  CastChannelSendFunction();

 protected:
  ~CastChannelSendFunction() override;

  // AsyncApiFunction:
  bool Prepare() override;
  void AsyncWorkStart() override;

 private:
  DECLARE_EXTENSION_FUNCTION("cast.channel.send", CAST_CHANNEL_SEND)

  void OnSend(int result);

  std::unique_ptr<api::cast_channel::Send::Params> params_;

  DISALLOW_COPY_AND_ASSIGN(CastChannelSendFunction);
};

class CastChannelCloseFunction : public CastChannelAsyncApiFunction {
 public:
  CastChannelCloseFunction();

 protected:
  ~CastChannelCloseFunction() override;

  // AsyncApiFunction:
  bool PrePrepare() override;
  bool Prepare() override;
  void AsyncWorkStart() override;

 private:
  DECLARE_EXTENSION_FUNCTION("cast.channel.close", CAST_CHANNEL_CLOSE)

  void OnClose(int result);

  std::unique_ptr<api::cast_channel::Close::Params> params_;
  CastChannelAPI* api_;

  DISALLOW_COPY_AND_ASSIGN(CastChannelCloseFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_CAST_CHANNEL_CAST_CHANNEL_API_H_
