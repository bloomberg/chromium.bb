# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/arc
      'target_name': 'arc',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        'arc_mojo_bindings',
        '../base/base.gyp:base',
        '../chromeos/chromeos.gyp:chromeos',
        '../ipc/ipc.gyp:ipc',
      ],
      'sources': [
        'arc/arc_bridge_bootstrap.cc',
        'arc/arc_bridge_bootstrap.h',
        'arc/arc_bridge_service.cc',
        'arc/arc_bridge_service.h',
        'arc/arc_bridge_service_impl.cc',
        'arc/arc_bridge_service_impl.h',
        'arc/arc_service_manager.cc',
        'arc/arc_service_manager.h',
      ],
    },
    {
      # GN version: //components/arc_test_support
      'target_name': 'arc_test_support',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        'arc',
      ],
      'sources': [
        'arc/test/fake_arc_bridge_instance.cc',
        'arc/test/fake_arc_bridge_instance.h',
        'arc/test/fake_arc_bridge_service.cc',
        'arc/test/fake_arc_bridge_service.h',
      ],
    },
    {
      # GN version: //components/arc:mojo_bindings
      'target_name': 'arc_mojo_bindings',
      'type': 'static_library',
      'includes': [
        '../third_party/mojo/mojom_bindings_generator.gypi',
      ],
      'sources': [
        'arc/common/arc_bridge.mojom',
      ],
    },
  ],
}
