// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_SIMPLE_CONNECTION_FILTER_H_
#define CONTENT_PUBLIC_COMMON_SIMPLE_CONNECTION_FILTER_H_

#include <memory>
#include <string>

#include "content/common/content_export.h"
#include "content/public/common/connection_filter.h"

namespace service_manager {
class BinderRegistry;
}

namespace content {

class CONTENT_EXPORT SimpleConnectionFilter : public ConnectionFilter {
 public:
  explicit SimpleConnectionFilter(
      std::unique_ptr<service_manager::BinderRegistry> registry);
  ~SimpleConnectionFilter() override;

  // ConnectionFilter:
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle* interface_pipe,
                       service_manager::Connector* connector) override;

 private:
  std::unique_ptr<service_manager::BinderRegistry> registry_;

  DISALLOW_COPY_AND_ASSIGN(SimpleConnectionFilter);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_SIMPLE_CONNECTION_FILTER_H_
