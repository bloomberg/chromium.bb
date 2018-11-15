// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/service.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/domain_reliability/context.h"
#include "components/domain_reliability/monitor.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

namespace domain_reliability {

namespace {

void AddContextForTestingOnNetworkTaskRunner(
    base::WeakPtr<DomainReliabilityMonitor> monitor,
    std::unique_ptr<const DomainReliabilityConfig> config) {
  if (!monitor)
    return;

  monitor->AddContextForTesting(std::move(config));
}

std::unique_ptr<base::Value> GetWebUIDataOnNetworkTaskRunner(
    base::WeakPtr<DomainReliabilityMonitor> monitor) {
  if (!monitor) {
    base::DictionaryValue* dict = new base::DictionaryValue();
    dict->SetString("error", "no_monitor");
    return std::unique_ptr<base::Value>(dict);
  }

  return monitor->GetWebUIData();
}

}  // namespace

class DomainReliabilityServiceImpl : public DomainReliabilityService {
 public:
  explicit DomainReliabilityServiceImpl(
      const std::string& upload_reporter_string)
      : upload_reporter_string_(upload_reporter_string), weak_factory_(this) {}

  ~DomainReliabilityServiceImpl() override {}

  // DomainReliabilityService implementation:

  std::unique_ptr<DomainReliabilityMonitor> CreateMonitor(
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
      const DomainReliabilityContext::UploadAllowedCallback&
          upload_allowed_callback) override {
    DCHECK(!network_task_runner_);

    std::unique_ptr<DomainReliabilityMonitor> monitor(
        new DomainReliabilityMonitor(upload_reporter_string_,
                                     upload_allowed_callback, ui_task_runner,
                                     network_task_runner));

    monitor_ = monitor->MakeWeakPtr();
    network_task_runner_ = network_task_runner;

    return monitor;
  }

  void ClearBrowsingData(
      DomainReliabilityClearMode clear_mode,
      const base::Callback<bool(const GURL&)>& origin_filter,
      const base::Closure& callback) override {
    DCHECK(network_task_runner_);

    network_task_runner_->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&DomainReliabilityMonitor::ClearBrowsingData, monitor_,
                       clear_mode, origin_filter),
        callback);
  }

  void GetWebUIData(const base::Callback<void(std::unique_ptr<base::Value>)>&
                        callback) const override {
    DCHECK(network_task_runner_);

    PostTaskAndReplyWithResult(
        network_task_runner_.get(),
        FROM_HERE,
        base::Bind(&GetWebUIDataOnNetworkTaskRunner, monitor_),
        callback);
  }

  void SetDiscardUploadsForTesting(bool discard_uploads) override {
    DCHECK(network_task_runner_);

    network_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&DomainReliabilityMonitor::SetDiscardUploads,
                                  monitor_, discard_uploads));
  }

  void AddContextForTesting(
      std::unique_ptr<const DomainReliabilityConfig> config) override {
    DCHECK(network_task_runner_);

    network_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&AddContextForTestingOnNetworkTaskRunner,
                                  monitor_, std::move(config)));
  }

  void ForceUploadsForTesting() override {
    DCHECK(network_task_runner_);

    network_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&DomainReliabilityMonitor::ForceUploadsForTesting,
                       monitor_));
  }

 private:
  std::string upload_reporter_string_;
  base::WeakPtr<DomainReliabilityMonitor> monitor_;
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;
  base::WeakPtrFactory<DomainReliabilityServiceImpl> weak_factory_;
};

// static
DomainReliabilityService* DomainReliabilityService::Create(
    const std::string& upload_reporter_string) {
  return new DomainReliabilityServiceImpl(upload_reporter_string);
}

DomainReliabilityService::~DomainReliabilityService() {}

DomainReliabilityService::DomainReliabilityService() {}

}  // namespace domain_reliability
