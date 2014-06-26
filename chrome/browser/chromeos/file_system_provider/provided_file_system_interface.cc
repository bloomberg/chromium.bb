// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/provided_file_system_interface.h"

namespace chromeos {
namespace file_system_provider {

EntryMetadata::EntryMetadata() : is_directory(false), size(0) {
}

EntryMetadata::~EntryMetadata() {
}

}  // namespace file_system_provider
}  // namespace chromeos
