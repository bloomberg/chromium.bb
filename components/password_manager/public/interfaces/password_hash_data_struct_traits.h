// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_PUBLIC_INTERFACES_PASSWORD_HASH_DATA_STRUCT_TRAITS_H_
#define COMPONENTS_PASSWORD_MANAGER_PUBLIC_INTERFACES_PASSWORD_HASH_DATA_STRUCT_TRAITS_H_

#include <string>

#include "components/password_manager/core/browser/hash_password_manager.h"
#include "components/password_manager/public/interfaces/password_hash_data.mojom.h"

namespace mojo {

template <>
struct StructTraits<password_manager::mojom::PasswordHashDataDataView,
                    password_manager::PasswordHashData> {
  static const std::string& username(
      const password_manager::PasswordHashData& r) {
    return r.username;
  }
  static uint32_t length(const password_manager::PasswordHashData& r) {
    return r.length;
  }
  static const std::string& salt(const password_manager::PasswordHashData& r) {
    return r.salt;
  }
  static uint64_t hash(const password_manager::PasswordHashData& r) {
    return r.hash;
  }

  static bool force_update(const password_manager::PasswordHashData& r) {
    return r.force_update;
  }

  static bool Read(password_manager::mojom::PasswordHashDataDataView data,
                   password_manager::PasswordHashData* out) {
    if (!data.ReadUsername(&out->username) || !data.ReadSalt(&out->salt))
      return false;
    out->length = data.length();
    out->force_update = data.force_update();
    return true;
  }
};

}  // namespace mojo

#endif  // COMPONENTS_PASSWORD_MANAGER_PUBLIC_INTERFACES_PASSWORD_HASH_DATA_STRUCT_TRAITS_H_
