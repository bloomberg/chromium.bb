# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
     'monacl_codegen_dir': '<(SHARED_INTERMEDIATE_DIR)/<!(python <(DEPTH)/build/inverse_depth.py <(DEPTH))/nacl',
   },
}
