// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/request_sender.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "components/component_updater/component_updater_configurator.h"
#include "components/component_updater/component_updater_utils.h"
#include "net/url_request/url_fetcher.h"

namespace component_updater {

RequestSender::RequestSender(const Configurator& config) : config_(config) {
}

RequestSender::~RequestSender() {
}

void RequestSender::Send(const std::string& request_string,
                         const std::vector<GURL>& urls,
                         const RequestSenderCallback& request_sender_callback) {
  if (urls.empty()) {
    request_sender_callback.Run(NULL);
    return;
  }

  request_string_ = request_string;
  urls_ = urls;
  request_sender_callback_ = request_sender_callback;

  cur_url_ = urls_.begin();

  SendInternal();
}

void RequestSender::SendInternal() {
  DCHECK(cur_url_ != urls_.end());
  DCHECK(cur_url_->is_valid());

  url_fetcher_.reset(SendProtocolRequest(
      *cur_url_, request_string_, this, config_.RequestContext()));
}

void RequestSender::OnURLFetchComplete(const net::URLFetcher* source) {
  if (GetFetchError(*source) == 0) {
    request_sender_callback_.Run(source);
    return;
  }

  if (++cur_url_ != urls_.end() &&
      config_.GetSequencedTaskRunner()->PostTask(
          FROM_HERE,
          base::Bind(&RequestSender::SendInternal, base::Unretained(this)))) {
    return;
  }

  request_sender_callback_.Run(source);
}

}  // namespace component_updater
