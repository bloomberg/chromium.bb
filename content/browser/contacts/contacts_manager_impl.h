// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONTACTS_CONTACTS_MANAGER_IMPL_H_
#define CONTENT_BROWSER_CONTACTS_CONTACTS_MANAGER_IMPL_H_

#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/contacts/contacts_manager.mojom.h"

namespace content {

class CONTENT_EXPORT ContactsManagerImpl
    : public blink::mojom::ContactsManager {
 public:
  static void Create(blink::mojom::ContactsManagerRequest request);

  ContactsManagerImpl();
  ~ContactsManagerImpl() override;

  void Select(bool multiple,
              bool include_names,
              bool include_emails,
              SelectCallback callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContactsManagerImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_CONTACTS_CONTACTS_MANAGER_IMPL_H_
