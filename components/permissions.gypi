# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN version: //components/permissions:mojo_bindings
      'target_name': 'permissions_mojo_bindings',
      'type': 'static_library',
      'includes': [
        '../mojo/mojom_bindings_generator.gypi',
      ],
      'sources': [
        'permissions/permission.mojom',
        'permissions/permission_status.mojom',
      ],
    },
  ],
}
