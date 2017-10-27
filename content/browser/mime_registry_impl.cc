// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/mime_registry_impl.h"

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/base/mime_util.h"

namespace content {

MimeRegistryImpl::MimeRegistryImpl() = default;

MimeRegistryImpl::~MimeRegistryImpl() = default;

// static
void MimeRegistryImpl::Create(
    blink::mojom::MimeRegistryRequest request) {
  mojo::MakeStrongBinding(std::make_unique<MimeRegistryImpl>(),
                          std::move(request));
}

void MimeRegistryImpl::GetMimeTypeFromExtension(
    const std::string& extension,
    GetMimeTypeFromExtensionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::string mime_type;
  net::GetMimeTypeFromExtension(
      base::FilePath::FromUTF8Unsafe(extension).value(), &mime_type);
  std::move(callback).Run(mime_type);
}

}  // namespace content
