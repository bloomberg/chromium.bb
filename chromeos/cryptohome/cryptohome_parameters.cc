// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/cryptohome/cryptohome_parameters.h"

namespace cryptohome {

Authorization::Authorization(const std::string& key, const std::string& label)
    : key(key), label(label) {}

Authorization::Authorization(const KeyDefinition& key_def)
    : key(key_def.key), label(key_def.label) {}

MountParameters::MountParameters(bool ephemeral) : ephemeral(ephemeral) {}

MountParameters::~MountParameters() {}

KeyDefinition::KeyDefinition(const std::string& key,
                             const std::string& label,
                             int /*AuthKeyPrivileges*/ privileges)
    : label(label), revision(1), key(key), privileges(privileges) {}

KeyDefinition::~KeyDefinition() {}

}  // namespace cryptohome
