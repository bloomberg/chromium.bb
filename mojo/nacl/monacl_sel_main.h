// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_NACL_MONACL_SEL_MAIN_H_
#define MOJO_NACL_MONACL_SEL_MAIN_H_

namespace mojo {

void LaunchNaCl(const char* nexe_file, const char* irt_file,
                int app_argc, char* app_argv[]);

} // namespace mojo

#endif // MOJO_NACL_MONACL_SEL_MAIN_H_
