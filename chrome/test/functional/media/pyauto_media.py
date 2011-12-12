# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""PyAuto media test base.  Handles PyAuto initialization and path setup.

Required to ensure each media test can load the appropriate libraries.  Each
test must include this snippet:

    # This should be at the top
    import pyauto_media

    <test code>

    # This should be at the bottom.
    if __name__ == '__main__':
      pyauto_media.Main()
"""

import os
import sys
import tempfile

from media_test_env_names import MediaTestEnvNames


def _SetupPaths():
  """Add paths required for loading PyAuto and other utilities to sys.path."""
  media_dir = os.path.abspath(os.path.dirname(__file__))
  sys.path.append(media_dir)
  sys.path.append(os.path.normpath(os.path.join(media_dir, os.pardir)))

  # Add psutil library path.
  # TODO(dalecurtis): This should only be added for tests which use psutil.
  sys.path.append(os.path.normpath(os.path.join(
      media_dir, os.pardir, os.pardir, os.pardir, os.pardir,
      'third_party', 'psutil')))

  # Setting PYTHONPATH for reference build.
  # TODO(dalecurtis): Don't use env variables, each test can process a command
  # line before passing off control to PyAuto.
  if os.getenv(MediaTestEnvNames.REFERENCE_BUILD_ENV_NAME):
    reference_build_dir = os.getenv(
        MediaTestEnvNames.REFERENCE_BUILD_DIR_ENV_NAME,
        # TODO(imasaki): Change the following default value.
        # Default directory is just for testing so the correct directory
        # must be set in the build script.
        os.path.join(tempfile.gettempdir(), 'chrome-media-test'))
    sys.path.insert(0, reference_build_dir)


_SetupPaths()


import pyauto_functional
Main = pyauto_functional.Main
