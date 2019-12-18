// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/mdns/mdns_probe_manager.h"

#include <utility>

#include "platform/api/task_runner.h"

namespace openscreen {
namespace discovery {

MdnsProbeManager::Callback::~Callback() = default;

MdnsProbeManager::MdnsProbeManager(MdnsSender* sender,
                                   MdnsQuerier* querier,
                                   MdnsRandom* random_delay,
                                   TaskRunner* task_runner)
    : sender_(sender),
      querier_(querier),
      random_delay_(random_delay),
      task_runner_(task_runner) {
  OSP_DCHECK(sender_);
  OSP_DCHECK(querier_);
  OSP_DCHECK(task_runner_);
  OSP_DCHECK(random_delay_);
}

MdnsProbeManager::~MdnsProbeManager() = default;

Error MdnsProbeManager::StartProbe(Callback* callback,
                                   DomainName requested_name,
                                   IPEndpoint endpoint) {
  // TODO(rwkeane): Ensure the requested name isn't already being queried for.

  auto probe = CreateProbe(requested_name, std::move(endpoint));
  ongoing_probes_.emplace_back(std::move(probe), std::move(requested_name),
                               callback);
  return Error::None();
}

Error MdnsProbeManager::StopProbe(const DomainName& requested_name) {
  // TODO(rwkeane): Implement this method.
  return Error::None();
}

bool MdnsProbeManager::IsDomainClaimed(const DomainName& domain) const {
  // TODO(rwkeane): Implement this method.
  return true;
}

void MdnsProbeManager::RespondToProbeQuery(const MdnsQuestion& message,
                                           const IPEndpoint& src) {
  // TODO(rwkeane): Implement this method.
}

void MdnsProbeManager::OnProbeSuccess(MdnsProbe* probe) {
  // TODO(rwkeane): Validations.

  auto it = std::find_if(ongoing_probes_.begin(), ongoing_probes_.end(),
                         [probe](const OngoingProbe& ongoing) {
                           return ongoing.probe.get() == probe;
                         });
  if (it != ongoing_probes_.end()) {
    std::unique_ptr<MdnsProbe> probe = std::move(it->probe);
    completed_probes_.push_back(std::move(probe));
    DomainName requested = std::move(it->requested_name);
    Callback* callback = it->callback;
    ongoing_probes_.erase(it);
    callback->OnDomainFound(requested, probe->target_name());
  }
}

void MdnsProbeManager::OnProbeFailure(MdnsProbe* probe) {
  // TODO(rwkeane): Implement this method.
}

MdnsProbeManager::OngoingProbe::OngoingProbe(std::unique_ptr<MdnsProbe> probe,
                                             DomainName name,
                                             Callback* callback)
    : probe(std::move(probe)),
      requested_name(std::move(name)),
      callback(callback) {}

}  // namespace discovery
}  // namespace openscreen
