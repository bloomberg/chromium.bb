# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test the partition_lib module."""

from __future__ import print_function

import os

from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import image_lib
from chromite.lib import image_lib_unittest

from chromite.lib.paygen import paygen_stateful_payload_lib


class GenerateStatefulPayloadTest(cros_test_lib.RunCommandTempDirTestCase):
  """Tests generating correct stateful payload."""

  def testGenerateStatefulPayload(self):
    """Test correct arguments propagated."""
    self.PatchObject(image_lib, 'LoopbackPartitions',
                     return_value=image_lib_unittest.LoopbackPartitionsMock(
                         'image', self.tempdir))
    create_tarball_mock = self.PatchObject(cros_build_lib, 'CreateTarball')

    paygen_stateful_payload_lib.GenerateStatefulPayload('foo-image',
                                                        self.tempdir)

    create_tarball_mock.assert_called_once_with(
        os.path.join(self.tempdir, 'stateful.tgz'), '.', sudo=True,
        compression=cros_build_lib.COMP_GZIP,
        inputs=('dev_image', 'var_overlay'),
        extra_args=['--directory=%s' % os.path.join(self.tempdir, 'dir-1'),
                    '--hard-dereference',
                    '--transform=s,^dev_image,dev_image_new,',
                    '--transform=s,^var_overlay,var_new,'])
