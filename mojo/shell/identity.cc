// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/identity.h"

#include "mojo/shell/public/cpp/names.h"
#include "mojo/shell/public/interfaces/shell.mojom.h"

namespace mojo {
namespace shell {

Identity::Identity() {}

Identity::Identity(const std::string& name)
    : Identity(name, GetNamePath(name), mojom::Connector::kUserRoot) {}

Identity::Identity(const std::string& name, const std::string& qualifier,
                   uint32_t user_id)
    : name_(name),
      qualifier_(qualifier.empty() ? GetNamePath(name_) : qualifier),
      user_id_(user_id) {}

Identity::~Identity() {}

bool Identity::operator<(const Identity& other) const {
  // We specifically don't include filter in the equivalence check because we
  // don't quite know how this should work yet.
  // TODO(beng): figure out how it should work.
  if (name_ != other.name_)
    return name_ < other.name_;
  if (qualifier_ != other.qualifier_)
    return qualifier_ < other.qualifier_;
  return user_id_ < other.user_id_;
}

bool Identity::operator==(const Identity& other) const {
  // We specifically don't include filter in the equivalence check because we
  // don't quite know how this should work yet.
  // TODO(beng): figure out how it should work.
  return other.name_ == name_ && other.qualifier_ == qualifier_ &&
         other.user_id_ == user_id_;
}

Identity CreateShellIdentity() {
  Identity id = Identity("mojo:shell", "", mojom::Connector::kUserRoot);
  id.set_filter(GetPermissiveCapabilityFilter());
  return id;
}

}  // namespace shell
}  // namespace mojo
