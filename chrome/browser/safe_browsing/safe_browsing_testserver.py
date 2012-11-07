#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wraps the upstream safebrowsing_test_server.py to run in Chrome tests."""

import os
import sys

BASE_DIR = os.path.dirname(os.path.abspath(__file__))

sys.path.append(os.path.join(BASE_DIR, '..', '..', '..', 'net',
                             'tools', 'testserver'))
import testserver_base


class ServerRunner(testserver_base.TestServerRunner):
  """TestServerRunner for safebrowsing_test_server.py."""

  def create_server(self, server_data):
    sys.path.append(os.path.join(BASE_DIR, '..', '..', '..', 'third_party',
                                 'safe_browsing', 'testing'))
    import safebrowsing_test_server
    server = safebrowsing_test_server.SetupServer(
        self.options.data_file, self.options.host, self.options.port,
        opt_enforce_caching=False, opt_validate_database=True)
    print 'Safebrowsing HTTP server started on port %d...' % server.server_port
    server_data['port'] = server.server_port

    return server

  def add_options(self):
    testserver_base.TestServerRunner.add_options(self)
    self.option_parser.add_option('--data-file', dest='data_file',
                                  help='File containing safebrowsing test '
                                  'data and expectations')
    # TODO(mattm): we define an unnecessary --data-dir option because
    # BaseTestServer unconditionally sets --data-dir. This should be removed
    # when BaseTestServer is refactored to not contain all the net/
    # test_server.py specific stuff.
    self.option_parser.add_option('--data-dir', dest='data_dir',
                                  help='unused')


if __name__ == '__main__':
  sys.exit(ServerRunner().main())
