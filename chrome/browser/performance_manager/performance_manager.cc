// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/performance_manager.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "build/build_config.h"
#include "chrome/browser/performance_manager/observers/metrics_collector.h"
#include "chrome/browser/performance_manager/observers/page_signal_generator_impl.h"
#include "chrome/browser/performance_manager/observers/working_set_trimmer_win.h"
#include "content/public/common/service_manager_connection.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_features.h"

namespace performance_manager {

namespace {
PerformanceManager* g_performance_manager = nullptr;

scoped_refptr<base::SequencedTaskRunner> CreateTaskRunner() {
  return base::CreateSequencedTaskRunnerWithTraits(
      {base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN, base::MayBlock()});
}

}  // namespace

PerformanceManager* PerformanceManager::GetInstance() {
  return g_performance_manager;
}

PerformanceManager::PerformanceManager()
    : service_keepalive_(static_cast<service_manager::ServiceBinding*>(nullptr),
                         base::nullopt),
      task_runner_(CreateTaskRunner()),
      introspector_(&graph_) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

PerformanceManager::~PerformanceManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
std::unique_ptr<PerformanceManager> PerformanceManager::Create() {
  DCHECK_EQ(nullptr, g_performance_manager);
  std::unique_ptr<PerformanceManager> instance =
      base::WrapUnique(new PerformanceManager());

  instance->OnStart();
  g_performance_manager = instance.get();

  return instance;
}

// static
void PerformanceManager::Destroy(std::unique_ptr<PerformanceManager> instance) {
  DCHECK_EQ(instance.get(), g_performance_manager);
  g_performance_manager = nullptr;

  instance->task_runner_->DeleteSoon(FROM_HERE, instance.release());
}

void PerformanceManager::BindInterface(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle message_pipe) {
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&PerformanceManager::BindInterfaceImpl,
                                        base::Unretained(this), interface_name,
                                        std::move(message_pipe)));
}

void PerformanceManager::OnStart() {
  // Some tests don't initialize the service manager connection, so this class
  // tolerates its absence for tests.
  auto* connection = content::ServiceManagerConnection::GetForProcess();
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &PerformanceManager::OnStartImpl, base::Unretained(this),
          connection ? connection->GetConnector()->Clone() : nullptr));
}

void PerformanceManager::OnStartImpl(
    std::unique_ptr<service_manager::Connector> connector) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  interface_registry_.AddInterface(
      base::BindRepeating(&CoordinationUnitIntrospectorImpl::BindToInterface,
                          base::Unretained(&introspector_)));

  // Register new |GraphObserver| implementations here.
  auto page_signal_generator_impl = std::make_unique<PageSignalGeneratorImpl>();
  interface_registry_.AddInterface(
      base::BindRepeating(&PageSignalGeneratorImpl::BindToInterface,
                          base::Unretained(page_signal_generator_impl.get())));
  graph_.RegisterObserver(std::move(page_signal_generator_impl));

  graph_.RegisterObserver(std::make_unique<MetricsCollector>());

#if defined(OS_WIN)
  if (base::FeatureList::IsEnabled(features::kEmptyWorkingSet))
    graph_.RegisterObserver(std::make_unique<WorkingSetTrimmer>());
#endif

  interface_registry_.AddInterface(base::BindRepeating(
      &PerformanceManager::BindWebUIGraphDump, base::Unretained(this)));

  if (connector) {
    ukm_recorder_ = ukm::MojoUkmRecorder::Create(connector.get());
    graph_.set_ukm_recorder(ukm_recorder_.get());
  }

  graph_.OnStart(&interface_registry_, &service_keepalive_);
}

void PerformanceManager::BindInterfaceImpl(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle message_pipe) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  interface_registry_.BindInterface(interface_name, std::move(message_pipe),
                                    service_manager::BindSourceInfo());
}

void PerformanceManager::BindWebUIGraphDump(
    resource_coordinator::mojom::WebUIGraphDumpRequest request,
    const service_manager::BindSourceInfo& source_info) {
  std::unique_ptr<WebUIGraphDumpImpl> graph_dump =
      std::make_unique<WebUIGraphDumpImpl>(&graph_);

  auto error_callback =
      base::BindOnce(&PerformanceManager::OnGraphDumpConnectionError,
                     base::Unretained(this), graph_dump.get());
  graph_dump->Bind(std::move(request), std::move(error_callback));

  graph_dumps_.push_back(std::move(graph_dump));
}

void PerformanceManager::OnGraphDumpConnectionError(
    WebUIGraphDumpImpl* graph_dump) {
  const auto it = std::find_if(
      graph_dumps_.begin(), graph_dumps_.end(),
      [graph_dump](const std::unique_ptr<WebUIGraphDumpImpl>& graph_dump_ptr) {
        return graph_dump_ptr.get() == graph_dump;
      });

  DCHECK(it != graph_dumps_.end());

  graph_dumps_.erase(it);
}

}  // namespace performance_manager
