# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'cloud_devices',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'sources': [
        'cloud_devices/cloud_device_description.cc',
        'cloud_devices/cloud_device_description.h',
        'cloud_devices/cloud_device_description_consts.cc',
        'cloud_devices/cloud_device_description_consts.h',
        'cloud_devices/description_items.h',
        'cloud_devices/description_items_inl.h',
        'cloud_devices/printer_description.cc',
        'cloud_devices/printer_description.h',
      ],
    },
  ],
}
