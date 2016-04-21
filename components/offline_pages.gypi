# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN: //components/offline_pages:offline_pages
      'target_name': 'offline_pages',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../url/url.gyp:url_lib',
        '../third_party/leveldatabase/leveldatabase.gyp:leveldatabase',
        'components.gyp:leveldb_proto',
        'keyed_service_core',
        'offline_pages_proto',
      ],
      'sources': [
        'offline_pages/offline_page_archiver.h',
	'offline_pages/offline_page_bookmark_bridge.h',
	'offline_pages/offline_page_bookmark_bridge.cc',
        'offline_pages/offline_page_feature.cc',
        'offline_pages/offline_page_feature.h',
        'offline_pages/offline_page_item.cc',
        'offline_pages/offline_page_item.h',
        'offline_pages/offline_page_model.cc',
        'offline_pages/offline_page_model.h',
        'offline_pages/offline_page_metadata_store.cc',
        'offline_pages/offline_page_metadata_store.h',
        'offline_pages/offline_page_metadata_store_impl.cc',
        'offline_pages/offline_page_metadata_store_impl.h',
      ],
    },
    {
      # GN version: //components/offline_pages:test_support
      'target_name': 'offline_pages_test_support',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
        '../url/url.gyp:url_lib',
        'offline_pages',
      ],
      'sources': [
        'offline_pages/offline_page_test_archiver.h',
        'offline_pages/offline_page_test_archiver.cc',
        'offline_pages/offline_page_test_store.h',
        'offline_pages/offline_page_test_store.cc',
      ],
    },
    {
      # Protobuf compiler / generator for the offline page item protocol buffer.
      # GN version: //components/offline_pages/proto
      'target_name': 'offline_pages_proto',
      'type': 'static_library',
      'sources': [ 'offline_pages/proto/offline_pages.proto', ],
      'variables': {
        'proto_in_dir': 'offline_pages/proto',
        'proto_out_dir': 'components/offline_pages/proto',
      },
      'includes': [ '../build/protoc.gypi', ],
    },
  ],
  'conditions': [
    ['OS == "android"', {
      'targets': [
        {
          # GN: //components/offline_pages:offline_page_model_enums_java
          'target_name': 'offline_page_model_enums_java',
          'type': 'none',
          'variables': {
            'source_file': 'offline_pages/offline_page_model.h',
          },
          'includes': [ '../build/android/java_cpp_enum.gypi' ],
        },
      ],
    }],
  ],
}
