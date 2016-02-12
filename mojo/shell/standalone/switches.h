// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_STANDALONE_SWITCHES_H_
#define MOJO_SHELL_STANDALONE_SWITCHES_H_

namespace mojo {
namespace switches {

// All switches in alphabetical order. The switches should be documented
// alongside the definition of their values in the .cc file.
extern const char kApp[];
extern const char kContentHandlers[];
extern const char kForceInProcess[];
extern const char kHelp[];
extern const char kMapOrigin[];
extern const char kURLMappings[];
extern const char kUseTemporaryUserDataDir[];
extern const char kUserDataDir[];

}  // namespace switches
}  // namespace mojo

#endif  // MOJO_SHELL_STANDALONE_SWITCHES_H_
