// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/controllable_http_response.h"

#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"

namespace content {

class ControllableHttpResponse::Interceptor
    : public net::test_server::HttpResponse {
 public:
  explicit Interceptor(
      base::WeakPtr<ControllableHttpResponse> controller,
      scoped_refptr<base::SingleThreadTaskRunner> controller_task_runner)
      : controller_(controller),
        controller_task_runner_(controller_task_runner) {}
  ~Interceptor() override {}

 private:
  void SendResponse(
      const net::test_server::SendBytesCallback& send,
      const net::test_server::SendCompleteCallback& done) override {
    controller_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ControllableHttpResponse::OnRequest, controller_,
                       base::ThreadTaskRunnerHandle::Get(), send, done));
  }

  base::WeakPtr<ControllableHttpResponse> controller_;
  scoped_refptr<base::SingleThreadTaskRunner> controller_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(Interceptor);
};

ControllableHttpResponse::ControllableHttpResponse(
    net::test_server::EmbeddedTestServer* embedded_test_server,
    const std::string& relative_url)
    : weak_ptr_factory_(this) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!embedded_test_server->Started()) << "ControllableHttpResponse must "
                                              "be instanciated before starting "
                                              "the EmbeddedTestServer.";
  embedded_test_server->RegisterRequestHandler(
      base::Bind(RequestHandler, weak_ptr_factory_.GetWeakPtr(),
                 base::ThreadTaskRunnerHandle::Get(),
                 base::Owned(new bool(true)), relative_url));
}

ControllableHttpResponse::~ControllableHttpResponse() {}

void ControllableHttpResponse::WaitForRequest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(State::WAITING_FOR_REQUEST, state_)
      << "WaitForRequest() called twice.";
  loop_.Run();
  DCHECK(embedded_test_server_task_runner_);
  DCHECK(send_);
  DCHECK(done_);
  state_ = State::READY_TO_SEND_DATA;
}

void ControllableHttpResponse::Send(const std::string& bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(State::READY_TO_SEND_DATA, state_) << "Send() called without any "
                                                  "opened connection. Did you "
                                                  "call WaitForRequest()?";
  base::RunLoop loop;
  embedded_test_server_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(send_, bytes, loop.QuitClosure()));
  loop.Run();
}

void ControllableHttpResponse::Done() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(State::READY_TO_SEND_DATA, state_) << "Done() called without any "
                                                  "opened connection. Did you "
                                                  "call WaitForRequest()?";
  embedded_test_server_task_runner_->PostTask(FROM_HERE, done_);
  state_ = State::DONE;
}

void ControllableHttpResponse::OnRequest(
    scoped_refptr<base::SingleThreadTaskRunner>
        embedded_test_server_task_runner,
    const net::test_server::SendBytesCallback& send,
    const net::test_server::SendCompleteCallback& done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!embedded_test_server_task_runner_)
      << "A ControllableHttpResponse can only handle one request at a time";
  embedded_test_server_task_runner_ = embedded_test_server_task_runner;
  send_ = send;
  done_ = done;
  loop_.Quit();
}

// Helper function used in the ControllableHttpResponse constructor.
// static
std::unique_ptr<net::test_server::HttpResponse>
ControllableHttpResponse::RequestHandler(
    base::WeakPtr<ControllableHttpResponse> controller,
    scoped_refptr<base::SingleThreadTaskRunner> controller_task_runner,
    bool* available,
    const std::string& relative_url,
    const net::test_server::HttpRequest& request) {
  if (*available && request.relative_url == relative_url) {
    *available = false;
    return std::make_unique<ControllableHttpResponse::Interceptor>(
        controller, controller_task_runner);
  }
  return nullptr;
}

}  // namespace content
