// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/mus/render_widget_window_tree_client_factory.h"

#include <stdint.h>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/common/render_widget_window_tree_client_factory.mojom.h"
#include "content/public/common/connection_filter.h"
#include "content/public/common/mojo_shell_connection.h"
#include "content/renderer/mus/render_widget_mus_connection.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/shell/public/cpp/interface_factory.h"
#include "services/shell/public/cpp/service.h"
#include "services/ui/public/interfaces/window_tree.mojom.h"
#include "url/gurl.h"

namespace content {

namespace {

void BindMusConnectionOnMainThread(
    uint32_t routing_id,
    ui::mojom::WindowTreeClientRequest request) {
  RenderWidgetMusConnection* connection =
      RenderWidgetMusConnection::GetOrCreate(routing_id);
  connection->Bind(std::move(request));
}

// This object's lifetime is managed by MojoShellConnection because it's a
// registered with it.
class RenderWidgetWindowTreeClientFactoryImpl
    : public ConnectionFilter,
      public shell::InterfaceFactory<
          mojom::RenderWidgetWindowTreeClientFactory>,
      public mojom::RenderWidgetWindowTreeClientFactory {
 public:
  RenderWidgetWindowTreeClientFactoryImpl() {
    DCHECK(MojoShellConnection::GetForProcess());

    main_thread_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  }

  ~RenderWidgetWindowTreeClientFactoryImpl() override {}

 private:
  // ConnectionFilter implementation:
  bool OnConnect(shell::Connection* connection,
                 shell::Connector* connector) override {
    connection->AddInterface<mojom::RenderWidgetWindowTreeClientFactory>(this);
    return true;
  }

  // shell::InterfaceFactory<mojom::RenderWidgetWindowTreeClientFactory>:
  void Create(shell::Connection* connection,
              mojo::InterfaceRequest<mojom::RenderWidgetWindowTreeClientFactory>
                  request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  // mojom::RenderWidgetWindowTreeClientFactory implementation.
  void CreateWindowTreeClientForRenderWidget(
      uint32_t routing_id,
      ui::mojom::WindowTreeClientRequest request) override {
    main_thread_task_runner_->PostTask(
        FROM_HERE, base::Bind(&BindMusConnectionOnMainThread, routing_id,
                              base::Passed(&request)));
  }

  scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner_;
  mojo::BindingSet<mojom::RenderWidgetWindowTreeClientFactory> bindings_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetWindowTreeClientFactoryImpl);
};

}  // namespace

void CreateRenderWidgetWindowTreeClientFactory(
    MojoShellConnection* connection) {
  connection->AddConnectionFilter(
      base::MakeUnique<RenderWidgetWindowTreeClientFactoryImpl>());
}

}  // namespace content
