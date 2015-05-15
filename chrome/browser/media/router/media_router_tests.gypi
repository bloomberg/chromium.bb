# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'sources': [
    '../../ui/webui/media_router/media_cast_mode_unittest.cc',
    '../../ui/webui/media_router/media_router_test.cc',
    '../../ui/webui/media_router/media_router_test.h',
    '../../ui/webui/media_router/media_router_dialog_controller_unittest.cc',
    'issue_manager_unittest.cc',
    'issue_unittest.cc',
    'media_route_unittest.cc',
    'media_router_type_converters_unittest.cc',
    'media_sink_unittest.cc',
    'media_source_helper_unittest.cc',
    'media_source_unittest.cc',
    "presentation_media_sinks_observer_unittest.cc",
    'route_id_manager_unittest.cc',
  ],
  'dependencies': [
    'browser/media/router/media_router.gyp:media_router_test_support',
  ],
}
