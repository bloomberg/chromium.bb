// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_CAST_CHANNEL_CAST_CHANNEL_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_CAST_CHANNEL_CAST_CHANNEL_API_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/extensions/api/api_function.h"
#include "chrome/browser/extensions/api/api_resource_manager.h"
#include "chrome/browser/extensions/api/cast_channel/cast_socket.h"
#include "chrome/browser/extensions/api/profile_keyed_api_factory.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/common/extensions/api/cast_channel.h"

class Profile;

namespace extensions {

namespace cast_channel = api::cast_channel;

class CastChannelAPI : public ProfileKeyedAPI,
                       public cast_channel::CastSocket::Delegate {

 public:
  explicit CastChannelAPI(Profile* profile);

  static CastChannelAPI* Get(Profile* profile);

  // ProfileKeyedAPI implementation.
  static ProfileKeyedAPIFactory<CastChannelAPI>* GetFactoryInstance();

 private:
  virtual ~CastChannelAPI();
  friend class ProfileKeyedAPIFactory<CastChannelAPI>;

  // CastSocket::Delegate.  Called on IO thread.
  virtual void OnError(const cast_channel::CastSocket* socket,
                       cast_channel::ChannelError error) OVERRIDE;
  virtual void OnStringMessage(const cast_channel::CastSocket* socket,
                               const std::string& namespace_,
                               const std::string& data) OVERRIDE;

  // ProfileKeyedAPI implementation.
  static const char* service_name() {
    return "CastChannelAPI";
  }

  Profile* const profile_;

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

  ApiResourceManager<cast_channel::CastSocket>*
      GetSocketManager();

  // Returns the socket corresponding to |channel_id| if one exists.  If found,
  // sets |socket_| and |channel_id_|.
  cast_channel::CastSocket* GetSocket(long channel_id);

  // Sets the ChannelInfo result from the state of the CastSocket used by the
  // function.
  void SetResultFromSocket();

  // Sets the ChannelInfo result from |url| and |error|, when there is no
  // CastSocket associated with the function.
  void SetResultFromError(const std::string& url,
                          cast_channel::ChannelError error);

  // Destroys the CastSocket used by the function.
  void RemoveSocket();

  // Destroys the CastSocket used by the function, but only when an error
  // has occurred..
  void RemoveSocketIfError();

  // The socket being used by the function.  Set lazily.
  cast_channel::CastSocket* socket_;

  // The id of the socket being used by the function.  Set lazily.
  long channel_id_;

 private:
  void SetResultFromChannelInfo(
      const cast_channel::ChannelInfo& channel_info);

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

  void OnOpen(int result);

  scoped_ptr<cast_channel::Open::Params> params_;
  // Ptr to the API object.
  CastChannelAPI* api_;

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

#endif  // CHROME_BROWSER_EXTENSIONS_API_CAST_CHANNEL_CAST_CHANNEL_API_H_
