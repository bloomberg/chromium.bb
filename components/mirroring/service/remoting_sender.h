// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRRORING_SERVICE_REMOTING_SENDER_H_
#define COMPONENTS_MIRRORING_SERVICE_REMOTING_SENDER_H_

#include <memory>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "media/cast/sender/frame_sender.h"
#include "media/mojo/mojom/remoting.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace base {
class TickClock;
}  // namespace base

namespace media {
class MojoDataPipeReader;
}  // namespace media

namespace mirroring {

// RTP sender for a single Cast Remoting RTP stream. The client calls Send() to
// instruct the sender to read from a Mojo data pipe and transmit the data using
// a CastTransport.
class COMPONENT_EXPORT(MIRRORING_SERVICE) RemotingSender final
    : public media::mojom::RemotingDataStreamSender,
      public media::cast::FrameSender {
 public:
  // |transport| is expected to outlive this class.
  RemotingSender(scoped_refptr<media::cast::CastEnvironment> cast_environment,
                 media::cast::CastTransport* transport,
                 const media::cast::FrameSenderConfig& config,
                 mojo::ScopedDataPipeConsumerHandle pipe,
                 mojo::PendingReceiver<media::mojom::RemotingDataStreamSender>
                     stream_sender,
                 base::OnceClosure error_callback);
  ~RemotingSender() override;

 private:
  // Friend class for unit tests.
  friend class RemotingSenderTest;

  // media::mojom::RemotingDataStreamSender implementation. SendFrame() will
  // push callbacks onto the back of the input queue, and these may or may not
  // be processed at a later time. It depends on whether the data pipe has data
  // available or the CastTransport can accept more frames. CancelInFlightData()
  // is processed immediately, and will cause all pending operations to discard
  // data when they are processed later.
  void SendFrame(uint32_t frame_size) override;
  void CancelInFlightData() override;

  // FrameSender override.
  int GetNumberOfFramesInEncoder() const override;
  base::TimeDelta GetInFlightMediaDuration() const override;
  void OnCancelSendingFrames() override;

  // Attempt to run next pending input task, popping the head of the input queue
  // as each task succeeds.
  void ProcessNextInputTask();

  // These are called via callbacks run from the input queue.
  // Consumes a frame of |size| from the associated Mojo data pipe.
  void ReadFrame(uint32_t size);
  // Sends out the frame to the receiver over network.
  void TrySendFrame();

  // Called when a frame is completely read/discarded from the data pipe.
  void OnFrameRead(bool success);

  // Called when an input task completes.
  void OnInputTaskComplete();

  void OnRemotingDataStreamError();

  SEQUENCE_CHECKER(sequence_checker_);

  const base::TickClock* clock_;

  // Callback that is run to notify when a fatal error occurs.
  base::OnceClosure error_callback_;

  std::unique_ptr<media::MojoDataPipeReader> data_pipe_reader_;

  // Mojo receiver for this instance. Implementation at the other end of the
  // message pipe uses the RemotingDataStreamSender remote to control when
  // this RemotingSender consumes from |pipe_|.
  mojo::Receiver<media::mojom::RemotingDataStreamSender> stream_sender_;

  // The next frame's payload data. Populated by call to OnFrameRead() when
  // reading succeeded.
  std::string next_frame_data_;

  // Queue of pending input operations. |input_queue_discards_remaining_|
  // indicates the number of operations where data should be discarded (due to
  // CancelInFlightData()).
  base::queue<base::RepeatingClosure> input_queue_;
  size_t input_queue_discards_remaining_;

  // Indicates whether the |data_pipe_reader_| is processing a reading request.
  bool is_reading_;

  // Set to true if the first frame has not yet been sent, or if a
  // CancelInFlightData() operation just completed. This causes TrySendFrame()
  // to mark the next frame as the start of a new sequence.
  bool flow_restart_pending_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<RemotingSender> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RemotingSender);
};

}  // namespace mirroring

#endif  // COMPONENTS_MIRRORING_SERVICE_REMOTING_SENDER_H_
