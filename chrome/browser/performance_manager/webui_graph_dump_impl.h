// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_WEBUI_GRAPH_DUMP_IMPL_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_WEBUI_GRAPH_DUMP_IMPL_H_

#include "chrome/browser/performance_manager/webui_graph_dump.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace performance_manager {

class GraphImpl;

class WebUIGraphDumpImpl : public mojom::WebUIGraphDump {
 public:
  explicit WebUIGraphDumpImpl(GraphImpl* graph);
  ~WebUIGraphDumpImpl() override;

  // WebUIGraphDump implementation.
  void GetCurrentGraph(GetCurrentGraphCallback callback) override;

  // Bind this instance to |request| with the |error_handler|.
  void Bind(mojom::WebUIGraphDumpRequest request,
            base::OnceClosure error_handler);

 private:
  GraphImpl* graph_;
  mojo::Binding<mojom::WebUIGraphDump> binding_;

  DISALLOW_COPY_AND_ASSIGN(WebUIGraphDumpImpl);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_WEBUI_GRAPH_DUMP_IMPL_H_
