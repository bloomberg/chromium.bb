// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Transport texture is a mechanism to share a texture between renderer process
// and GPU process. This is useful when a texture is used in the renderer
// process but updated in the GPU process.
//
// BACKGROUND INFORMATION
//
// Renderer process uses command buffer to submit GLES2 commands to the GPU
// process for execution. When using a texture in the renderer process it is
// supposed to update the texture, for example by using glTexImage2D().
//
// However for some cases the texture needs to be updated in the GPU process.
// Objects other than command buffer in the GPU process will then need to
// access the texture ID.
//
// This class provides the following functions that solve the problems:
// 1. Textures be requested in the GPU process. The request is submitted
//    to renderer process and textures are created in the command buffer
//    context and eventually in the system context.
// 2. Textures are then create and used in the renderer process.
// 3. The corresponding texture ID in the GPU process can be obtained.
// 4. When texture is updated in the GPU process, notification can be received
//    in the renderer process.
//
// THREAD SEMANTICS
//
// TransportTextureHost *must* be created on the render thread.
// After that methods of this object call be called on any thread.
//
// GLES2 commands are executed on Render Thread. IPC messages are sent and
// received on IO thread.
//
// USAGE
//
// --------------------------
// | In the renderer procss |
// --------------------------
//
// class TextureUpdateHandler {
//  public:
//   void OnTextureUpdate(int texture_id) {
//     // Do something with the texture.
//   }
// }
//
// TransportTextureHost factory_host;
// TextureUpdateHandler handler;
//
// void MyObjectInitDone() {
//   std::vector<int> textures;
//   factory_host.GetTextures(
//       NewCallback(&handler, &TextureUpdateHandler::OnTextureUpdate),
//       &textures);
// }
//
// void InitDone() {
//   InitMyObjectInGPUProcess(factory_host.GetPeerId(),
//                            NewRunnableFunction(&MyObjectInitDone));
// }
//
// factory_host.Init(NewRunnableFunction(&InitDone));
//
// ----------------------
// | In the GPU process |
// ----------------------
//
// // When the transport texture factory id is known.
// TransportTexture* factory = gpu_channel->GetTransportTexture(id);
//
// void TextureCreateDone(vector<int> textures) {
//   // Send message to renderer saying init is done.
//   SendInitDone();
//
//   UpdateTextureContent(textures[0]);
//   factory->TextureUpdated(textures[0]);
// }
//
// // When init is requested from renderer.
// vector<int> textures;
//
// void OnInit() {
//   factory->CreateTextures(3, 1024, 768, TransportTexture::RGB, &textures,
//                           NewRunnableFunction(&TextureCreateDone),
//                           textures);
// }

#ifndef CONTENT_RENDERER_TRANSPORT_TEXTURE_HOST_H_
#define CONTENT_RENDERER_TRANSPORT_TEXTURE_HOST_H_

#include <vector>

#include "base/basictypes.h"
#include "base/callback_old.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/task.h"
#include "ipc/ipc_channel.h"

class MessageLoop;
class RendererGLContext;
class TransportTextureService;

class TransportTextureHost
    : public base::RefCountedThreadSafe<TransportTextureHost>,
      public IPC::Channel::Listener {
 public:
  typedef Callback1<int>::Type TextureUpdateCallback;

  // |io_message_loop| is where the IPC communication should happen.
  // |render_message_loop| is where the GLES2 commands should be exeucted.
  // |service| contains the route to this object.
  // |sender| is used to send IPC messages to GPU process.
  // |context| is the RendererGLContextt for generating textures.
  // |context_route_id| is the route ID for the GpuChannelHost.
  // |host_id| is the ID of this object in GpuChannelHost.
  TransportTextureHost(MessageLoop* io_message_loop,
                       MessageLoop* render_message_loop,
                       TransportTextureService* service,
                       IPC::Message::Sender* sender,
                       RendererGLContext* context,
                       int32 context_route_id,
                       int32 host_id);
  virtual ~TransportTextureHost();

  // Initialize this object, this will cause a corresponding
  // TransportTexture be created in the GPU process.
  // |done_task| is called when initialization is completed.
  void Init(Task* done_task);

  // Destroy resources acquired by this object and in the GPU process.
  //
  // WARNING
  //
  // Call this method only after textures are not used in both the renderer
  // and GPU process.
  void Destroy();

  // Get the list of textures generated through this factory.
  // A callback must be provided to listen to texture update events. The
  // callback will be called on the IO thread.
  //
  // Note that this method doesn't generate any textures, it simply return
  // the list of textures generated.
  void GetTextures(TextureUpdateCallback* callback,
                   std::vector<int>* textures);

  // Return the peer ID of TransportTexture in the GPU process.
  int GetPeerId();

  // IPC::Channel::Listener.
  virtual void OnChannelConnected(int32 peer_pid);
  virtual void OnChannelError();
  virtual bool OnMessageReceived(const IPC::Message& message);

 private:
  // Released all textures generated.
  void ReleaseTexturesInternal();

  // Send the texture IDs to the GPU process. This will copy the set of
  // texture IDs.
  void SendTexturesInternal(std::vector<int> textures);

  // Send the destroy message to the GPU process.
  void SendDestroyInternal();

  ////////////////////////////////////////////////////////////////////////////
  // IPC Message Handlers
  void OnTransportTextureCreated(int32 peer_id);
  void OnCreateTextures(int32 n, uint32 width, uint32 height, int32 format);
  void OnReleaseTextures();
  void OnTextureUpdated(int texture_id);

  MessageLoop* io_message_loop_;
  MessageLoop* render_message_loop_;
  scoped_refptr<TransportTextureService> service_;

  IPC::Message::Sender* sender_;
  RendererGLContext* context_;
  int32 context_route_id_;
  int32 host_id_;
  int32 peer_id_;

  scoped_ptr<Task> init_task_;

  // A list of textures generated.
  std::vector<int> textures_;

  // Callback when a texture is updated.
  scoped_ptr<TextureUpdateCallback> update_callback_;

  DISALLOW_COPY_AND_ASSIGN(TransportTextureHost);
};

#endif  // CONTENT_RENDERER_TRANSPORT_TEXTURE_HOST_H_
