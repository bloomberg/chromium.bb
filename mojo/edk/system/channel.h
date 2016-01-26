// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_CHANNEL_H_
#define MOJO_EDK_SYSTEM_CHANNEL_H_

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/task_runner.h"
#include "mojo/edk/embedder/platform_handle_vector.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"

namespace mojo {
namespace edk {

const size_t kChannelMessageAlignment = 8;

// Channel provides a thread-safe interface to read and write arbitrary
// delimited messages over an underlying I/O channel, optionally transferring
// one or more platform handles in the process.
class Channel : public base::RefCountedThreadSafe<Channel> {
 public:
  struct Message;

  using MessagePtr = scoped_ptr<Message>;

  // A message to be written to a channel.
  struct Message {
    struct Header {
      // Message size in bytes, including the header.
      uint32_t num_bytes;

      // Number of attached handles.
      uint16_t num_handles;

      // Zero
      uint16_t padding;
    };

    // Allocates and owns a buffer for message data with enough capacity for
    // |payload_size| bytes plus a header. Takes ownership of |handles|, which
    // may be null.
    Message(size_t payload_size, size_t num_handles);

    ~Message();

    // Constructs a Message from serialized message data.
    static MessagePtr Deserialize(const void* data, size_t data_num_bytes);

    const void* data() const { return data_; }
    size_t data_num_bytes() const { return size_; }

    void* mutable_payload() { return static_cast<void*>(header_ + 1); }
    const void* payload() const {
      return static_cast<const void*>(header_ + 1);
    }
    size_t payload_size() const;

    size_t num_handles() const { return header_->num_handles; }
    bool has_handles() const { return header_->num_handles > 0; }
    PlatformHandle* handles();

    // Note: SetHandles() and TakeHandles() invalidate any previous value of
    // handles().
    void SetHandles(ScopedPlatformHandleVectorPtr new_handles);
    ScopedPlatformHandleVectorPtr TakeHandles();

   private:
    size_t size_;
    char* data_;
    Header* header_;

#if defined(OS_WIN)
    // On Windows, handles are serialized in the data buffer along with the
    // rest of the payload.
    PlatformHandle* handles_ = nullptr;
#else
    ScopedPlatformHandleVectorPtr handle_vector_;
#endif

    DISALLOW_COPY_AND_ASSIGN(Message);
  };

  // Delegate methods are called from the I/O task runner with which the Channel
  // was created (see Channel::Create).
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Notify of a received message. |payload| is not owned and must not be
    // retained; it will be null if |payload_size| is 0. |handles| are
    // transferred to the callee.
    virtual void OnChannelMessage(const void* payload,
                                  size_t payload_size,
                                  ScopedPlatformHandleVectorPtr handles) = 0;

    // Notify that an error has occured and the Channel will cease operation.
    virtual void OnChannelError() = 0;
  };

  // Creates a new Channel around a |platform_handle|, taking ownership of the
  // handle. All I/O on the handle will be performed on |io_task_runner|.
  // Note that ShutDown() MUST be called on the Channel some time before
  // |delegate| is destroyed.
  static scoped_refptr<Channel> Create(
      Delegate* delegate,
      ScopedPlatformHandle platform_handle,
      scoped_refptr<base::TaskRunner> io_task_runner);

  // Request that the channel be shut down. This should always be called before
  // releasing the last reference to a Channel to ensure that it's cleaned up
  // on its I/O task runner's thread.
  //
  // Delegate methods will no longer be invoked after this call.
  void ShutDown();

  // Begin processing I/O events. Delegate methods must only be invoked after
  // this call.
  virtual void Start() = 0;

  // Stop processing I/O events.
  virtual void ShutDownImpl() = 0;

  // Queues an outgoing message on the Channel. This message will either
  // eventually be written or will fail to write and trigger
  // Delegate::OnChannelError.
  virtual void Write(MessagePtr message) = 0;

 protected:
  explicit Channel(Delegate* delegate);
  virtual ~Channel();

  // Called by the implementation when it wants somewhere to stick data.
  // |*buffer_capacity| may be set by the caller to indicate the desired buffer
  // size. If 0, a sane default size will be used instead.
  //
  // Returns the address of a buffer which can be written to, and indicates its
  // actual capacity in |*buffer_capacity|.
  char* GetReadBuffer(size_t* buffer_capacity);

  // Called by the implementation when new data is available in the read
  // buffer. Returns false to indicate an error. Upon success,
  // |*next_read_size_hint| will be set to a recommended size for the next
  // read done by the implementation.
  bool OnReadComplete(size_t bytes_read, size_t* next_read_size_hint);

  // Called by the implementation when something goes horribly wrong. It is NOT
  // OK to call this synchronously from any public interface methods.
  void OnError();

  // Retrieves the set of platform handles read for a given message. |payload|
  // and |payload_size| correspond to the full message body. Depending on
  // the Channel implementation, this body may encode platform handles, or
  // handles may be stored and managed elsewhere by the implementation.
  // If |num_handles| handles cannot be returned, this must return null.
  // The implementation may also adjust the values of |*payload| and/or
  // |*payload_size| to hide handle data from the user.
  virtual ScopedPlatformHandleVectorPtr GetReadPlatformHandles(
      size_t num_handles,
      void** payload,
      size_t* payload_size) = 0;

 private:
  friend class base::RefCountedThreadSafe<Channel>;

  class ReadBuffer;

  Delegate* delegate_;
  const scoped_ptr<ReadBuffer> read_buffer_;

  DISALLOW_COPY_AND_ASSIGN(Channel);
};

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_CHANNEL_H_
