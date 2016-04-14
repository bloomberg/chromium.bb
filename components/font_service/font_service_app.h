// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FONT_SERVICE_FONT_SERVICE_APP_H_
#define COMPONENTS_FONT_SERVICE_FONT_SERVICE_APP_H_

#include <stdint.h>
#include <vector>

#include "base/macros.h"
#include "components/font_service/public/interfaces/font_service.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/shell/public/cpp/interface_factory.h"
#include "services/shell/public/cpp/shell_client.h"
#include "services/tracing/public/cpp/tracing_impl.h"
#include "skia/ext/skia_utils_base.h"

namespace font_service {

class FontServiceApp : public shell::ShellClient,
                       public shell::InterfaceFactory<FontService>,
                       public FontService {
 public:
  FontServiceApp();
  ~FontServiceApp() override;

 private:
  // shell::ShellClient:
  void Initialize(shell::Connector* connector,
                  const shell::Identity& identity,
                  uint32_t id) override;
  bool AcceptConnection(shell::Connection* connection) override;

  // shell::InterfaceFactory<FontService>:
  void Create(shell::Connection* connection,
              mojo::InterfaceRequest<FontService> request) override;

  // FontService:
  void MatchFamilyName(const mojo::String& family_name,
                       TypefaceStylePtr requested_style,
                       const MatchFamilyNameCallback& callback) override;
  void OpenStream(uint32_t id_number,
                  const OpenStreamCallback& callback) override;

  int FindOrAddPath(const SkString& path);

  mojo::BindingSet<FontService> bindings_;

  mojo::TracingImpl tracing_;

  // We don't want to leak paths to our callers; we thus enumerate the paths of
  // fonts.
  std::vector<SkString> paths_;

  DISALLOW_COPY_AND_ASSIGN(FontServiceApp);
};

}  // namespace font_service

#endif  // COMPONENTS_FONT_SERVICE_FONT_SERVICE_APP_H_
