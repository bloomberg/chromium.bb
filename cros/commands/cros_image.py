# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module is currently an example Cros command."""

from chromite.lib import cros_build_lib

from chromite import cros


@cros.CommandDecorator('image')
class ImageCommand(cros.CrosCommand):
  """Currently an example command.

  For more information see cros.CrosCommand.
  """
  @classmethod
  def AddParser(cls, parser):
    super(ImageCommand, cls).AddParser(parser)
    parser.set_defaults(usage='Example image help')
    parser.add_argument('--myoption', help='Example option')

  def Run(self):
    cros_build_lib.Info('My options are %r', self.options)
