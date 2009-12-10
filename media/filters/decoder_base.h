// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

// A base class that provides the plumbing for a decoder filters.

#ifndef MEDIA_FILTERS_DECODER_BASE_H_
#define MEDIA_FILTERS_DECODER_BASE_H_

#include <deque>

#include "base/lock.h"
#include "base/stl_util-inl.h"
#include "base/task.h"
#include "base/thread.h"
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
  typedef CallbackRunner< Tuple1<Output*> > ReadCallback;

  // MediaFilter implementation.
  virtual void Stop() {
    this->message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(this, &DecoderBase::StopTask));
  }

  virtual void Seek(base::TimeDelta time,
                    FilterCallback* callback) {
    this->message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(this, &DecoderBase::SeekTask, time, callback));
  }

  // Decoder implementation.
  virtual void Initialize(DemuxerStream* demuxer_stream,
                          FilterCallback* callback) {
    this->message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(this, &DecoderBase::InitializeTask, demuxer_stream,
                          callback));
  }

  virtual const MediaFormat& media_format() { return media_format_; }

  // Audio or video decoder.
  virtual void Read(ReadCallback* read_callback) {
    this->message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(this, &DecoderBase::ReadTask, read_callback));
  }

 protected:
  DecoderBase()
      : pending_reads_(0),
        expecting_discontinuous_(false),
        state_(kUninitialized) {
  }

  virtual ~DecoderBase() {
    DCHECK(state_ == kUninitialized || state_ == kStopped);
    DCHECK(result_queue_.empty());
    DCHECK(read_queue_.empty());
  }

  // This method is called by the derived class from within the OnDecode method.
  // It places an output buffer in the result queue.  It must be called from
  // within the OnDecode method.
  void EnqueueResult(Output* output) {
    DCHECK_EQ(MessageLoop::current(), this->message_loop());
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
  // be called from within the MediaFilter::Stop() method prior to stopping the
  // base class.
  //
  // TODO(ajwong): Make this asynchronous.
  virtual void DoStop() {}

  // Derived class can implement this method and perform seeking logic prior
  // to the base class.
  virtual void DoSeek(base::TimeDelta time, Task* done_cb) = 0;

  // Method that must be implemented by the derived class.  If the decode
  // operation produces one or more outputs, the derived class should call
  // the EnequeueResult() method from within this method.
  virtual void DoDecode(Buffer* input, Task* done_cb) = 0;

  MediaFormat media_format_;

 private:
  bool IsStopped() { return state_ == kStopped; }

  void OnReadComplete(Buffer* buffer) {
    // Little bit of magic here to get NewRunnableMethod() to generate a Task
    // that holds onto a reference via scoped_refptr<>.
    //
    // TODO(scherkus): change the callback format to pass a scoped_refptr<> or
    // better yet see if we can get away with not using reference counting.
    scoped_refptr<Buffer> buffer_ref = buffer;
    this->message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(this, &DecoderBase::ReadCompleteTask, buffer_ref));
  }

  void StopTask() {
    DCHECK_EQ(MessageLoop::current(), this->message_loop());

    // Delegate to the subclass first.
    DoStop();

    // Throw away all buffers in all queues.
    result_queue_.clear();
    STLDeleteElements(&read_queue_);
    state_ = kStopped;
  }

  void SeekTask(base::TimeDelta time, FilterCallback* callback) {
    DCHECK_EQ(MessageLoop::current(), this->message_loop());
    DCHECK_EQ(0u, pending_reads_) << "Pending reads should have completed";
    DCHECK(read_queue_.empty()) << "Read requests should be empty";

    // Delegate to the subclass first.
    //
    // TODO(scherkus): if we have the strong assertion that there are no pending
    // reads in the entire pipeline when we receive Seek(), subclasses could
    // either flush their buffers here or wait for IsDiscontinuous().  I'm
    // inclined to say that they should still wait for IsDiscontinuous() so they
    // don't have duplicated logic for Seek() and actual discontinuous frames.
    DoSeek(time,
           NewRunnableMethod(this, &DecoderBase::OnSeekComplete, callback));
  }

  void OnSeekComplete(FilterCallback* callback) {
    // Flush our decoded results.  We'll set a boolean that we can DCHECK to
    // verify our assertion that the first buffer received after a Seek() should
    // always be discontinuous.
    result_queue_.clear();
    expecting_discontinuous_ = true;

    // Signal that we're done seeking.
    callback->Run();
  }

  void InitializeTask(DemuxerStream* demuxer_stream, FilterCallback* callback) {
    DCHECK_EQ(MessageLoop::current(), this->message_loop());
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

    DCHECK_EQ(MessageLoop::current(), this->message_loop());
    // Delegate to subclass first.
    if (!*success) {
      this->host()->SetError(PIPELINE_ERROR_DECODE);
    } else {
      // TODO(scherkus): subclass shouldn't mutate superclass media format.
      DCHECK(!media_format_.empty()) << "Subclass did not set media_format_";
      state_ = kInitialized;
    }
  }

  void ReadTask(ReadCallback* read_callback) {
    DCHECK_EQ(MessageLoop::current(), this->message_loop());

    // TODO(scherkus): should reply with a null operation (empty buffer).
    if (IsStopped()) {
      delete read_callback;
      return;
    }

    // Enqueue the callback and attempt to fulfill it immediately.
    read_queue_.push_back(read_callback);
    FulfillPendingRead();

    // Issue reads as necessary.
    while (pending_reads_ < read_queue_.size()) {
      demuxer_stream_->Read(NewCallback(this, &DecoderBase::OnReadComplete));
      ++pending_reads_;
    }
  }

  void ReadCompleteTask(scoped_refptr<Buffer> buffer) {
    DCHECK_EQ(MessageLoop::current(), this->message_loop());
    DCHECK_GT(pending_reads_, 0u);
    --pending_reads_;
    if (IsStopped()) {
      return;
    }

    // TODO(scherkus): remove this when we're less paranoid about our seeking
    // invariants.
    if (buffer->IsDiscontinuous()) {
      DCHECK(expecting_discontinuous_);
      expecting_discontinuous_ = false;
    }

    // Decode the frame right away.
    DoDecode(buffer, NewRunnableMethod(this, &DecoderBase::OnDecodeComplete));
  }

  void OnDecodeComplete() {
    // Attempt to fulfill a pending read callback and schedule additional reads
    // if necessary.
    FulfillPendingRead();

    // Issue reads as necessary.
    //
    // Note that it's possible for us to decode but not produce a frame, in
    // which case |pending_reads_| will remain less than |read_queue_| so we
    // need to schedule an additional read.
    DCHECK_LE(pending_reads_, read_queue_.size());
    while (pending_reads_ < read_queue_.size()) {
      demuxer_stream_->Read(NewCallback(this, &DecoderBase::OnReadComplete));
      ++pending_reads_;
    }
  }

  // Attempts to fulfill a single pending read by dequeuing a buffer and read
  // callback pair and executing the callback.
  void FulfillPendingRead() {
    DCHECK_EQ(MessageLoop::current(), this->message_loop());
    if (read_queue_.empty() || result_queue_.empty()) {
      return;
    }

    // Dequeue a frame and read callback pair.
    scoped_refptr<Output> output = result_queue_.front();
    scoped_ptr<ReadCallback> read_callback(read_queue_.front());
    result_queue_.pop_front();
    read_queue_.pop_front();

    // Execute the callback!
    read_callback->Run(output);
  }

  // Tracks the number of asynchronous reads issued to |demuxer_stream_|.
  // Using size_t since it is always compared against deque::size().
  size_t pending_reads_;

  // A flag used for debugging that we expect our next read to be discontinuous.
  bool expecting_discontinuous_;

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

  // Queue of callbacks supplied by the renderer through the Read() method.
  typedef std::deque<ReadCallback*> ReadQueue;
  ReadQueue read_queue_;

  // Pause callback.
  scoped_ptr<FilterCallback> pause_callback_;

  // Simple state tracking variable.
  enum State {
    kUninitialized,
    kInitialized,
    kStopped,
  };
  State state_;

  DISALLOW_COPY_AND_ASSIGN(DecoderBase);
};

}  // namespace media

#endif  // MEDIA_FILTERS_DECODER_BASE_H_
