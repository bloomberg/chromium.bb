# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library for handling Chrome OS partition."""

from __future__ import print_function

import os

from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import dev_server_wrapper
from chromite.lib import image_lib
from chromite.lib import osutils


_STATEFUL_FILE = 'stateful.tgz'


def GenerateStatefulPayload(image_path, output_directory):
  """Generates a stateful update payload given a full path to an image.

  Args:
    image_path: Full path to the image.
    output_directory: Path to the directory to leave the resulting output.
    logging: logging instance.
  """
  logging.info('Generating stateful payload file from %s', image_path)

  stateful_file = os.path.join(output_directory,
                               dev_server_wrapper.STATEFUL_FILENAME)

  with osutils.TempDir() as temp_dir, \
      image_lib.LoopbackPartitions(image_path, temp_dir) as image:
    stateful_dir = image.Mount((constants.PART_STATE,))[0]
    cros_build_lib.CreateTarball(
        stateful_file, '.', sudo=True, compression=cros_build_lib.COMP_GZIP,
        inputs=('dev_image', 'var_overlay'),
        extra_args=['--directory=%s' % stateful_dir,
                    '--hard-dereference',
                    '--transform=s,^dev_image,dev_image_new,',
                    '--transform=s,^var_overlay,var_new,'])

  logging.info('Successfully generated stateful payload %s', stateful_file)
