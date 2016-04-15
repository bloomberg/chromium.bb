// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_CATALOG_VIEWER_CATALOG_VIEWER_H_
#define MASH_CATALOG_VIEWER_CATALOG_VIEWER_H_

#include <map>
#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "services/shell/public/cpp/shell_client.h"
#include "services/tracing/public/cpp/tracing_impl.h"

namespace views {
class AuraInit;
}

namespace mash {
namespace catalog_viewer {

class CatalogViewer : public shell::ShellClient {
 public:
  CatalogViewer();
  ~CatalogViewer() override;

 private:
  // shell::ShellClient:
  void Initialize(shell::Connector* connector,
                  const shell::Identity& identity,
                  uint32_t id) override;

  mojo::TracingImpl tracing_;
  std::unique_ptr<views::AuraInit> aura_init_;

  DISALLOW_COPY_AND_ASSIGN(CatalogViewer);
};

}  // namespace catalog_viewer
}  // namespace mash

#endif  // MASH_CATALOG_VIEWER_CATALOG_VIEWER_H_
