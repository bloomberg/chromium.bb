# Copyright (c) 2012 The Chromium Authors. All rights reserved.
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


_SetupPaths()


import pyauto_functional
Main = pyauto_functional.Main
