// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_HTML_VIEWER_H_
#define COMPONENTS_HTML_VIEWER_HTML_VIEWER_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/shell/public/cpp/interface_factory.h"
#include "mojo/shell/public/cpp/shell_client.h"
#include "mojo/shell/public/interfaces/content_handler.mojom.h"

namespace html_viewer {

class GlobalState;

class HTMLViewer
    : public mojo::ShellClient,
      public mojo::InterfaceFactory<mojo::shell::mojom::ContentHandler> {
 public:
  HTMLViewer();
  ~HTMLViewer() override;

 protected:
  GlobalState* global_state() const { return global_state_.get(); }
  mojo::Shell* shell() const { return shell_; }

  // Overridden from mojo::ShellClient:
  void Initialize(mojo::Shell* shell, const std::string& url,
                  uint32_t id) override;

 private:
  // Overridden from mojo::ShellClient:
  bool AcceptConnection(
      mojo::Connection* connection) override;

  // Overridden from InterfaceFactory<shell::mojom::ContentHandler>
  void Create(mojo::Connection* connection,
              mojo::InterfaceRequest<mojo::shell::mojom::ContentHandler>
                  request) override;

  scoped_ptr<GlobalState> global_state_;
  mojo::Shell* shell_;

  DISALLOW_COPY_AND_ASSIGN(HTMLViewer);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_HTML_VIEWER_H_
