# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'content_common',
      'type': '<(library)',
      'dependencies': [
        '../ipc/ipc.gyp:ipc',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'common/common_param_traits.cc',
        'common/common_param_traits.h',
        'common/content_message_generator.cc',
        'common/content_message_generator.h',
        'common/content_constants.cc',
        'common/content_constants.h',
        'common/content_switches.cc',
        'common/content_switches.h',
        'common/message_router.cc',
        'common/message_router.h',
        'common/notification_details.cc',
        'common/notification_details.h',
        'common/notification_observer.h',
        'common/notification_registrar.cc',
        'common/notification_registrar.h',
        'common/notification_service.cc',
        'common/notification_service.h',
        'common/notification_source.cc',
        'common/notification_source.h',
        'common/notification_type.h',
        'common/p2p_messages.h',
        'common/p2p_sockets.cc',
        'common/p2p_sockets.h',
        'common/resource_dispatcher.cc',
        'common/resource_dispatcher.h',
        'common/resource_messages.h',
        'common/resource_response.cc',
        'common/resource_response.h',
        'common/socket_stream.h',
        'common/socket_stream_messages.h',
      ],
      'conditions': [
        ['OS=="win"', {
          'msvs_guid': '062E9260-304A-4657-A74C-0D3AA1A0A0A4',
        }],
      ],
    },
  ],
}
