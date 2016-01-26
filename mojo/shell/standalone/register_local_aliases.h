// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_STANDALONE_REGISTER_LOCAL_ALIASES_H_
#define MOJO_SHELL_STANDALONE_REGISTER_LOCAL_ALIASES_H_

namespace mojo {
namespace shell {
class PackageManagerImpl;

void RegisterLocalAliases(PackageManagerImpl* manager);

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_STANDALONE_REGISTER_LOCAL_ALIASES_H_
