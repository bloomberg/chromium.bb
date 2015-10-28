# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'media_router_integration_test_resources': [
      'resources/basic_test.html',
      'resources/common.js',
      'resources/fail_create_route.json',
      'resources/fail_reconnect_session.html',
      'resources/fail_reconnect_session.json',
      'resources/no_provider.json',
      'resources/route_creation_timed_out.json',
    ],
  }, # end of variables
  'targets': [
    {
      'target_name': 'media_router_integration_test_files',
      'type': 'none',
      'variables': {
        'output_dir': '<(PRODUCT_DIR)/media_router/browser_test_resources',
        'resource_files': [
          '<@(media_router_integration_test_resources)',
        ]
      },
      'copies': [
        {
          'destination': '<(output_dir)',
          'files': [
            '<@(resource_files)',
          ],
        },
      ],
    },  # end of target 'media_router_integration_test_files'
  ],  # end of targets
}
