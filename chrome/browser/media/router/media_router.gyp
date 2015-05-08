# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'media_router',
      'type': 'static_library',
      'include_dirs': [
        '<(DEPTH)',
        '<(DEPTH)/third_party/mojo/src',
      ],
      'dependencies': [
        ':media_router_mojo',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/url/url.gyp:url_lib',
      ],
      'sources': [
        'create_session_request.cc',
        'create_session_request.h',
        'issue.cc',
        'issue.h',
        'issue_manager.cc',
        'issue_manager.h',
        'issue_observer.h',
        'media_route.cc',
        'media_route.h',
        'media_route_id.h',
        'media_router.h',
        'media_router_impl.cc',
        'media_router_impl.h',
        'media_router_impl_factory.cc',
        'media_router_impl_factory.h',
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
        'route_id_manager.cc',
        'route_id_manager.h',
      ],
    },
    {
      # Mojo bindings for the Media Router internal API.
      'target_name': 'media_router_mojo_gen',
      'type': 'none',
      'sources': [
        'media_router.mojom',
      ],
      'includes': [
        '../../../../third_party/mojo/mojom_bindings_generator.gypi',
      ],
    },
    {
      'target_name': 'media_router_mojo',
      'type': 'static_library',
      'include_dirs': [
        '<(DEPTH)/third_party/mojo/src',
      ],
      'dependencies': [
        'media_router_mojo_gen',
        '<(DEPTH)/mojo/mojo_base.gyp:mojo_common_lib',
        '<(DEPTH)/mojo/mojo_base.gyp:mojo_environment_chromium',
        '<(DEPTH)/third_party/mojo/mojo_public.gyp:mojo_cpp_bindings',
      ],
      'sources': [
        '<(SHARED_INTERMEDIATE_DIR)/chrome/browser/media/router/media_router.mojom.cc',
        '<(SHARED_INTERMEDIATE_DIR)/chrome/browser/media/router/media_router.mojom.h',
      ],
    },
    {
      'target_name': 'media_router_test_support',
      'type': 'static_library',
      'include_dirs': [
        '<(DEPTH)',
      ],
      'dependencies': [
        ':media_router',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/testing/gmock.gyp:gmock',
      ],
      'sources': [
        'mock_media_router.cc',
        'mock_media_router.h',
        'mock_screen_availability_listener.cc',
        'mock_screen_availability_listener.h',
      ],
    },

  ],
}
