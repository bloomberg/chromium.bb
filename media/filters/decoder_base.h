// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A base class that provides the plumbing for a decoder filters.

#ifndef MEDIA_FILTERS_DECODER_BASE_H_
#define MEDIA_FILTERS_DECODER_BASE_H_

#include <deque>

#include "base/callback.h"
#include "base/stl_util-inl.h"
#include "base/task.h"
#include "base/threading/thread.h"
#include "media/base/buffers.h"
#include "media/base/callback.h"
#include "media/base/filters.h"
#include "media/base/filter_host.h"

namespace media {

// In this class, we over-specify method lookup via this-> to avoid unexpected
// name resolution issues due to the two-phase lookup needed for dependent
// name resolution in templates.
template <class Decoder, class Output>
class DecoderBase : public Decoder {
 public:
  // Filter implementation.
  virtual void Stop(FilterCallback* callback) {
    message_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &DecoderBase::StopTask, callback));
  }

  virtual void Seek(base::TimeDelta time,
                    FilterCallback* callback) {
    message_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &DecoderBase::SeekTask, time, callback));
  }

  // Decoder implementation.
  virtual void Initialize(DemuxerStream* demuxer_stream,
                          FilterCallback* callback,
                          StatisticsCallback* stats_callback) {
    statistics_callback_.reset(stats_callback);
    message_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this,
                          &DecoderBase::InitializeTask,
                          make_scoped_refptr(demuxer_stream),
                          callback));
  }

  virtual const MediaFormat& media_format() { return media_format_; }

  // Audio decoder.
  // Note that this class is only used by the audio decoder, this will
  // eventually be merged into FFmpegAudioDecoder.
  virtual void ProduceAudioSamples(scoped_refptr<Output> output) {
    message_loop_->PostTask(FROM_HERE,
        NewRunnableMethod(this, &DecoderBase::ReadTask, output));
  }

 protected:
  explicit DecoderBase(MessageLoop* message_loop)
      : message_loop_(message_loop),
        pending_reads_(0),
        pending_requests_(0),
        state_(kUninitialized) {
  }

  virtual ~DecoderBase() {
    DCHECK(state_ == kUninitialized || state_ == kStopped);
    DCHECK(result_queue_.empty());
  }

  // This method is called by the derived class from within the OnDecode method.
  // It places an output buffer in the result queue.  It must be called from
  // within the OnDecode method.
  void EnqueueResult(Output* output) {
    DCHECK_EQ(MessageLoop::current(), message_loop_);
    if (!IsStopped()) {
      result_queue_.push_back(output);
    }
  }

  // TODO(ajwong): All these "Task*" used as completion callbacks should be
  // FilterCallbacks.  However, since NewCallback() cannot prebind parameters,
  // we use NewRunnableMethod() instead which causes an unfortunate refcount.
  // We should move stoyan's Mutant code into base/task.h and turn these
  // back into FilterCallbacks.

  // Method that must be implemented by the derived class.  Called from within
  // the DecoderBase::Initialize() method before any reads are submitted to
  // the demuxer stream.  Returns true if successful, otherwise false indicates
  // a fatal error.  The derived class should NOT call the filter host's
  // InitializationComplete() method.  If this method returns true, then the
  // base class will call the host to complete initialization.  During this
  // call, the derived class must fill in the media_format_ member.
  virtual void DoInitialize(DemuxerStream* demuxer_stream, bool* success,
                            Task* done_cb) = 0;

  // Method that may be implemented by the derived class if desired.  It will
  // be called from within the Filter::Stop() method prior to stopping the
  // base class.
  virtual void DoStop(Task* done_cb) {
    AutoTaskRunner done_runner(done_cb);
  }

  // Derived class can implement this method and perform seeking logic prior
  // to the base class.
  virtual void DoSeek(base::TimeDelta time, Task* done_cb) = 0;

  // Method that must be implemented by the derived class.  If the decode
  // operation produces one or more outputs, the derived class should call
  // the EnequeueResult() method from within this method.
  virtual void DoDecode(Buffer* input) = 0;

  MediaFormat media_format_;

  void OnDecodeComplete(const PipelineStatistics& statistics) {
    statistics_callback_->Run(statistics);

    // Attempt to fulfill a pending read callback and schedule additional reads
    // if necessary.
    bool fulfilled = FulfillPendingRead();

    // Issue reads as necessary.
    //
    // Note that it's possible for us to decode but not produce a frame, in
    // which case |pending_reads_| will remain less than |read_queue_| so we
    // need to schedule an additional read.
    DCHECK_LE(pending_reads_, pending_requests_);
    if (!fulfilled) {
      DCHECK_LT(pending_reads_, pending_requests_);
      demuxer_stream_->Read(NewCallback(this, &DecoderBase::OnReadComplete));
      ++pending_reads_;
    }
  }

  // Provide access to subclasses.
  MessageLoop* message_loop() { return message_loop_; }

 private:
  bool IsStopped() { return state_ == kStopped; }

  void OnReadComplete(Buffer* buffer) {
    // Little bit of magic here to get NewRunnableMethod() to generate a Task
    // that holds onto a reference via scoped_refptr<>.
    //
    // TODO(scherkus): change the callback format to pass a scoped_refptr<> or
    // better yet see if we can get away with not using reference counting.
    scoped_refptr<Buffer> buffer_ref = buffer;
    message_loop_->PostTask(FROM_HERE,
        NewRunnableMethod(this, &DecoderBase::ReadCompleteTask, buffer_ref));
  }

  void StopTask(FilterCallback* callback) {
    DCHECK_EQ(MessageLoop::current(), message_loop_);

    // Delegate to the subclass first.
    DoStop(NewRunnableMethod(this, &DecoderBase::OnStopComplete, callback));
  }

  void OnStopComplete(FilterCallback* callback) {
    // Throw away all buffers in all queues.
    result_queue_.clear();
    state_ = kStopped;

    if (callback) {
      callback->Run();
      delete callback;
    }
  }

  void SeekTask(base::TimeDelta time, FilterCallback* callback) {
    DCHECK_EQ(MessageLoop::current(), message_loop_);
    DCHECK_EQ(0u, pending_reads_) << "Pending reads should have completed";
    DCHECK_EQ(0u, pending_requests_) << "Pending requests should be empty";

    // Delegate to the subclass first.
    DoSeek(time,
           NewRunnableMethod(this, &DecoderBase::OnSeekComplete, callback));
  }

  void OnSeekComplete(FilterCallback* callback) {
    // Flush our decoded results.
    result_queue_.clear();

    // Signal that we're done seeking.
    if (callback) {
      callback->Run();
      delete callback;
    }
  }

  void InitializeTask(DemuxerStream* demuxer_stream,
                      FilterCallback* callback) {
    DCHECK_EQ(MessageLoop::current(), message_loop_);
    CHECK(kUninitialized == state_);
    CHECK(!demuxer_stream_);
    demuxer_stream_ = demuxer_stream;

    bool* success = new bool;
    DoInitialize(demuxer_stream,
                 success,
                 NewRunnableMethod(this, &DecoderBase::OnInitializeComplete,
                                   success, callback));
  }

  void OnInitializeComplete(bool* success, FilterCallback* done_cb) {
    // Note: The done_runner must be declared *last* to ensure proper
    // destruction order.
    scoped_ptr<bool> success_deleter(success);
    AutoCallbackRunner done_runner(done_cb);

    DCHECK_EQ(MessageLoop::current(), message_loop_);
    // Delegate to subclass first.
    if (!*success) {
      this->host()->SetError(PIPELINE_ERROR_DECODE);
    } else {
      state_ = kInitialized;
    }
  }

  void ReadTask(scoped_refptr<Output> output) {
    DCHECK_EQ(MessageLoop::current(), message_loop_);

    // TODO(scherkus): should reply with a null operation (empty buffer).
    if (IsStopped())
      return;

    ++pending_requests_;

    // Try to fulfill it immediately.
    if (FulfillPendingRead())
      return;

    // Since we can't fulfill a read request now then submit a read
    // request to the demuxer stream.
    demuxer_stream_->Read(NewCallback(this, &DecoderBase::OnReadComplete));
    ++pending_reads_;
  }

  void ReadCompleteTask(scoped_refptr<Buffer> buffer) {
    DCHECK_EQ(MessageLoop::current(), message_loop_);
    DCHECK_GT(pending_reads_, 0u);
    --pending_reads_;
    if (IsStopped()) {
      return;
    }

    // Decode the frame right away.
    DoDecode(buffer);
  }

  // Attempts to fulfill a single pending read by dequeuing a buffer and read
  // callback pair and executing the callback.
  //
  // Return true if one read request is fulfilled.
  bool FulfillPendingRead() {
    DCHECK_EQ(MessageLoop::current(), message_loop_);
    if (!pending_requests_ || result_queue_.empty()) {
      return false;
    }

    // Dequeue a frame and read callback pair.
    scoped_refptr<Output> output = result_queue_.front();
    result_queue_.pop_front();

    // Execute the callback!
    --pending_requests_;

    // TODO(hclam): We only inherit this class from FFmpegAudioDecoder so we
    // are making this call. We should correct this by merging this class into
    // FFmpegAudioDecoder.
    Decoder::consume_audio_samples_callback()->Run(output);
    return true;
  }

  MessageLoop* message_loop_;

  // Tracks the number of asynchronous reads issued to |demuxer_stream_|.
  // Using size_t since it is always compared against deque::size().
  size_t pending_reads_;
  // Tracks the number of asynchronous reads issued from renderer.
  size_t pending_requests_;

  // Pointer to the demuxer stream that will feed us compressed buffers.
  scoped_refptr<DemuxerStream> demuxer_stream_;

  // Queue of decoded samples produced in the OnDecode() method of the decoder.
  // Any samples placed in this queue will be assigned to the OutputQueue
  // buffers once the OnDecode() method returns.
  //
  // TODO(ralphl): Eventually we want to have decoders get their destination
  // buffer from the OutputQueue and write to it directly.  Until we change
  // from the Assignable buffer to callbacks and renderer-allocated buffers,
  // we need this extra queue.
  typedef std::deque<scoped_refptr<Output> > ResultQueue;
  ResultQueue result_queue_;

  // Simple state tracking variable.
  enum State {
    kUninitialized,
    kInitialized,
    kStopped,
  };
  State state_;

  // Callback to update pipeline statistics.
  scoped_ptr<StatisticsCallback> statistics_callback_;

  DISALLOW_COPY_AND_ASSIGN(DecoderBase);
};

}  // namespace media

#endif  // MEDIA_FILTERS_DECODER_BASE_H_
