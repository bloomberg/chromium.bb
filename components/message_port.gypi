# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'message_port',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../mojo/mojo_public.gyp:mojo_message_pump_lib',
        '../mojo/mojo_public.gyp:mojo_cpp_system',
        '../third_party/WebKit/public/blink.gyp:blink',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'message_port/web_message_port_channel_impl.cc',
        'message_port/web_message_port_channel_impl.h',
      ],
    },
  ],
}
