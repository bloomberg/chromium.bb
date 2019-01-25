// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/chrome_content_browser_client_resource_coordinator_part.h"

#include <utility>

#include "base/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/resource_coordinator/render_process_user_data.h"

namespace {

void BindProcessResourceCoordinator(
    content::RenderProcessHost* render_process_host,
    resource_coordinator::mojom::ProcessCoordinationUnitRequest request) {
  resource_coordinator::RenderProcessUserData* user_data =
      resource_coordinator::RenderProcessUserData::GetForRenderProcessHost(
          render_process_host);

  user_data->process_resource_coordinator()->AddBinding(std::move(request));
}

}  // namespace

ChromeContentBrowserClientResourceCoordinatorPart::
    ChromeContentBrowserClientResourceCoordinatorPart() = default;
ChromeContentBrowserClientResourceCoordinatorPart::
    ~ChromeContentBrowserClientResourceCoordinatorPart() = default;

void ChromeContentBrowserClientResourceCoordinatorPart::
    ExposeInterfacesToRenderer(
        service_manager::BinderRegistry* registry,
        blink::AssociatedInterfaceRegistry* associated_registry,
        content::RenderProcessHost* render_process_host) {
  registry->AddInterface(
      base::BindRepeating(&BindProcessResourceCoordinator,
                          base::Unretained(render_process_host)),
      base::SequencedTaskRunnerHandle::Get());

  // When a RenderFrameHost is "resurrected" with a new process  it will already
  // have user data attached. This will happen on renderer crash.
  if (!resource_coordinator::RenderProcessUserData::GetForRenderProcessHost(
          render_process_host)) {
    resource_coordinator::RenderProcessUserData::CreateForRenderProcessHost(
        render_process_host);
  }
}
