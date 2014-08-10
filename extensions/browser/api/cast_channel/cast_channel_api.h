// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_CAST_CHANNEL_CAST_CHANNEL_API_H_
#define EXTENSIONS_BROWSER_API_CAST_CHANNEL_CAST_CHANNEL_API_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/browser/api/async_api_function.h"
#include "extensions/browser/api/cast_channel/cast_socket.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/common/api/cast_channel.h"

class GURL;
class CastChannelAPITest;

namespace content {
class BrowserContext;
}

namespace net {
class IPEndPoint;
}

namespace extensions {

namespace core_api {
namespace cast_channel {
class Logger;
}  // namespace cast_channel
}  // namespace core_api

namespace cast_channel = core_api::cast_channel;

class CastChannelAPI : public BrowserContextKeyedAPI,
                       public cast_channel::CastSocket::Delegate {
 public:
  explicit CastChannelAPI(content::BrowserContext* context);

  static CastChannelAPI* Get(content::BrowserContext* context);

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<CastChannelAPI>* GetFactoryInstance();

  // Returns a new CastSocket that connects to |ip_endpoint| with authentication
  // |channel_auth| and is to be owned by |extension_id|.
  scoped_ptr<cast_channel::CastSocket> CreateCastSocket(
      const std::string& extension_id,
      const net::IPEndPoint& ip_endpoint,
      cast_channel::ChannelAuthType channel_auth,
      const base::TimeDelta& timeout);

  // Returns a pointer to the Logger member variable.
  // TODO(imcheng): Consider whether it is possible for this class to own the
  // CastSockets and make this class the sole owner of Logger. Alternatively,
  // consider making Logger not ref-counted by passing a weak
  // reference of Logger to the CastSockets instead.
  scoped_refptr<cast_channel::Logger> GetLogger();

  // Sets the CastSocket instance to be returned by CreateCastSocket for
  // testing.
  void SetSocketForTest(scoped_ptr<cast_channel::CastSocket> socket_for_test);

 private:
  friend class BrowserContextKeyedAPIFactory<CastChannelAPI>;
  friend class ::CastChannelAPITest;

  virtual ~CastChannelAPI();

  // CastSocket::Delegate.  Called on IO thread.
  virtual void OnError(const cast_channel::CastSocket* socket,
                       cast_channel::ChannelError error) OVERRIDE;
  virtual void OnMessage(const cast_channel::CastSocket* socket,
                         const cast_channel::MessageInfo& message) OVERRIDE;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "CastChannelAPI"; }

  content::BrowserContext* const browser_context_;
  scoped_refptr<cast_channel::Logger> logger_;
  scoped_ptr<cast_channel::CastSocket> socket_for_test_;

  DISALLOW_COPY_AND_ASSIGN(CastChannelAPI);
};

class CastChannelAsyncApiFunction : public AsyncApiFunction {
 public:
  CastChannelAsyncApiFunction();

 protected:
  virtual ~CastChannelAsyncApiFunction();

  // AsyncApiFunction:
  virtual bool PrePrepare() OVERRIDE;
  virtual bool Respond() OVERRIDE;

  // Returns the socket corresponding to |channel_id| if one exists.  Otherwise,
  // sets the function result with CHANNEL_ERROR_INVALID_CHANNEL_ID, completes
  // the function, and returns null.
  cast_channel::CastSocket* GetSocketOrCompleteWithError(int channel_id);

  // Adds |socket| to |manager_| and returns the new channel_id.  |manager_|
  // assumes ownership of |socket|.
  int AddSocket(cast_channel::CastSocket* socket);

  // Removes the CastSocket corresponding to |channel_id| from the resource
  // manager.
  void RemoveSocket(int channel_id);

  // Sets the function result to a ChannelInfo obtained from the state of
  // |socket|.
  void SetResultFromSocket(const cast_channel::CastSocket& socket);

  // Sets the function result to a ChannelInfo populated with |channel_id| and
  // |error|.
  void SetResultFromError(int channel_id, cast_channel::ChannelError error);

  // Returns the socket corresponding to |channel_id| if one exists, or null
  // otherwise.
  cast_channel::CastSocket* GetSocket(int channel_id);

 private:
  // Sets the function result from |channel_info|.
  void SetResultFromChannelInfo(const cast_channel::ChannelInfo& channel_info);

  // The API resource manager for CastSockets.
  ApiResourceManager<cast_channel::CastSocket>* manager_;

  // The result of the function.
  cast_channel::ChannelError error_;
};

class CastChannelOpenFunction : public CastChannelAsyncApiFunction {
 public:
  CastChannelOpenFunction();

 protected:
  virtual ~CastChannelOpenFunction();

  // AsyncApiFunction:
  virtual bool PrePrepare() OVERRIDE;
  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;

 private:
  DECLARE_EXTENSION_FUNCTION("cast.channel.open", CAST_CHANNEL_OPEN)

  // Parses the cast:// or casts:// |url|, fills |connect_info| with the
  // corresponding details, and returns true. Returns false if |url| is not a
  // valid Cast URL.
  static bool ParseChannelUrl(const GURL& url,
                              cast_channel::ConnectInfo* connect_info);

  // Validates that |connect_info| represents a valid IP end point and returns a
  // new IPEndPoint if so.  Otherwise returns NULL.
  static net::IPEndPoint* ParseConnectInfo(
      const cast_channel::ConnectInfo& connect_info);

  void OnOpen(int result);

  scoped_ptr<cast_channel::Open::Params> params_;
  // The id of the newly opened socket.
  int new_channel_id_;
  CastChannelAPI* api_;
  scoped_ptr<cast_channel::ConnectInfo> connect_info_;
  scoped_ptr<net::IPEndPoint> ip_endpoint_;
  cast_channel::ChannelAuthType channel_auth_;

  FRIEND_TEST_ALL_PREFIXES(CastChannelOpenFunctionTest, TestParseChannelUrl);
  FRIEND_TEST_ALL_PREFIXES(CastChannelOpenFunctionTest, TestParseConnectInfo);
  DISALLOW_COPY_AND_ASSIGN(CastChannelOpenFunction);
};

class CastChannelSendFunction : public CastChannelAsyncApiFunction {
 public:
  CastChannelSendFunction();

 protected:
  virtual ~CastChannelSendFunction();

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;

 private:
  DECLARE_EXTENSION_FUNCTION("cast.channel.send", CAST_CHANNEL_SEND)

  void OnSend(int result);

  scoped_ptr<cast_channel::Send::Params> params_;

  DISALLOW_COPY_AND_ASSIGN(CastChannelSendFunction);
};

class CastChannelCloseFunction : public CastChannelAsyncApiFunction {
 public:
  CastChannelCloseFunction();

 protected:
  virtual ~CastChannelCloseFunction();

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;

 private:
  DECLARE_EXTENSION_FUNCTION("cast.channel.close", CAST_CHANNEL_CLOSE)

  void OnClose(int result);

  scoped_ptr<cast_channel::Close::Params> params_;

  DISALLOW_COPY_AND_ASSIGN(CastChannelCloseFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_CAST_CHANNEL_CAST_CHANNEL_API_H_
