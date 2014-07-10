// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/service.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner_util.h"
#include "components/domain_reliability/monitor.h"
#include "net/url_request/url_request_context_getter.h"

namespace domain_reliability {

namespace {

scoped_ptr<base::Value> GetWebUIDataOnNetworkTaskRunner(
    base::WeakPtr<DomainReliabilityMonitor> monitor) {
  if (!monitor) {
    base::DictionaryValue* dict = new base::DictionaryValue();
    dict->SetString("error", "no_monitor");
    return scoped_ptr<base::Value>(dict);
  }

  return monitor->GetWebUIData();
}

}  // namespace

class DomainReliabilityServiceImpl : public DomainReliabilityService {
 public:
  explicit DomainReliabilityServiceImpl(
      const std::string& upload_reporter_string)
      : upload_reporter_string_(upload_reporter_string) {}

  virtual ~DomainReliabilityServiceImpl() {}

  // DomainReliabilityService implementation:

  virtual scoped_ptr<DomainReliabilityMonitor> CreateMonitor(
      scoped_refptr<base::SequencedTaskRunner> network_task_runner) OVERRIDE {
    DCHECK(!network_task_runner_);

    scoped_ptr<DomainReliabilityMonitor> monitor(
        new DomainReliabilityMonitor(upload_reporter_string_));

    monitor_ = monitor->MakeWeakPtr();
    network_task_runner_ = network_task_runner;

    return monitor.Pass();
  }

  virtual void ClearBrowsingData(DomainReliabilityClearMode clear_mode,
                                 const base::Closure& callback) OVERRIDE {
    DCHECK(network_task_runner_);

    network_task_runner_->PostTaskAndReply(
        FROM_HERE,
        base::Bind(&DomainReliabilityMonitor::ClearBrowsingData,
                   monitor_,
                   clear_mode),
        callback);
  }

  virtual void GetWebUIData(
      const base::Callback<void(scoped_ptr<base::Value>)>& callback)
      const OVERRIDE {
    DCHECK(network_task_runner_);

    PostTaskAndReplyWithResult(
        network_task_runner_,
        FROM_HERE,
        base::Bind(&GetWebUIDataOnNetworkTaskRunner, monitor_),
        callback);
  }

 private:
  std::string upload_reporter_string_;
  base::WeakPtr<DomainReliabilityMonitor> monitor_;
  scoped_refptr<base::SequencedTaskRunner> network_task_runner_;
};

// static
DomainReliabilityService* DomainReliabilityService::Create(
    const std::string& upload_reporter_string) {
  return new DomainReliabilityServiceImpl(upload_reporter_string);
}

DomainReliabilityService::~DomainReliabilityService() {}

DomainReliabilityService::DomainReliabilityService() {}

}  // namespace domain_reliability
