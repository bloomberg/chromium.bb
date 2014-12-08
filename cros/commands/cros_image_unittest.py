# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module tests the cros image command."""

from __future__ import print_function

import argparse

from chromite.cros.commands import cros_image
from chromite.lib import cros_test_lib


class ImageCommandTest(cros_test_lib.TestCase):
  """Test class for our ImageCommand class."""

  def testParser(self):
    """Tests that our example parser works as expected."""
    fake_parser = argparse.ArgumentParser()
    fake_subparser = fake_parser.add_subparsers()
    image_parser = fake_subparser.add_parser('image')
    cros_image.ImageCommand.AddParser(image_parser)

    options = fake_parser.parse_args(['image', '--myoption=awesome'])
    instance = options.cros_class(options)
    self.assertEqual(instance.options.myoption, 'awesome')
