# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'component_updater',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'component_updater/component_updater_paths.cc',
        'component_updater/component_updater_paths.h',
        'component_updater/component_updater_switches.cc',
        'component_updater/component_updater_switches.h',
        'component_updater/pref_names.cc',
        'component_updater/pref_names.h',
      ],
    },
  ],
}
