// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MOJO_BLINK_CONNECTOR_IMPL_H_
#define CONTENT_RENDERER_MOJO_BLINK_CONNECTOR_IMPL_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/WebKit/public/platform/Connector.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace content {

// An implementation of blink::Connector that forwards to a
// service_manager::Connector.
class CONTENT_EXPORT BlinkConnectorImpl : public blink::Connector {
 public:
  explicit BlinkConnectorImpl(
      std::unique_ptr<service_manager::Connector> connector);
  ~BlinkConnectorImpl();

  // blink::Connector override.
  void bindInterface(const char* service_name,
                     const char* interface_name,
                     mojo::ScopedMessagePipeHandle handle) override;

  void AddOverrideForTesting(
      const std::string& service_name,
      const std::string& interface_name,
      const base::Callback<void(mojo::ScopedMessagePipeHandle)>& binder);

  void ClearOverridesForTesting();

  void SetConnector(std::unique_ptr<service_manager::Connector> connector) {
    connector_ = std::move(connector);
  }

  base::WeakPtr<BlinkConnectorImpl> GetWeakPtr();

 private:
  using Binder = base::Callback<void(mojo::ScopedMessagePipeHandle)>;
  using InterfaceBinderMap = std::map<std::string, Binder>;
  using ServiceBinderMap =
      std::map<std::string, std::unique_ptr<InterfaceBinderMap>>;

  // Returns a pointer to the InterfaceBinderMap in action for |service_name|,
  // or nullptr if |service_name| has no overrides in action.
  InterfaceBinderMap* GetOverridesForService(const char* service_name);

  // Maps service names to the per-interface overrides that have been set for
  // that service.
  ServiceBinderMap service_binders_;

  std::unique_ptr<service_manager::Connector> connector_;

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  base::WeakPtrFactory<BlinkConnectorImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BlinkConnectorImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MOJO_BLINK_CONNECTOR_IMPL_H_
