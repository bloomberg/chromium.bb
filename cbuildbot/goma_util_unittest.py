# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for upload_goma_info.py"""

from __future__ import print_function

import collections
import datetime
import json
import os

from chromite.cbuildbot import goma_util
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import gs
from chromite.lib import osutils


class TestGomaLogUploader(cros_test_lib.MockTempDirTestCase):
  """Tests for upload_goma_info."""

  def _CreateLogFile(self, name, timestamp):
    path = os.path.join(
        self.tempdir,
        '%s.host.log.INFO.%s' % (name, timestamp.strftime('%Y%m%d-%H%M%S.%f')))
    osutils.WriteFile(
        path,
        timestamp.strftime('Log file created at: %Y/%m/%d %H:%M:%S'))

  def testUpload(self):
    self._CreateLogFile(
        'compiler_proxy', datetime.datetime(2017, 4, 26, 12, 0, 0))
    self._CreateLogFile(
        'compiler_proxy-subproc', datetime.datetime(2017, 4, 26, 12, 0, 0))
    self._CreateLogFile(
        'gomacc', datetime.datetime(2017, 4, 26, 12, 1, 0))
    self._CreateLogFile(
        'gomacc', datetime.datetime(2017, 4, 26, 12, 2, 0))

    # Set env vars.
    os.environ.update({
        'GLOG_log_dir': self.tempdir,
        'BUILDBOT_BUILDERNAME': 'builder-name',
        'BUILDBOT_MASTERNAME': 'master-name',
        'BUILDBOT_SLAVENAME': 'slave-name',
        'BUILDBOT_CLOBBER': '1',
    })

    self.PatchObject(cros_build_lib, 'GetHostName', lambda: 'dummy-host-name')
    copy_log = []
    self.PatchObject(
        gs.GSContext, 'CopyInto',
        lambda _, __, remote_dir, filename=None, **kwargs: copy_log.append(
            (remote_dir, filename, kwargs.get('headers'))))

    goma_util.GomaLogUploader(self.tempdir, today=datetime.date(2017, 4, 26),
                              dry_run=True).Upload()

    expect_builderinfo = json.dumps(collections.OrderedDict([
        ('builder', 'builder-name'),
        ('master', 'master-name'),
        ('slave', 'slave-name'),
        ('clobber', True),
        ('os', 'chromeos'),
    ]))
    self.assertEqual(
        copy_log,
        [('gs://chrome-goma-log/2017/04/26/dummy-host-name',
          'compiler_proxy-subproc.host.log.INFO.20170426-120000.000000.gz',
          ['x-goog-meta-builderinfo:' + expect_builderinfo]),
         ('gs://chrome-goma-log/2017/04/26/dummy-host-name',
          'compiler_proxy.host.log.INFO.20170426-120000.000000.gz',
          ['x-goog-meta-builderinfo:' + expect_builderinfo]),
        ])
