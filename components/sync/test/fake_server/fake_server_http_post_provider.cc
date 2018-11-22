// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/fake_server/fake_server_http_post_provider.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/time/time.h"
#include "components/sync/test/fake_server/fake_server.h"
#include "net/base/net_errors.h"

using syncer::HttpPostProviderInterface;

namespace fake_server {

// static
bool FakeServerHttpPostProvider::network_enabled_ = true;

namespace {

void RunAndSignal(base::OnceClosure task,
                  base::WaitableEvent* completion_event) {
  std::move(task).Run();
  completion_event->Signal();
}

}  // namespace

FakeServerHttpPostProviderFactory::FakeServerHttpPostProviderFactory(
    const base::WeakPtr<FakeServer>& fake_server,
    scoped_refptr<base::SequencedTaskRunner> fake_server_task_runner)
    : fake_server_(fake_server),
      fake_server_task_runner_(fake_server_task_runner) {}

FakeServerHttpPostProviderFactory::~FakeServerHttpPostProviderFactory() {}

void FakeServerHttpPostProviderFactory::Init(
    const std::string& user_agent,
    const syncer::BindToTrackerCallback& bind_to_tracker_callback) {}

HttpPostProviderInterface* FakeServerHttpPostProviderFactory::Create() {
  FakeServerHttpPostProvider* http =
      new FakeServerHttpPostProvider(fake_server_, fake_server_task_runner_);
  http->AddRef();
  return http;
}

void FakeServerHttpPostProviderFactory::Destroy(
    HttpPostProviderInterface* http) {
  static_cast<FakeServerHttpPostProvider*>(http)->Release();
}

FakeServerHttpPostProvider::FakeServerHttpPostProvider(
    const base::WeakPtr<FakeServer>& fake_server,
    scoped_refptr<base::SequencedTaskRunner> fake_server_task_runner)
    : fake_server_(fake_server),
      fake_server_task_runner_(fake_server_task_runner) {}

FakeServerHttpPostProvider::~FakeServerHttpPostProvider() {}

void FakeServerHttpPostProvider::SetExtraRequestHeaders(const char* headers) {
  // TODO(pvalenzuela): Add assertions on this value.
  extra_request_headers_.assign(headers);
}

void FakeServerHttpPostProvider::SetURL(const char* url, int port) {
  // TODO(pvalenzuela): Add assertions on these values.
  request_url_.assign(url);
  request_port_ = port;
}

void FakeServerHttpPostProvider::SetPostPayload(const char* content_type,
                                                int content_length,
                                                const char* content) {
  request_content_type_.assign(content_type);
  request_content_.assign(content, content_length);
}

bool FakeServerHttpPostProvider::MakeSynchronousPost(int* net_error_code,
                                                     int* http_response_code) {
  if (!network_enabled_) {
    response_.clear();
    *net_error_code = net::ERR_INTERNET_DISCONNECTED;
    *http_response_code = 0;
    return false;
  }

  // It is assumed that a POST is being made to /command.
  int post_response_code = -1;
  std::string post_response;

  base::WaitableEvent post_complete(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  bool result = fake_server_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&RunAndSignal,
                     base::BindOnce(&FakeServer::HandleCommand, fake_server_,
                                    base::ConstRef(request_content_),
                                    &post_response_code, &post_response),
                     base::Unretained(&post_complete)));

  if (!result) {
    response_.clear();
    *net_error_code = net::ERR_UNEXPECTED;
    *http_response_code = 0;
    return false;
  }

  // Note: This is a potential deadlock. Here we're on the sync thread, and
  // we're waiting for something to happen on the UI thread (where the
  // FakeServer lives). If at the same time, ProfileSyncService is trying to
  // Stop() the sync thread, we're deadlocked. For a lack of better ideas, let's
  // just give up after a few seconds.
  // TODO(crbug.com/869404): Maybe the FakeServer should live on its own thread.
  if (!post_complete.TimedWait(base::TimeDelta::FromSeconds(5))) {
    *net_error_code = net::ERR_TIMED_OUT;
    return false;
  }

  // Zero means success.
  *net_error_code = 0;
  *http_response_code = post_response_code;
  response_ = post_response;

  return true;
}

int FakeServerHttpPostProvider::GetResponseContentLength() const {
  return response_.length();
}

const char* FakeServerHttpPostProvider::GetResponseContent() const {
  return response_.c_str();
}

const std::string FakeServerHttpPostProvider::GetResponseHeaderValue(
    const std::string& name) const {
  return std::string();
}

void FakeServerHttpPostProvider::Abort() {}

void FakeServerHttpPostProvider::DisableNetwork() {
  network_enabled_ = false;
}

void FakeServerHttpPostProvider::EnableNetwork() {
  network_enabled_ = true;
}

}  // namespace fake_server
