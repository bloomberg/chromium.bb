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
        '../base/base.gyp:base',
        '../chromeos/chromeos.gyp:chromeos',
        '../ipc/ipc.gyp:ipc',
      ],
      'sources': [
        'arc/arc_bridge_service.cc',
        'arc/arc_bridge_service.h',
        'arc/arc_bridge_service_impl.cc',
        'arc/arc_bridge_service_impl.h',
        'arc/arc_service_manager.cc',
        'arc/arc_service_manager.h',
        'arc/common/arc_host_messages.h',
        'arc/common/arc_instance_messages.h',
        'arc/common/arc_message_generator.cc',
        'arc/common/arc_message_generator.h',
        'arc/common/arc_message_traits.h',
        'arc/common/arc_message_types.h',
        'arc/common/arc_notification_traits.h',
        'arc/common/arc_notification_types.h',
        'arc/common/arc_notification_types.cc',
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
        'arc/test/fake_arc_bridge_service.cc',
        'arc/test/fake_arc_bridge_service.h',
      ],
    },
  ],
}
