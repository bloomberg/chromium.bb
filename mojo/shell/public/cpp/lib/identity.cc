// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/public/cpp/identity.h"

#include "base/guid.h"
#include "mojo/shell/public/cpp/names.h"

namespace mojo {

Identity::Identity() {}

Identity::Identity(const std::string& name, const std::string& user_id)
    : Identity(name, user_id, "") {}

Identity::Identity(const std::string& name, const std::string& user_id,
                   const std::string& instance)
    : name_(name),
      user_id_(user_id),
      instance_(instance.empty() ? GetNamePath(name_) : instance) {
  CHECK(!user_id.empty());
  CHECK(base::IsValidGUID(user_id));
}

Identity::Identity(const Identity& other) = default;

Identity::~Identity() {}

bool Identity::operator<(const Identity& other) const {
  if (name_ != other.name_)
    return name_ < other.name_;
  if (instance_ != other.instance_)
    return instance_ < other.instance_;
  return user_id_ < other.user_id_;
}

bool Identity::operator==(const Identity& other) const {
  return other.name_ == name_ && other.instance_ == instance_ &&
         other.user_id_ == user_id_;
}

// static
shell::mojom::IdentityPtr
    TypeConverter<shell::mojom::IdentityPtr, Identity>::Convert(
        const Identity& input) {
  shell::mojom::IdentityPtr identity(shell::mojom::Identity::New());
  identity->name = input.name();
  identity->user_id = input.user_id();
  identity->instance = input.instance();
  return identity;
}

// static
Identity TypeConverter<Identity, shell::mojom::IdentityPtr>::Convert(
    const shell::mojom::IdentityPtr& input) {
  return Identity(input->name, input->user_id, input->instance);
}

}  // namespace mojo
