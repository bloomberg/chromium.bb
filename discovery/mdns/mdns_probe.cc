// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/mdns/mdns_probe.h"

#include <utility>

#include "platform/api/task_runner.h"

namespace openscreen {
namespace discovery {

MdnsProbe::MdnsProbe(DomainName target_name, IPEndpoint endpoint)
    : target_name_(std::move(target_name)), endpoint_(std::move(endpoint)) {}

MdnsProbe::~MdnsProbe() = default;

MdnsProbeImpl::Observer::~Observer() = default;

MdnsProbeImpl::MdnsProbeImpl(MdnsSender* sender,
                             MdnsQuerier* querier,
                             MdnsRandom* random_delay,
                             TaskRunner* task_runner,
                             Observer* observer,
                             DomainName target_name,
                             IPEndpoint endpoint)
    : MdnsProbe(std::move(target_name), std::move(endpoint)),
      sender_(sender),
      querier_(querier),
      random_delay_(random_delay),
      task_runner_(task_runner),
      observer_(observer) {
  OSP_DCHECK(querier_);
  OSP_DCHECK(sender_);
  OSP_DCHECK(task_runner_);
  OSP_DCHECK(random_delay_);
  OSP_DCHECK(observer_);

  ProbeOnce();
}

MdnsProbeImpl::~MdnsProbeImpl() = default;

void MdnsProbeImpl::ProbeOnce() {
  // TODO(rwkeane): Implement this method.

  task_runner_->PostTask([this]() { observer_->OnProbeSuccess(this); });
}

void MdnsProbeImpl::Postpone(std::chrono::seconds delay) {
  // TODO(rwkeane): Implement this method.
}

}  // namespace discovery
}  // namespace openscreen
