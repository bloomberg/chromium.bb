// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_MEDIA_CAST_SESSION_H_
#define CHROME_RENDERER_MEDIA_CAST_SESSION_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/ip_endpoint.h"

namespace base {
class MessageLoopProxy;
}  // namespace base

namespace media {
class VideoFrame;
namespace cast {
class FrameInput;
struct AudioSenderConfig;
struct VideoSenderConfig;
}  // namespace cast
}  // namespace media

namespace content{
class P2PSocketClient;
}  // namespace content

class CastSessionDelegate;

// This class represents a Cast session and allows the session to be
// configured on the main thread. Actual work is forwarded to
// CastSessionDelegate on the IO thread.
class CastSession : public base::RefCounted<CastSession> {
 public:
  typedef
  base::Callback<void(const scoped_refptr<media::cast::FrameInput>&)>
  FrameInputAvailableCallback;

  CastSession();

  // Start encoding of audio and video using the provided configuration.
  //
  // When Cast sender is started and ready to be used
  // media::cast::FrameInput will be given through the callback. The
  // callback will be made on the main thread.
  void StartAudio(const media::cast::AudioSenderConfig& config,
                  const FrameInputAvailableCallback& callback);
  void StartVideo(const media::cast::VideoSenderConfig& config,
                  const FrameInputAvailableCallback& callback);

  class P2PSocketFactory {
   public:
    virtual ~P2PSocketFactory();

    // Called on IO thread.
    virtual scoped_refptr<content::P2PSocketClient> Create() = 0;
  };

  // Send the socket factory to the delegate, where create will be
  // called. The delegate will then delete the socket factory on the
  // IO thread. We do it this way because the P2P socket needs to
  // be created on the same thread that the callbacks will be called on.
  // The |remote_endpoint| is the address will be used when calling
  // SendWithDscp on the P2PSocketClient.
  // Takes ownership of socket_factory.
  void SetSocketFactory(scoped_ptr<P2PSocketFactory> socket_factory,
                        const net::IPEndPoint& remote_address);

 private:
  friend class base::RefCounted<CastSession>;
  virtual ~CastSession();

  // This member should never be dereferenced on the main thread.
  // CastSessionDelegate lives only on the IO thread. It is always
  // safe to post task on the IO thread to access CastSessionDelegate
  // because it is owned by this object.
  scoped_ptr<CastSessionDelegate> delegate_;

  // Proxy to the IO message loop.
  const scoped_refptr<base::MessageLoopProxy> io_message_loop_proxy_;

  DISALLOW_COPY_AND_ASSIGN(CastSession);
};

#endif  // CHROME_RENDERER_MEDIA_CAST_SESSION_H_
