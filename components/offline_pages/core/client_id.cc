// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/client_id.h"

namespace offline_pages {

ClientId::ClientId() {}

ClientId::ClientId(const std::string& name_space, const std::string& id)
    : name_space(name_space), id(id) {}

bool ClientId::operator==(const ClientId& client_id) const {
  return name_space == client_id.name_space && id == client_id.id;
}

bool ClientId::operator<(const ClientId& client_id) const {
  if (name_space == client_id.name_space)
    return (id < client_id.id);

  return name_space < client_id.name_space;
}

}  // namespace offline_pages
