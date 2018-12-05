// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/contacts/contacts_manager_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace content {

// static
void ContactsManagerImpl::Create(blink::mojom::ContactsManagerRequest request) {
  mojo::MakeStrongBinding(std::make_unique<ContactsManagerImpl>(),
                          std::move(request));
}

ContactsManagerImpl::ContactsManagerImpl() = default;
ContactsManagerImpl::~ContactsManagerImpl() = default;

void ContactsManagerImpl::Select(bool multiple,
                                 bool include_names,
                                 bool include_emails,
                                 SelectCallback callback) {
  // TODO(finnur): Implement showing the dialog instead of returning null.
  std::move(callback).Run(base::nullopt);
}

}  // namespace content
