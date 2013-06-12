# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'browser_context_keyed_service',
      'type': '<(component)',
      'defines': [
        'BROWSER_CONTEXT_KEYED_SERVICE_IMPLEMENTATION',
      ],
      'include_dirs': [
        '..',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:base_prefs',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../content/content.gyp:content_common',
        'user_prefs',
      ],
      'sources': [
        'browser_context_keyed_service/browser_context_keyed_service_export.h',
        'browser_context_keyed_service/browser_context_dependency_manager.cc',
        'browser_context_keyed_service/browser_context_dependency_manager.h',
        'browser_context_keyed_service/browser_context_keyed_base_factory.h',
        'browser_context_keyed_service/browser_context_keyed_base_factory.cc',
        'browser_context_keyed_service/browser_context_keyed_service.h',
        'browser_context_keyed_service/browser_context_keyed_service_factory.cc',
        'browser_context_keyed_service/browser_context_keyed_service_factory.h',
        'browser_context_keyed_service/dependency_graph.cc',
        'browser_context_keyed_service/dependency_graph.h',
        'browser_context_keyed_service/dependency_node.h',
        'browser_context_keyed_service/refcounted_browser_context_keyed_service.cc',
        'browser_context_keyed_service/refcounted_browser_context_keyed_service.h',
        'browser_context_keyed_service/refcounted_browser_context_keyed_service_factory.cc',
        'browser_context_keyed_service/refcounted_browser_context_keyed_service_factory.h',
      ],
    },
  ],
}
