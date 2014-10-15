// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_SWITCHES_H_
#define MOJO_SHELL_SWITCHES_H_

namespace switches {

// All switches in alphabetical order. The switches should be documented
// alongside the definition of their values in the .cc file and, as needed,
// in mojo_main's Usage() function.
extern const char kArgsFor[];
extern const char kHelp[];
extern const char kChildProcessType[];
extern const char kContentHandlers[];
extern const char kDisableCache[];
extern const char kEnableExternalApplications[];
extern const char kEnableMultiprocess[];
extern const char kOrigin[];
extern const char kSpy[];
extern const char kURLMappings[];

}  // namespace switches

#endif  // MOJO_SHELL_SWITCHES_H_
