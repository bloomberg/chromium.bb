# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # File lists shared with GN build.
    'media_router_sources': [
      'create_session_request.cc',
      'create_session_request.h',
      'issue.cc',
      'issue.h',
      'issue_manager.cc',
      'issue_manager.h',
      'issues_observer.h',
      'media_route.cc',
      'media_route.h',
      'media_route_id.h',
      'media_router.h',
      'media_router_type_converters.cc',
      'media_router_type_converters.h',
      'media_routes_observer.cc',
      'media_routes_observer.h',
      'media_sink.cc',
      'media_sink.h',
      'media_sinks_observer.cc',
      'media_sinks_observer.h',
      'media_source.cc',
      'media_source.h',
      'media_source_helper.cc',
      'media_source_helper.h',
      'presentation_media_sinks_observer.cc',
      'presentation_media_sinks_observer.h',
    ],
    'media_router_test_support_sources': [
      'mock_media_router.cc',
      'mock_media_router.h',
      'mock_screen_availability_listener.cc',
      'mock_screen_availability_listener.h',
    ],
  },
}
