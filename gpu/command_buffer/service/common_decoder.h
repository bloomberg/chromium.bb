/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef GPU_COMMAND_BUFFER_SERVICE_COMMON_DECODER_H_
#define GPU_COMMAND_BUFFER_SERVICE_COMMON_DECODER_H_

#include <map>
#include "base/linked_ptr.h"
#include "base/scoped_ptr.h"
#include "gpu/command_buffer/service/cmd_parser.h"

namespace gpu {

class CommandBufferEngine;

// This class is a helper base class for implementing the common parts of the
// o3d/gl2 command buffer decoder.
class CommonDecoder : public AsyncAPIInterface {
 public:
  typedef parse_error::ParseError ParseError;

  // A bucket is a buffer to help collect memory across a command buffer. When
  // creating a command buffer implementation of an existing API, sometimes that
  // API has functions that take a pointer to data.  A good example is OpenGL's
  // glBufferData. Because the data is separated between client and service,
  // there are 2 ways to get this data across. 1 is to put all the data in
  // shared memory. The problem with this is the data can be arbitarily large
  // and the host OS may not support that much shared memory. Another solution
  // is to shuffle memory across a little bit at a time, collecting it on the
  // service side and when it is all there then call glBufferData. Buckets
  // implement this second solution. Using the common commands, SetBucketSize,
  // SetBucketData, SetBucketDataImmediate the client can fill a bucket. It can
  // then call a command that uses that bucket (like BufferDataBucket in the
  // GLES2 command buffer implementation).
  //
  // If you are designing an API from scratch you can avoid this need for
  // Buckets by making your API always take an offset and a size
  // similar to glBufferSubData.
  //
  // Buckets also help pass strings to/from the service. To return a string of
  // arbitary size, the service puts the string in a bucket. The client can
  // then query the size of a bucket and request sections of the bucket to
  // be passed across shared memory.
  class Bucket {
   public:
    Bucket() : size_(0) {
    }

    size_t size() const {
      return size_;
    }

    // Gets a pointer to a section the bucket. Returns NULL if offset or size is
    // out of range.
    const void* GetData(size_t offset, size_t size) const;

    template <typename T>
    T GetDataAs(size_t offset, size_t size) const {
      return reinterpret_cast<T>(GetData(offset, size));
    }

    // Sets the size of the bucket.
    void SetSize(size_t size);

    // Sets a part of the bucket.
    // Returns false if offset or size is out of range.
    bool SetData(const void* src, size_t offset, size_t size);

   private:
    bool OffsetSizeValid(size_t offset, size_t size) const {
      size_t temp = offset + size;
      return temp <= size_ && temp >= offset;
    }

    size_t size_;
    scoped_array<int8> data_;

    DISALLOW_COPY_AND_ASSIGN(Bucket);
  };

  CommonDecoder() : engine_(NULL) {
  }
  virtual ~CommonDecoder() {
  }

  // Sets the engine, to get shared memory buffers from, and to set the token
  // to.
  void set_engine(CommandBufferEngine* engine) {
    engine_ = engine;
  }

 protected:
  // Executes a common command.
  // Parameters:
  //    command: the command index.
  //    arg_count: the number of CommandBufferEntry arguments.
  //    cmd_data: the command data.
  // Returns:
  //   parse_error::NO_ERROR if no error was found, one of
  //   parse_error::ParseError otherwise.
  parse_error::ParseError DoCommonCommand(
      unsigned int command,
      unsigned int arg_count,
      const void* cmd_data);

  // Gets the address of shared memory data, given a shared memory ID and an
  // offset. Also checks that the size is consistent with the shared memory
  // size.
  // Parameters:
  //   shm_id: the id of the shared memory buffer.
  //   offset: the offset of the data in the shared memory buffer.
  //   size: the size of the data.
  // Returns:
  //   NULL if shm_id isn't a valid shared memory buffer ID or if the size
  //   check fails. Return a pointer to the data otherwise.
  void* GetAddressAndCheckSize(unsigned int shm_id,
                               unsigned int offset,
                               unsigned int size);

  // Typed version of GetAddressAndCheckSize.
  template <typename T>
  T GetSharedMemoryAs(unsigned int shm_id, unsigned int offset,
                      unsigned int size) {
    return static_cast<T>(GetAddressAndCheckSize(shm_id, offset, size));
  }

  // Gets an name for a common command.
  const char* GetCommonCommandName(cmd::CommandId command_id) const;

  // Gets a bucket. Returns NULL if the bucket does not exist.
  Bucket* GetBucket(uint32 bucket_id) const;

 private:
  // Generate a member function prototype for each command in an automated and
  // typesafe way.
  #define COMMON_COMMAND_BUFFER_CMD_OP(name)             \
     parse_error::ParseError Handle ## name(             \
       uint32 immediate_data_size,                       \
       const cmd::name& args);                           \

  COMMON_COMMAND_BUFFER_CMDS(COMMON_COMMAND_BUFFER_CMD_OP)

  #undef COMMON_COMMAND_BUFFER_CMD_OP

  CommandBufferEngine* engine_;

  typedef std::map<uint32, linked_ptr<Bucket> > BucketMap;
  BucketMap buckets_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_COMMON_DECODER_H_

