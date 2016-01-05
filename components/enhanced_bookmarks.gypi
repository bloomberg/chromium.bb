# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN: //components/enhanced_bookmarks:enhanced_bookmarks
      'target_name': 'enhanced_bookmarks',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../google_apis/google_apis.gyp:google_apis',
        '../net/net.gyp:net',
        '../skia/skia.gyp:skia',
        '../sql/sql.gyp:sql',
        '../ui/gfx/gfx.gyp:gfx',
        '../url/url.gyp:url_lib',
        'bookmarks_browser',
        'enhanced_bookmarks_proto',
        'keyed_service_core',
        'signin_core_browser',
        'sync_driver',
        'variations',
      ],
      'sources': [
        'enhanced_bookmarks/bookmark_server_cluster_service.cc',
        'enhanced_bookmarks/bookmark_server_cluster_service.h',
        'enhanced_bookmarks/bookmark_server_service.cc',
        'enhanced_bookmarks/bookmark_server_service.h',
        'enhanced_bookmarks/enhanced_bookmark_model.cc',
        'enhanced_bookmarks/enhanced_bookmark_model.h',
        'enhanced_bookmarks/enhanced_bookmark_model_observer.h',
        'enhanced_bookmarks/enhanced_bookmark_switches_ios.cc',
        'enhanced_bookmarks/enhanced_bookmark_switches_ios.h',
        'enhanced_bookmarks/enhanced_bookmark_utils.cc',
        'enhanced_bookmarks/enhanced_bookmark_utils.h',
        'enhanced_bookmarks/image_record.cc',
        'enhanced_bookmarks/image_record.h',
        'enhanced_bookmarks/image_store.cc',
        'enhanced_bookmarks/image_store.h',
        'enhanced_bookmarks/image_store_util.cc',
        'enhanced_bookmarks/image_store_util.h',
        'enhanced_bookmarks/image_store_util_ios.mm',
        'enhanced_bookmarks/item_position.cc',
        'enhanced_bookmarks/item_position.h',
        'enhanced_bookmarks/metadata_accessor.cc',
        'enhanced_bookmarks/metadata_accessor.h',
        'enhanced_bookmarks/persistent_image_store.cc',
        'enhanced_bookmarks/persistent_image_store.h',
        'enhanced_bookmarks/pref_names.cc',
        'enhanced_bookmarks/pref_names.h',
      ],
      'conditions': [
        ['OS=="ios"', {
          'sources!': [
            'enhanced_bookmarks/image_store_util.cc',
          ],
        }],
      ],
    },
    {
      # GN: //components/enhanced_bookmarks:enhanced_bookmarks_test_support
      'target_name': 'enhanced_bookmarks_test_support',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
        'enhanced_bookmarks',
      ],
      'sources': [
        'enhanced_bookmarks/test_image_store.cc',
        'enhanced_bookmarks/test_image_store.h',
      ],
    },
    {
      'target_name': 'enhanced_bookmarks_proto',
      'type': 'static_library',
      'sources': [
        'enhanced_bookmarks/proto/cluster.proto',
        'enhanced_bookmarks/proto/metadata.proto',
        'enhanced_bookmarks/proto/search.proto',
      ],
      'variables': {
        'proto_in_dir': './enhanced_bookmarks/proto',
        'proto_out_dir': 'components/enhanced_bookmarks/proto',
      },
      'includes': [ '../build/protoc.gypi' ],
    },
  ],
  'conditions' : [
    ['OS=="android"', {
      'targets': [
        {
          # GN: //components/enhanced_bookmarks:enhanced_bookmarks_java_enums_srcjar
          'target_name': 'enhanced_bookmarks_java_enums_srcjar',
          'type': 'none',
          'variables': {
            'source_file': 'enhanced_bookmarks/enhanced_bookmark_utils.h',
          },
          'includes': [ '../build/android/java_cpp_enum.gypi' ],
        },
      ],
     },
   ],
  ],
}
