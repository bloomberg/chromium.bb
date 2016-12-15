// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webshare/share_service_impl.h"

#include "mojo/public/cpp/bindings/strong_binding.h"

// static
void ShareServiceImpl::Create(blink::mojom::ShareServiceRequest request) {
  mojo::MakeStrongBinding(base::MakeUnique<ShareServiceImpl>(),
                          std::move(request));
}

void ShareServiceImpl::Share(const std::string& title,
                             const std::string& text,
                             const GURL& url,
                             const ShareCallback& callback) {
  // TODO(constantina): Implement Web Share Target here.
  NOTIMPLEMENTED();
  callback.Run(base::Optional<std::string>("Not implemented: navigator.share"));
}
