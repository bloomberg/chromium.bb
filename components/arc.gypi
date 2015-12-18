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
        '../ui/aura/aura.gyp:aura',
        '../ui/events/events.gyp:events_base',
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
        'arc/input/arc_input_bridge.h',
        'arc/input/arc_input_bridge_impl.cc',
        'arc/input/arc_input_bridge_impl.h',
        'arc/settings/arc_settings_bridge.cc',
        'arc/settings/arc_settings_bridge.h',
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
