// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONTACTS_CONTACTS_PROVIDER_H_
#define CONTENT_BROWSER_CONTACTS_CONTACTS_PROVIDER_H_

#include "third_party/blink/public/mojom/contacts/contacts_manager.mojom.h"

namespace content {

class ContactsProvider {
 public:
  ContactsProvider() = default;
  virtual ~ContactsProvider() = default;

  // Launches the Contacts Picker Dialog and waits for the results to come back.
  // |callback| is called with the contacts list, once the operation finishes.
  virtual void Select(
      bool multiple,
      bool include_names,
      bool include_emails,
      bool include_tel,
      blink::mojom::ContactsManager::SelectCallback callback) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CONTACTS_CONTACTS_PROVIDER_ANDROID_H_
