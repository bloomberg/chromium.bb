// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiling_host/profiling_process_host.h"

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/profiling/constants.mojom.h"
#include "chrome/common/profiling/memlog.mojom.h"
#include "chrome/common/profiling/memlog_sender.h"
#include "chrome/common/profiling/profiling_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_manager_connection.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/service_manager/public/cpp/connector.h"

namespace profiling {

namespace {

ProfilingProcessHost* pph_singleton = nullptr;

void BindToBrowserConnector(service_manager::mojom::ConnectorRequest request) {
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::BindOnce(&BindToBrowserConnector, std::move(request)));
    return;
  }
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindConnectorRequest(std::move(request));
}

}  // namespace

ProfilingProcessHost::ProfilingProcessHost() {
  pph_singleton = this;
  LaunchAsService();
}

ProfilingProcessHost::~ProfilingProcessHost() {
  pph_singleton = nullptr;
}

// static
ProfilingProcessHost* ProfilingProcessHost::EnsureStarted() {
  static ProfilingProcessHost host;
  return &host;
}

// static
ProfilingProcessHost* ProfilingProcessHost::Get() {
  return pph_singleton;
}

// static
void ProfilingProcessHost::AddSwitchesToChildCmdLine(
    base::CommandLine* child_cmd_line) {
  // TODO(ajwong): Figure out how to trace the zygote process.
  if (child_cmd_line->GetSwitchValueASCII(switches::kProcessType) ==
      switches::kZygoteProcess) {
    return;
  }

  // TODO(ajwong): Change this to just reuse the --memlog flag. There is no
  // need for a separate pipe flag.
  //
  // Zero is browser which is specified in LaunchAsService.
  static int sender_id = 1;
  child_cmd_line->AppendSwitchASCII(switches::kMemlogPipe,
                                    base::IntToString(sender_id++));
}

void ProfilingProcessHost::RequestProcessDump(base::ProcessId pid) {
  // TODO(brettw) implement process dumping.
}

void ProfilingProcessHost::LaunchAsService() {
  // May get called on different threads, we need to be on the IO thread to
  // work.
  //
  // TODO(ajwong): This thread bouncing logic is dumb. The
  // BindToBrowserConnector() ends up jumping to the UI thread also so this is
  // at least 2 bounces. Simplify.
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::IO)) {
    content::BrowserThread::GetTaskRunnerForThread(content::BrowserThread::IO)
        ->PostTask(FROM_HERE,
                   base::BindOnce(&ProfilingProcessHost::LaunchAsService,
                                  base::Unretained(this)));
    return;
  }

  // TODO(ajwong): There's likely a cleaner preexisting connector sitting
  // around. See if there's a way to reuse that rather than creating our own?
  //
  // TODO(ajwong): Dedupe with InitMemlogSenderIfNecessary().
  service_manager::mojom::ConnectorRequest connector_request;
  std::unique_ptr<service_manager::Connector> connector =
      service_manager::Connector::Create(&connector_request);

  BindToBrowserConnector(std::move(connector_request));

  mojom::MemlogPtr memlog;
  connector->BindInterface(mojom::kServiceName, &memlog);

  mojo::edk::PlatformChannelPair data_channel;
  memlog->AddSender(
      mojo::WrapPlatformFile(data_channel.PassServerHandle().release().handle),
      0);  // 0 is the browser.
  StartMemlogSender(base::ScopedPlatformFile(
      data_channel.PassClientHandle().release().handle));
}

}  // namespace profiling
