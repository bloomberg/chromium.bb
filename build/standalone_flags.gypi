# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # These flags are only included in the NaCl standalone build.
    # They won't get included when NaCl is built as part of another
    # project, such as Chrome.  We don't necessarily want to enable
    # these flags for Chrome source that is built as NaCl untrusted
    # code.
    'nacl_default_compile_flags': [
      # TODO(mseaborn): Move -Werror to untrusted.gypi so that it
      # applies to Chrome source built as untrusted code.  We have
      # -Werror as a NaCl-side only flag at the moment because some
      # Chrome code produces warnings when built with PNaCl/ARM.
      # See http://code.google.com/p/nativeclient/issues/detail?id=3108
      '-Werror',
      '-Wundef',
    ],
  },
}
