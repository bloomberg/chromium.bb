// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/spy/spy.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/threading/worker_pool.h"

#include "mojo/public/cpp/system/core.h"
#include "mojo/service_manager/service_manager.h"

namespace {

const size_t kMessageBufSize = 2 * 1024;
const size_t kHandleBufSize = 64;

// In charge of processing messages that flow over a
// single message pipe.
class MessageProcessor :
    public base::RefCountedThreadSafe<MessageProcessor> {
 public:

  MessageProcessor()
      : last_result_(MOJO_RESULT_OK),
        bytes_transfered_(0) {

    message_count_[0] = 0;
    message_count_[1] = 0;
    handle_count_[0] = 0;
    handle_count_[1] = 0;
  }

  void Start(mojo::ScopedMessagePipeHandle client,
             mojo::ScopedMessagePipeHandle interceptor) {
    std::vector<mojo::MessagePipeHandle> pipes;
    pipes.push_back(client.get());
    pipes.push_back(interceptor.get());
    std::vector<MojoWaitFlags> wait_flags;
    wait_flags.push_back(MOJO_WAIT_FLAG_READABLE);
    wait_flags.push_back(MOJO_WAIT_FLAG_READABLE);

    scoped_ptr<char> mbuf(new char[kMessageBufSize]);
    scoped_ptr<MojoHandle> hbuf(new MojoHandle[kHandleBufSize]);

    // Main processing loop:
    // 1- Wait for an endpoint to have a message.
    // 2- Read the message
    // 3- Log data
    // 4- Wait until the opposite port is ready for writting
    // 4- Write the message to opposite port.

    for (;;) {
      int r = WaitMany(pipes, wait_flags, MOJO_DEADLINE_INDEFINITE);
      if ((r < 0) || (r > 1)) {
        last_result_ = r;
        break;
      }

      uint32_t bytes_read = kMessageBufSize;
      uint32_t handles_read = kHandleBufSize;

      if (!CheckResult(ReadMessageRaw(pipes[r],
                                      mbuf.get(), &bytes_read,
                                      hbuf.get(), &handles_read,
                                      MOJO_READ_MESSAGE_FLAG_NONE)))
        break;

      if (!bytes_read && !handles_read)
        continue;

      if (handles_read)
        handle_count_[r] += handles_read;

      ++message_count_[r];
      bytes_transfered_ += bytes_read;

      mojo::MessagePipeHandle write_handle = (r == 0) ? pipes[1] : pipes[0];
      if (!CheckResult(Wait(write_handle,
                            MOJO_WAIT_FLAG_WRITABLE,
                            MOJO_DEADLINE_INDEFINITE)))
        break;

      if (!CheckResult(WriteMessageRaw(write_handle,
                                       mbuf.get(), bytes_read,
                                       hbuf.get(), handles_read,
                                       MOJO_WRITE_MESSAGE_FLAG_NONE)))
        break;
    }

  }

 private:
   friend class base::RefCountedThreadSafe<MessageProcessor>;
   virtual ~MessageProcessor() {}

   bool CheckResult(MojoResult mr) {
     if (mr == MOJO_RESULT_OK)
       return true;
     last_result_ = mr;
     return false;
   }

   MojoResult last_result_;
   uint32_t bytes_transfered_;
   uint32_t message_count_[2];
   uint32_t handle_count_[2];
};

// In charge of intercepting access to the service manager.
class SpyInterceptor : public mojo::ServiceManager::Interceptor {
 private:
  virtual mojo::ScopedMessagePipeHandle OnConnectToClient(
    const GURL& url, mojo::ScopedMessagePipeHandle real_client) OVERRIDE {
      if (!MustIntercept(url))
        return real_client.Pass();

      // You can get an invalid handle if the app (or service) is
      // by unconventional means, for example the command line.
      if (!real_client.is_valid())
        return real_client.Pass();

      mojo::ScopedMessagePipeHandle faux_client;
      mojo::ScopedMessagePipeHandle interceptor;
      CreateMessagePipe(&faux_client, &interceptor);

      scoped_refptr<MessageProcessor> processor = new MessageProcessor();
      base::WorkerPool::PostTask(
          FROM_HERE,
          base::Bind(&MessageProcessor::Start,
                     processor,
                     base::Passed(&real_client), base::Passed(&interceptor)),
          true);

      return faux_client.Pass();
  }

  bool MustIntercept(const GURL& url) {
    // TODO(cpu): manage who and when to intercept.
    return true;
  }
};

}  // namespace

namespace mojo {

Spy::Spy(mojo::ServiceManager* service_manager, const std::string& options) {
  service_manager->SetInterceptor(new SpyInterceptor());
}

Spy::~Spy(){
  // TODO(cpu): Do not leak the interceptor. Lifetime between the
  // service_manager and the spy is still unclear hence the leak.
}

}  // namespace mojo
