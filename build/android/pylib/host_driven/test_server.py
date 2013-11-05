# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Host driven test server controller.

This class controls the startup and shutdown of a python driven test server that
runs in a separate process.

The server starts up automatically when the object is created.

After it starts up, it is possible to retreive the hostname it started on
through accessing the member field |host| and the port name through |port|.

For shutting down the server, call TearDown().
"""

import subprocess
import os
import os.path

from pylib import constants

# NOTE: when adding or modifying these lines, omit any leading slashes!
# Otherwise os.path.join() will (correctly) treat them as absolute paths
# instead of relative paths, and will do nothing.
_PYTHONPATH_DIRS = [
    'net/tools/testserver/',
    'third_party/',
    'third_party/pyftpdlib/src/',
    'third_party/pywebsocket/src',
    'third_party/tlslite/',
]

# Python files in these directories are generated as part of the build.
# These dirs are located in out/(Debug|Release) directory.
# The correct path is determined based on the build type. E.g. out/Debug for
# debug builds and out/Release for release builds.
_GENERATED_PYTHONPATH_DIRS = [
    'pyproto/sync/protocol/',
    'pyproto/'
]

_TEST_SERVER_HOST = '127.0.0.1'
# Paths for supported test server executables.
TEST_NET_SERVER_PATH = 'net/tools/testserver/testserver.py'
TEST_SYNC_SERVER_PATH = 'sync/tools/testserver/sync_testserver.py'


class TestServer(object):
  """Sets up a host driven test server on the host machine.

  For shutting down the server, call TearDown().
  """

  def __init__(self, shard_index, test_server_port, test_server_path):
    """Sets up a Python driven test server on the host machine.

    Args:
      shard_index: Index of the current shard.
      test_server_port: Port to run the test server on. This is multiplexed with
                        the shard index. To retrieve the real port access the
                        member variable |port|.
      test_server_path: The path (relative to the root src dir) of the server
    """
    self.host = _TEST_SERVER_HOST
    self.port = test_server_port + shard_index

    src_dir = constants.DIR_SOURCE_ROOT
    # Make dirs into a list of absolute paths.
    abs_dirs = [os.path.join(src_dir, d) for d in _PYTHONPATH_DIRS]
    # Add the generated python files to the path
    abs_dirs.extend([os.path.join(src_dir, constants.GetOutDirectory(), d)
                     for d in _GENERATED_PYTHONPATH_DIRS])
    current_python_path = os.environ.get('PYTHONPATH')
    extra_python_path = ':'.join(abs_dirs)
    if current_python_path:
      python_path = current_python_path + ':' + extra_python_path
    else:
      python_path = extra_python_path

    # NOTE: A separate python process is used to simplify getting the right
    # system path for finding includes.
    cmd = ['python', os.path.join(src_dir, test_server_path),
           '--log-to-console',
           ('--host=%s' % self.host),
           ('--port=%d' % self.port)]
    self._test_server_process = subprocess.Popen(
          cmd, env={'PYTHONPATH': python_path})

  def TearDown(self):
    self._test_server_process.kill()
