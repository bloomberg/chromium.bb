// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/policy_header_io_helper.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/sequenced_task_runner.h"
#include "net/url_request/url_request.h"

namespace {

// The name of the header containing the policy information.
const char kChromePolicyHeader[] = "Chrome-Policy-Posture";

}  // namespace

namespace policy {

PolicyHeaderIOHelper::PolicyHeaderIOHelper(
    const std::string& server_url,
    const std::string& initial_header_value,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : server_url_(server_url),
      io_task_runner_(task_runner),
      policy_header_(initial_header_value) {
}

PolicyHeaderIOHelper::~PolicyHeaderIOHelper() {
}

// Sets any necessary policy headers on the passed request.
void PolicyHeaderIOHelper::AddPolicyHeaders(net::URLRequest* request) const {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  const GURL& url = request->url();
  if (!policy_header_.empty() &&
      url.spec().compare(0, server_url_.size(), server_url_) == 0) {
    request->SetExtraRequestHeaderByName(kChromePolicyHeader,
                                         policy_header_,
                                         true /* overwrite */);
  }
}

void PolicyHeaderIOHelper::UpdateHeader(const std::string& new_header) {
  // Post a task to the IO thread to modify this.
  io_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&PolicyHeaderIOHelper::UpdateHeaderOnIOThread,
                 base::Unretained(this), new_header));
}

void PolicyHeaderIOHelper::UpdateHeaderOnIOThread(
    const std::string& new_header) {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  policy_header_ = new_header;
}

}  // namespace policy
