// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_manager/common_browser_interfaces.h"

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/task_runner.h"
#include "build/build_config.h"
#include "components/discardable_memory/service/discardable_shared_memory_manager.h"
#include "content/browser/browser_main_loop.h"
#include "content/public/common/connection_filter.h"
#include "content/public/common/service_manager_connection.h"
#include "device/geolocation/geolocation_config.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/service_manager/public/cpp/binder_registry.h"

#if defined(OS_WIN)
#include "content/common/font_cache_dispatcher_win.h"
#endif

namespace content {

namespace {

class ConnectionFilterImpl : public ConnectionFilter {
 public:
  ConnectionFilterImpl()
      : main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
    RegisterMainThreadInterface(base::Bind(&device::GeolocationConfig::Create));
#if defined(OS_WIN)
    registry_.AddInterface(base::Bind(&FontCacheDispatcher::Create));
#endif
    auto* browser_main_loop = BrowserMainLoop::GetInstance();
    if (browser_main_loop) {
      auto* manager = browser_main_loop->discardable_shared_memory_manager();
      if (manager) {
        registry_.AddInterface(base::Bind(
            &discardable_memory::DiscardableSharedMemoryManager::Bind,
            base::Unretained(manager)));
      }
    }
  }

  ~ConnectionFilterImpl() override {}

 private:
  template <typename Interface>
  using InterfaceBinder =
      base::Callback<void(mojo::InterfaceRequest<Interface>,
                          const service_manager::BindSourceInfo&)>;

  // ConnectionFilter:
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle* interface_pipe,
                       service_manager::Connector* connector) override {
    registry_.TryBindInterface(interface_name, interface_pipe, source_info);
  }

  template <typename Interface>
  static void BindOnTaskRunner(
      const scoped_refptr<base::TaskRunner>& task_runner,
      const InterfaceBinder<Interface>& binder,
      mojo::InterfaceRequest<Interface> request,
      const service_manager::BindSourceInfo& source_info) {
    task_runner->PostTask(
        FROM_HERE, base::BindOnce(binder, std::move(request), source_info));
  }

  template <typename Interface>
  void RegisterMainThreadInterface(const InterfaceBinder<Interface>& binder) {
    registry_.AddInterface(base::Bind(&BindOnTaskRunner<Interface>,
                                      main_thread_task_runner_, binder));
  }

  const scoped_refptr<base::TaskRunner> main_thread_task_runner_;
  service_manager::BinderRegistryWithArgs<
      const service_manager::BindSourceInfo&>
      registry_;

  DISALLOW_COPY_AND_ASSIGN(ConnectionFilterImpl);
};

}  // namespace

void RegisterCommonBrowserInterfaces(ServiceManagerConnection* connection) {
  connection->AddConnectionFilter(std::make_unique<ConnectionFilterImpl>());
}

}  // namespace content
