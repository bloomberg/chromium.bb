# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Build an image."""

from __future__ import print_function

import os

from chromite.cbuildbot import constants
from chromite.cli import command
from chromite.lib import blueprint_lib
from chromite.lib import brick_lib
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import operation
from chromite.lib import workspace_lib


IMAGE_TYPES = ['base', 'dev', 'test', 'factory_test', 'factory_install', []]


class BrilloImageOperation(operation.ParallelEmergeOperation):
  """Progress bar for brillo image.

  Since brillo image is a long running operation, we divide it into stages.
  For each stage, we display a different progress bar.
  """
  BASE_STAGE = 'base'
  DEV_STAGE = 'dev'
  TEST_STAGE = 'test'

  def __init__(self):
    super(BrilloImageOperation, self).__init__()
    self._stage_name = None
    self._done = False

  def _StageEnter(self, output):
    """Return stage's name if we are entering a stage, else False."""
    events = ['operation: creating base image',
              'operation: creating developer image',
              'operation: creating test image']
    stages = [self.BASE_STAGE, self.DEV_STAGE, self.TEST_STAGE]
    for event, stage in zip(events, stages):
      if event in output:
        return stage
    return None

  def _StageExit(self, output):
    """Determine if we are exiting a stage."""
    events = ['operation: done creating base image',
              'operation: done creating developer image',
              'operation: done creating test image']
    for event in events:
      if event in output:
        return True
    return False

  def _StageStatus(self, output):
    "Returns stage name if we are entering that stage and if exiting a stage."""
    return self._StageEnter(output), self._StageExit(output)

  def _PrintEnterStageMessages(self):
    """Messages to indicate the start of a new stage.

    As the base image is always created, we display a message then. For the
    other stages, messages are only displayed if those stages will have a
    progress bar.

    Returns:
      A message that is to be displayed before the progress bar is shown (if
      needed). If the progress bar is not shown, then the message should not be
      displayed.
    """
    if self._stage_name == self.BASE_STAGE:
      logging.notice('Creating disk layout')
      return 'Building base image.'
    elif self._stage_name == self.DEV_STAGE:
      return 'Building developer image.'
    else:
      return 'Building test image.'

  def _PrintEndStageMessages(self):
    """Messages to be shown at the end of a stage."""
    logging.notice('Unmounting image. This may take a while.')

  def ParseOutput(self, output=None):
    """Display progress bars for brillo image."""

    stdout = self._stdout.read()
    stderr = self._stderr.read()
    output = stdout + stderr
    stage_name, stage_exit = self._StageStatus(output)

    # If we are in a stage, then we update the progress bar accordingly.
    if self._stage_name is not None and not self._done:
      progress = super(BrilloImageOperation, self).ParseOutput(output)
      # If we are done displaying a progress bar for a stage, then we display
      # progress bar operation (parallel emerge).
      if progress == 1:
        self._done = True
        self.Cleanup()
        # Do not display a 100% progress in exit because it has already been
        # done.
        self._progress_bar_displayed = False
        self._PrintEndStageMessages()

    # Perform cleanup when exiting a stage.
    if stage_exit:
      self._stage_name = None
      self._total = None
      self._done = False
      self._completed = 0
      self._printed_no_packages = False
      self.Cleanup()
      self._progress_bar_displayed = False

    # When entering a stage, print stage appropriate entry messages.
    if stage_name is not None:
      self._stage_name = stage_name
      msg = self._PrintEnterStageMessages()
      self.SetProgressBarMessage(msg)


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
                        default=None, help="The image types to build.")

  def Run(self):
    commandline.RunInsideChroot(self, auto_detect_brick=True)

    self.options.Freeze()

    cmd = [os.path.join(constants.CROSUTILS_DIR, 'build_image')]

    if self.options.blueprint:
      blueprint = blueprint_lib.Blueprint(self.options.blueprint)
      packages = []
      for b in blueprint.GetBricks():
        packages.extend(brick_lib.Brick(b).MainPackages())
      if blueprint.GetBSP():
        packages.extend(brick_lib.Brick(blueprint.GetBSP()).MainPackages())

      cmd.append('--extra_packages=%s' % ' '.join(packages))
      #TODO(stevefung): Support multiple sysroots (brbug.com/676)
      cmd.append('--board=%s' % blueprint.FriendlyName())
    elif self.options.brick:
      brick = brick_lib.Brick(self.options.brick)
      cmd.append('--extra_packages=%s' % ' '.join(brick.MainPackages()))
      cmd.append('--board=%s' % brick.FriendlyName())
    elif self.options.board:
      cmd.append('--board=%s' % self.options.board)

    if self.options.adjust_part:
      cmd.append('--adjust_part=%s' % self.options.adjust_part)

    if self.options.boot_args:
      cmd.append('--boot_args=%s' % self.options.boot_args)

    if self.options.enable_bootcache:
      cmd.append('--enable_bootcache')
    else:
      cmd.append('--noenable_bootcache')

    if self.options.enable_rootfs_verification:
      cmd.append('--enable_rootfs_verification')
    else:
      cmd.append('--noenable_rootfs_verification')

    if self.options.output_root:
      if workspace_lib.IsLocator(self.options.output_root):
        path = workspace_lib.LocatorToPath(self.options.output_root)
      else:
        path = self.options.output_root
      cmd.append('--output_root=%s' % path)

    if self.options.disk_layout:
      cmd.append('--disk_layout=%s' % self.options.disk_layout)

    if self.options.enable_serial:
      cmd.append('--enable_serial=%s' % self.options.enable_serial)

    if self.options.kernel_log_level:
      cmd.append('--loglevel=%s' % self.options.kernel_log_level)

    if self.options.image_types:
      cmd.extend(self.options.image_types)

    if command.UseProgressBar():
      cmd.append('--progress_bar')
      op = BrilloImageOperation()
      op.Run(cros_build_lib.RunCommand, cmd, log_level=logging.DEBUG)
    else:
      cros_build_lib.RunCommand(cmd)
