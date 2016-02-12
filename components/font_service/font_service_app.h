// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FONT_SERVICE_FONT_SERVICE_APP_H_
#define COMPONENTS_FONT_SERVICE_FONT_SERVICE_APP_H_

#include <stdint.h>

#include "base/macros.h"
#include "components/font_service/public/interfaces/font_service.mojom.h"
#include "mojo/public/cpp/bindings/weak_binding_set.h"
#include "mojo/services/tracing/public/cpp/tracing_impl.h"
#include "mojo/shell/public/cpp/interface_factory.h"
#include "mojo/shell/public/cpp/shell_client.h"
#include "skia/ext/skia_utils_base.h"

namespace font_service {

class FontServiceApp : public mojo::ShellClient,
                       public mojo::InterfaceFactory<FontService>,
                       public FontService {
 public:
  FontServiceApp();
  ~FontServiceApp() override;

 private:
  // mojo::ShellClient:
  void Initialize(mojo::Shell* shell, const std::string& url,
                  uint32_t id) override;
  bool AcceptConnection(mojo::Connection* connection) override;

  // mojo::InterfaceFactory<FontService>:
  void Create(mojo::Connection* connection,
              mojo::InterfaceRequest<FontService> request) override;

  // FontService:
  void MatchFamilyName(const mojo::String& family_name,
                       TypefaceStyle requested_style,
                       const MatchFamilyNameCallback& callback) override;
  void OpenStream(uint32_t id_number,
                  const OpenStreamCallback& callback) override;

  int FindOrAddPath(const SkString& path);

  mojo::WeakBindingSet<FontService> bindings_;

  mojo::TracingImpl tracing_;

  // We don't want to leak paths to our callers; we thus enumerate the paths of
  // fonts.
  SkTDArray<SkString*> paths_;

  DISALLOW_COPY_AND_ASSIGN(FontServiceApp);
};

}  // namespace font_service

#endif  // COMPONENTS_FONT_SERVICE_FONT_SERVICE_APP_H_
