#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helper script to perform actions as a super-user on ChromeOS.

Needs to be run with superuser privileges, typically using the
suid_python binary.

Usage:
  sudo python suid_actions.py --action=clean_flimflam
"""

import optparse
import os
import shutil
import sys


class SuidAction(object):
  """Helper to perform some super-user actions on ChromeOS."""

  def _ParseArgs(self):
    parser = optparse.OptionParser()
    parser.add_option(
        '-a', '--action', help='Action to perform.')
    self._options = parser.parse_args()[0]
    if not self._options.action:
      raise RuntimeError('No action specified.')

  def Run(self):
    self._ParseArgs()
    assert os.geteuid() == 0, 'Needs superuser privileges.'
    handler = getattr(self, self._options.action)
    assert handler and callable(handler), \
        'No handler for %s' % self._options.action
    handler()
    return 0

  ## Actions ##

  def CleanFlimflamDir(self):
    """Clean the contents of /home/chronos/user/flimflam."""
    flimflam_dir = '/home/chronos/user/flimflam'
    if not os.path.exists(flimflam_dir):
      return
    for item in os.listdir(flimflam_dir):
      path = os.path.join(flimflam_dir, item)
      if os.path.isdir(path):
        shutil.rmtree(path)
      else:
        os.remove(path)


if __name__ == '__main__':
  sys.exit(SuidAction().Run())
