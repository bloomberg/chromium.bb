# Copyright (c) 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'mojo_variables.gypi',
  ],
  'targets': [
    {
      # GN version: //mojo/converters/input_events
      'target_name': 'mojo_input_events_lib',
      'type': '<(component)',
      'defines': [
        'MOJO_INPUT_EVENTS_IMPLEMENTATION',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../ui/events/events.gyp:events',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        'mojo_geometry_converters.gyp:mojo_geometry_lib',
        'services/public/mojo_services_public.gyp:mojo_geometry_bindings',
        'services/public/mojo_services_public.gyp:mojo_input_events_bindings',
        '<(mojo_system_for_component)',
      ],
      'sources': [
        'converters/input_events/input_events_type_converters.cc',
        'converters/input_events/input_events_type_converters.h',
        'converters/input_events/mojo_extended_key_event_data.cc',
        'converters/input_events/mojo_extended_key_event_data.h',
        'converters/input_events/mojo_input_events_export.h',
      ],
      'conditions': [
        ['component=="shared_library"', {
          'dependencies': [
            'mojo_base.gyp:mojo_environment_chromium',
          ],
        }],
      ],
    },
  ],
}
