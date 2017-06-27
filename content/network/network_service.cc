// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/network/network_service.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "content/network/network_context.h"
#include "content/public/common/content_switches.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/log/net_log_util.h"
#include "net/log/write_to_file_net_log_observer.h"
#include "services/service_manager/public/cpp/bind_source_info.h"

namespace content {

class NetworkService::MojoNetLog : public net::NetLog {
 public:
  MojoNetLog() {
    const base::CommandLine* command_line =
        base::CommandLine::ForCurrentProcess();
    if (!command_line->HasSwitch(switches::kLogNetLog))
      return;
    base::FilePath log_path =
        command_line->GetSwitchValuePath(switches::kLogNetLog);
    base::ScopedFILE file;
#if defined(OS_WIN)
    file.reset(_wfopen(log_path.value().c_str(), L"w"));
#elif defined(OS_POSIX)
    file.reset(fopen(log_path.value().c_str(), "w"));
#endif
    if (!file) {
      LOG(ERROR) << "Could not open file " << log_path.value()
                 << " for net logging";
    } else {
      write_to_file_observer_.reset(new net::WriteToFileNetLogObserver());
      write_to_file_observer_->set_capture_mode(
          net::NetLogCaptureMode::IncludeCookiesAndCredentials());
      write_to_file_observer_->StartObserving(this, std::move(file), nullptr,
                                              nullptr);
    }
  }
  ~MojoNetLog() override {
    if (write_to_file_observer_)
      write_to_file_observer_->StopObserving(nullptr);
  }

 private:
  std::unique_ptr<net::WriteToFileNetLogObserver> write_to_file_observer_;
  DISALLOW_COPY_AND_ASSIGN(MojoNetLog);
};

NetworkService::NetworkService(
    std::unique_ptr<service_manager::BinderRegistry> registry)
    : net_log_(new MojoNetLog), registry_(std::move(registry)), binding_(this) {
  registry_->AddInterface<mojom::NetworkService>(
      base::Bind(&NetworkService::Create, base::Unretained(this)));
}

NetworkService::~NetworkService() = default;

void NetworkService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_->BindInterface(source_info, interface_name,
                           std::move(interface_pipe));
}

void NetworkService::Create(const service_manager::BindSourceInfo& source_info,
                            mojom::NetworkServiceRequest request) {
  DCHECK(!binding_.is_bound());
  binding_.Bind(std::move(request));
}

void NetworkService::CreateNetworkContext(
    mojom::NetworkContextRequest request,
    mojom::NetworkContextParamsPtr params) {
  mojo::MakeStrongBinding(
      base::MakeUnique<NetworkContext>(std::move(request), std::move(params)),
      std::move(request));
}

}  // namespace content
