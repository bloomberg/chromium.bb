// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interface_provider_filtering.h"

#include <utility>

#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "services/service_manager/public/cpp/connector.h"

namespace content {
namespace {

void FilterInterfacesImpl(
    const char* spec,
    int process_id,
    service_manager::mojom::InterfaceProviderRequest request,
    service_manager::mojom::InterfaceProviderPtr provider) {
  RenderProcessHost* process = RenderProcessHost::FromID(process_id);
  if (!process)
    return;

  service_manager::Connector* connector =
      BrowserContext::GetConnectorFor(process->GetBrowserContext());
  // |connector| is null in unit tests.
  if (!connector)
    return;

  connector->FilterInterfaces(spec, process->GetChildIdentity(),
                              std::move(request), std::move(provider));
}

}  // namespace

service_manager::mojom::InterfaceProviderRequest
FilterRendererExposedInterfaces(
    const char* spec,
    int process_id,
    service_manager::mojom::InterfaceProviderRequest request) {
  service_manager::mojom::InterfaceProviderPtr provider;
  auto filtered_request = mojo::MakeRequest(&provider);
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(&FilterInterfacesImpl, spec, process_id,
                       std::move(request), std::move(provider)));
  } else {
    FilterInterfacesImpl(spec, process_id, std::move(request),
                         std::move(provider));
  }
  return filtered_request;
}

}  // namespace content
