// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// VideoCaptureController is the glue between a VideoCaptureDevice and all
// VideoCaptureHosts that have connected to it. A controller exists on behalf of
// one (and only one) VideoCaptureDevice; both are owned by the
// VideoCaptureManager.
//
// The VideoCaptureController is responsible for:
//
//   * Allocating and keeping track of shared memory buffers, and filling them
//     with I420 video frames for IPC communication between VideoCaptureHost (in
//     the browser process) and VideoCaptureMessageFilter (in the renderer
//     process).
//   * Conveying events from the device thread (where capture devices live) to
//     the IO thread (where the clients can be reached).
//   * Broadcasting the events from a single VideoCaptureDevice, fanning them
//     out to multiple clients.
//   * Keeping track of the clients on behalf of the VideoCaptureManager, making
//     it possible for the Manager to delete the Controller and its Device when
//     there are no clients left.
//   * Performing some image transformations on the output of the Device;
//     specifically, colorspace conversion and rotation.
//
// Interactions between VideoCaptureController and other classes:
//
//   * VideoCaptureController receives events from its VideoCaptureDevice
//     entirely through the VideoCaptureDevice::EventHandler interface, which
//     VCC implements.
//   * A VideoCaptureController interacts with its clients (VideoCaptureHosts)
//     via the VideoCaptureControllerEventHandler interface.
//   * Conversely, a VideoCaptureControllerEventHandler (typically,
//     VideoCaptureHost) will interact directly with VideoCaptureController to
//     return leased buffers by means of the ReturnBuffer() public method of
//     VCC.
//   * VideoCaptureManager (which owns the VCC) interacts directly with
//     VideoCaptureController through its public methods, to add and remove
//     clients.
//
// Thread safety:
//
//   The public methods implementing the VCD::EventHandler interface are safe to
//   call from any thread -- in practice, this is the thread on which the Device
//   is running. For this reason, it is RefCountedThreadSafe. ALL OTHER METHODS
//   are only safe to run on the IO browser thread.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_CONTROLLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_CONTROLLER_H_

#include <list>
#include <map>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/process/process.h"
#include "base/synchronization/lock.h"
#include "content/browser/renderer_host/media/video_capture_buffer_pool.h"
#include "content/browser/renderer_host/media/video_capture_controller_event_handler.h"
#include "content/common/content_export.h"
#include "content/common/media/video_capture.h"
#include "media/video/capture/video_capture.h"
#include "media/video/capture/video_capture_device.h"
#include "media/video/capture/video_capture_types.h"

namespace content {
class VideoCaptureBufferPool;

class CONTENT_EXPORT VideoCaptureController
    : public base::RefCountedThreadSafe<VideoCaptureController>,
      public media::VideoCaptureDevice::EventHandler {
 public:
  VideoCaptureController();

  // Start video capturing and try to use the resolution specified in
  // |params|.
  // When capturing starts, the |event_handler| will receive an OnFrameInfo()
  // call informing it of the resolution that was actually picked by the device.
  void AddClient(const VideoCaptureControllerID& id,
                 VideoCaptureControllerEventHandler* event_handler,
                 base::ProcessHandle render_process,
                 const media::VideoCaptureParams& params);

  // Stop video capture. This will take back all buffers held by by
  // |event_handler|, and |event_handler| shouldn't use those buffers any more.
  // Returns the session_id of the stopped client, or
  // kInvalidMediaCaptureSessionId if the indicated client was not registered.
  int RemoveClient(const VideoCaptureControllerID& id,
                   VideoCaptureControllerEventHandler* event_handler);

  int GetClientCount();

  // API called directly by VideoCaptureManager in case the device is
  // prematurely closed.
  void StopSession(int session_id);

  // Return a buffer previously given in
  // VideoCaptureControllerEventHandler::OnBufferReady.
  void ReturnBuffer(const VideoCaptureControllerID& id,
                    VideoCaptureControllerEventHandler* event_handler,
                    int buffer_id);

  // Implement media::VideoCaptureDevice::EventHandler.
  virtual scoped_refptr<media::VideoFrame> ReserveOutputBuffer() OVERRIDE;
  virtual void OnIncomingCapturedFrame(const uint8* data,
                                       int length,
                                       base::Time timestamp,
                                       int rotation,
                                       bool flip_vert,
                                       bool flip_horiz) OVERRIDE;
  virtual void OnIncomingCapturedVideoFrame(
      const scoped_refptr<media::VideoFrame>& frame,
      base::Time timestamp) OVERRIDE;
  virtual void OnError() OVERRIDE;
  virtual void OnFrameInfo(const media::VideoCaptureCapability& info) OVERRIDE;
  virtual void OnFrameInfoChanged(
      const media::VideoCaptureCapability& info) OVERRIDE;

 protected:
  virtual ~VideoCaptureController();

 private:
  friend class base::RefCountedThreadSafe<VideoCaptureController>;

  struct ControllerClient;
  typedef std::list<ControllerClient*> ControllerClients;

  // Worker functions on IO thread.
  void DoIncomingCapturedFrameOnIOThread(
      const scoped_refptr<media::VideoFrame>& captured_frame,
      base::Time timestamp);
  void DoFrameInfoOnIOThread();
  void DoFrameInfoChangedOnIOThread(const media::VideoCaptureCapability& info);
  void DoErrorOnIOThread();
  void DoDeviceStoppedOnIOThread();

  // Send frame info and init buffers to |client|.
  void SendFrameInfoAndBuffers(ControllerClient* client);

  // Find a client of |id| and |handler| in |clients|.
  ControllerClient* FindClient(
      const VideoCaptureControllerID& id,
      VideoCaptureControllerEventHandler* handler,
      const ControllerClients& clients);

  // Find a client of |session_id| in |clients|.
  ControllerClient* FindClient(
      int session_id,
      const ControllerClients& clients);

  // Protects access to the |buffer_pool_| pointer on non-IO threads.  IO thread
  // must hold this lock when modifying the |buffer_pool_| pointer itself.
  // TODO(nick): Make it so that this lock isn't required.
  base::Lock buffer_pool_lock_;

  // The pool of shared-memory buffers used for capturing.
  scoped_refptr<VideoCaptureBufferPool> buffer_pool_;

  // All clients served by this controller.
  ControllerClients controller_clients_;

  // The parameter that currently used for the capturing.
  media::VideoCaptureParams current_params_;

  // It's modified on caller thread, assuming there is only one OnFrameInfo()
  // call per StartCapture().
  media::VideoCaptureCapability frame_info_;

  // Chopped pixels in width/height in case video capture device has odd numbers
  // for width/height. Accessed only on the device thread.
  int chopped_width_;
  int chopped_height_;

  // It's accessed only on IO thread.
  bool frame_info_available_;

  // Takes on only the states 'STARTED' and 'ERROR'. 'ERROR' is an absorbing
  // state which stops the flow of data to clients. Accessed only on IO thread.
  VideoCaptureState state_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureController);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_CONTROLLER_H_
