# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Build an image."""

from __future__ import print_function

from chromite.cli import command
from chromite.lib import blueprint_lib
from chromite.lib import brick_lib
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import image_lib


IMAGE_TYPES = ['base', 'dev', 'test', 'factory_test', 'factory_install']


@command.CommandDecorator('image')
class ImageCommand(command.CliCommand):
  """Create an image

  Creates an image from the specified board.
  """
  @classmethod
  def AddParser(cls, parser):
    super(ImageCommand, cls).AddParser(parser)
    parser.set_defaults(usage='Create an image')
    target = parser.add_mutually_exclusive_group()
    target.add_argument('--blueprint', type='blueprint_path',
                        help="The blueprint that defines an image "
                        "specification to build.")
    target.add_argument('--board', help="The board to build an image for.")
    target.add_argument('--brick', type='brick_path',
                        help='The brick to build an image for.')
    parser.add_argument('--adjust_part', help="Adjustments to apply to "
                        "partition table (LABEL:[+-=]SIZE) e.g. ROOT-A:+1G.")
    parser.add_argument('--boot_args', help="Additional boot arguments to pass "
                        "to the commandline")
    parser.add_argument('--enable_bootcache', default=False, type='bool',
                        help="Default all bootloaders to use boot cache.")
    parser.add_argument('--enable_rootfs_verification', default=True,
                        type='bool', help="Default all bootloaders to use "
                        "kernel-based root fs integrity checking.")
    parser.add_argument('--output_root', default='//build/images',
                        help="Directory in which to place image result "
                        "directories (named by version)")
    parser.add_argument('--disk_layout',
                        help="The disk layout type to use for this image.")
    parser.add_argument('--enable_serial', help="Enable serial port for "
                        "printk() calls. Example values: ttyS0")
    parser.add_argument('--kernel_log_level', default=7, type=int,
                        help="The log level to add to the kernel command line.")
    parser.add_argument('image_types', nargs='*', choices=IMAGE_TYPES,
                        default='test', help="The image types to build.")

  def Run(self):
    commandline.RunInsideChroot(self)

    # argparse does not behave well with nargs='*', default, and choices
    # (http://bugs.python.org/issue9625). Before freezing options, we need to
    # check if image_types is a string rather than an array.  If it is, then
    # no argument was supplied and the default value is being used, so convert
    # it into an array.  Doing this will:
    # * Allow --help to parse correctly, which it won't do when adding ['test']
    #   as one of the choices.
    # * ensure image_types is always a list.
    if isinstance(self.options.image_types, str):
      self.options.image_types = [self.options.image_types]

    self.options.Freeze()

    board = None
    packages = None
    app_id = None

    if self.options.blueprint:
      blueprint = blueprint_lib.Blueprint(self.options.blueprint)
      packages = blueprint.GetPackages(with_implicit=False)
      #TODO(stevefung): Support multiple sysroots (brbug.com/676)
      board = blueprint.FriendlyName()
      app_id = blueprint.GetAppId()
    elif self.options.brick:
      brick = brick_lib.Brick(self.options.brick)
      packages = brick.MainPackages()
      board = brick.FriendlyName()
    elif self.options.board:
      board = self.options.board

    try:
      image_lib.BuildImage(
          board,
          adjust_part=self.options.adjust_part,
          app_id=app_id,
          boot_args=self.options.boot_args,
          enable_bootcache=self.options.enable_bootcache,
          enable_rootfs_verification=self.options.enable_rootfs_verification,
          output_root=self.options.output_root,
          disk_layout=self.options.disk_layout,
          enable_serial=self.options.enable_serial,
          kernel_log_level=self.options.kernel_log_level,
          packages=packages,
          image_types=self.options.image_types)
    except image_lib.AppIdError:
      cros_build_lib.Die('Invalid field \'%s\' in blueprint %s: %s.  It should '
                         'be a UUID in the canonical {8-4-4-4-12} format, e.g. '
                         '{01234567-89AB-CDEF-0123-456789ABCDEF}.' %
                         (blueprint_lib.APP_ID_FIELD,
                          self.options.blueprint,
                          app_id))
