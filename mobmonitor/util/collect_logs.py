# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Simple log collection script for Mob* Monitor"""

from __future__ import print_function

import os
import tempfile
import shutil

from chromite.lib import cros_build_lib


TMPDIR_PREFIX = 'moblab_logs_'
LOG_DIRS = {
    'system_logs': '/var/log',
    'autotest_logs': '/usr/local/autotest/logs'
}


def collect_logs():
  tempdir = tempfile.mkdtemp(prefix=TMPDIR_PREFIX)
  os.chmod(tempdir, 0o777)

  for name, path in LOG_DIRS.iteritems():
    if not os.path.exists(path):
      continue
    shutil.copytree(path, os.path.join(tempdir, name))

  cmd = ['mobmoncli', 'GetStatus']
  cros_build_lib.RunCommand(
      cmd,
      log_stdout_to_file=os.path.join(tempdir, 'mobmonitor_getstatus')
  )

  tarball = '%s.tgz' % tempdir
  cros_build_lib.CreateTarball(tarball, tempdir)
  return tarball
