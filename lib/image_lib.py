# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utilities for manipulating ChromeOS images."""

from __future__ import print_function

import glob
import os
import re
import sys

from chromite.cbuildbot import constants
from chromite.cli import command
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import operation
from chromite.lib import osutils
from chromite.lib import workspace_lib


class LoopbackError(Exception):
  """An exception raised when something went wrong setting up a loopback"""


class LoopbackPartitions(object):
  """Loopback mount a file and provide access to its partitions.

  This class can be used as a context manager with the "with" statement, or
  individual instances of it can be created which will clean themselves up
  when garbage collected or when explicitly closed, ala the tempfile module.

  In either case, the same arguments should be passed to init.

  Args:
    path: Path to the backing file.
    losetup: Path to the losetup utility. The default uses the system PATH.
  """
  def __init__(self, path, util_path=None):
    self._util_path = util_path
    self.path = path
    self.dev = None
    self.parts = {}

    try:
      cmd = ['losetup', '--show', '-f', self.path]
      ret = self._au_safe_sudo(cmd, print_cmd=False, capture_output=True)
      self.dev = ret.output.strip()
      cmd = ['partx', '-d', self.dev]
      self._au_safe_sudo(cmd, quiet=True, error_code_ok=True)
      cmd = ['partx', '-a', self.dev]
      self._au_safe_sudo(cmd, print_cmd=False)

      self.parts = {}
      part_devs = glob.glob(self.dev + 'p*')
      if not part_devs:
        logging.Warning('Didn\'t find partition devices nodes for %s.',
                        self.path)
        return

      for part in part_devs:
        number = int(re.search(r'p(\d+)$', part).group(1))
        self.parts[number] = part

    except:
      self.close()
      raise

  def _au_safe_sudo(self, cmd, **kwargs):
    """Run a command using sudo in a way that respects the util_path"""
    newcmd = osutils.Which(cmd[0], path=self._util_path)
    if newcmd:
      cmd = [newcmd] + cmd[1:]
    return cros_build_lib.SudoRunCommand(cmd, **kwargs)


  def close(self):
    if self.dev:
      cmd = ['partx', '-d', self.dev]
      self._au_safe_sudo(cmd, quiet=True, error_code_ok=True)
      self._au_safe_sudo(['losetup', '--detach', self.dev])
      self.dev = None
      self.parts = {}

  def __enter__(self):
    return self

  def __exit__(self, exc_type, exc, tb):
    self.close()

  def __del__(self):
    self.close()


class BrilloImageOperation(operation.ParallelEmergeOperation):
  """Progress bar for brillo image.

  Since brillo image is a long running operation, we divide it into stages.
  For each stage, we display a different progress bar.
  """
  BASE_STAGE = 'base'
  DEV_STAGE = 'dev'
  TEST_STAGE = 'test'
  SUMMARIZE_STAGE = 'summarize'
  SUMMARIZE_STAGE_START = 'operation: summarize'
  SUMMARIZE_STAGE_STOP = 'operation: done summarize'

  def __init__(self):
    super(BrilloImageOperation, self).__init__()
    self._stage_name = None
    self._done = False
    self._enable_print = False

  def _StageEnter(self, output):
    """Return stage's name if we are entering a stage, else False."""
    events = [self.SUMMARIZE_STAGE_START,
              'operation: creating test image',
              'operation: creating developer image',
              'operation: creating base image']
    stages = [self.SUMMARIZE_STAGE, self.TEST_STAGE, self.DEV_STAGE,
              self.BASE_STAGE]
    for event, stage in zip(events, stages):
      if event in output:
        return stage
    return None

  def _StageExit(self, output):
    """Determine if we are exiting a stage."""
    events = ['operation: done creating base image',
              'operation: done creating developer image',
              'operation: done creating test image',
              self.SUMMARIZE_STAGE_STOP]
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

    # If we are in a non-summarize stage, then we update the progress bar
    # accordingly.
    if (self._stage_name is not None and
        self._stage_name != self.SUMMARIZE_STAGE and not self._done):
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
      if self._stage_name == self.SUMMARIZE_STAGE:
        sys.stdout.write('\n')

    # If we are in a summarize stage, properly format and display the output.
    if self._stage_name == self.SUMMARIZE_STAGE and not self._done:
      summarize_stage_prefix = 'INFO    : '
      for line in output.split('\n'):
        if self.SUMMARIZE_STAGE_START in line:
          self._enable_print = True
          continue
        if self.SUMMARIZE_STAGE_STOP in line:
          self._enable_print = False
          break
        if summarize_stage_prefix in line and self._enable_print:
          line = line.replace(summarize_stage_prefix, '')
          logging.notice(line)

  def Run(self, func, *args, **kwargs):
    """Run function for BrilloImageOperation."""
    try:
      super(BrilloImageOperation, self).Run(func, *args, **kwargs)
    except:
      logging.error('The output directory has been automatically deleted. '
                    'To keep it around, please re-run the command with '
                    '--log-level info.')
      raise


class AppIdError(Exception):
  """Thrown when attempting to install an invalid app_id."""


def VerifyAppId(app_id):
  """Verifies the APP_ID is in the correct format.

  An APP_ID is a UUID in the canonical {8-4-4-4-12} format, e.g.
  {01234567-89AB-CDEF-0123-456789ABCDEF}.
  """
  pattern = re.compile("[{][0-9A-F]{8}-([0-9A-F]{4}-){3}[0-9A-F]{12}[}]")
  return pattern.match(app_id)


def WriteLsbRelease(sysroot, fields):
  """Writes out the /etc/lsb-release file into the given sysroot.

  Args:
    sysroot: The sysroot to write the lsb-release file to.
    fields: A dictionary of all the fields and values to write.
  """
  content = '\n'.join('%s=%s' % (k, v) for k, v in fields.items()) + '\n'

  path = os.path.join(sysroot, constants.LSB_RELEASE_PATH.lstrip('/'))

  if os.path.exists(path):
    # The file has already been pre-populated with some fields.  Since
    # osutils.WriteFile(..) doesn't support appending with sudo, read in the
    # content and prepend it to the new content to write.
    # TODO(stevefung): Remove this appending, once all writing to the
    #   /etc/lsb-release file has been removed from ebuilds and consolidated
    #  to the buid tools.
    content = osutils.ReadFile(path) + content

  osutils.WriteFile(path, content, mode='w', makedirs=True, sudo=True)


def BuildImage(board, adjust_part=None, app_id=None, boot_args=None,
               enable_bootcache=False, enable_rootfs_verification=True,
               output_root=None, disk_layout=None, enable_serial=None,
               kernel_log_level=None, packages=None, image_types=None):
  """Build an image.

  Args:
    board: The board to build an image for.
    adjust_part: Adjustments to apply to partition table
      (LABEL:[+-=]SIZE) e.g. ROOT-A:+1G.
    app_id: The application ID to install into the image's /etc/lsb-release
      file.  It is a UUID in the canonical {8-4-4-4-12} format, e.g.
      {01234567-89AB-CDEF-0123-456789ABCDEF}
    boot_args: Additional boot arguments to pass to the commandline.
    enable_bootcache: Default all bootloaders to use boot cache.
    enable_rootfs_verification: Default all bootloaders to use kernel-based
      root fs integrity checking.
    output_root: Directory in which to place image result directories
      (named by version).
    disk_layout: The disk layout type to use for this image.
    enable_serial: Enable serial port for printk() calls. Example values: ttyS0
    kernel_log_level: The log level to add to the kernel command line.
    packages: Any additional packages outside of the virtual/target-os
      dependency tree to install into the image.
    image_types: The image types to build.  If None, builds a base image.
  """
  cmd = [os.path.join(constants.CROSUTILS_DIR, 'build_image')]

  if app_id:
    if VerifyAppId(app_id):
      cmd.append('--app_id=%s' % app_id)
    else:
      raise AppIdError('Invalid APP_ID format: %s' % app_id)

  cmd.append('--board=%s' % board)

  if packages:
    cmd.append('--extra_packages=%s' % ' '.join(packages))

  if adjust_part:
    cmd.append('--adjust_part=%s' % adjust_part)

  if boot_args:
    cmd.append('--boot_args=%s' % boot_args)

  if enable_bootcache:
    cmd.append('--enable_bootcache')
  else:
    cmd.append('--noenable_bootcache')

  if enable_rootfs_verification:
    cmd.append('--enable_rootfs_verification')
  else:
    cmd.append('--noenable_rootfs_verification')

  if output_root:
    if workspace_lib.IsLocator(output_root):
      path = workspace_lib.LocatorToPath(output_root)
    else:
      path = output_root
    cmd.append('--output_root=%s' % path)

  if disk_layout:
    cmd.append('--disk_layout=%s' % disk_layout)

  if enable_serial:
    cmd.append('--enable_serial=%s' % enable_serial)

  if kernel_log_level:
    cmd.append('--loglevel=%s' % kernel_log_level)

  if image_types:
    cmd.extend(image_types)

  if command.GetToolset() == 'brillo':
    cmd.append('--toolset=brillo')

  if command.UseProgressBar():
    cmd.append('--progress_bar')
    op = BrilloImageOperation()
    op.Run(cros_build_lib.RunCommand, cmd, log_level=logging.DEBUG)
  else:
    cros_build_lib.RunCommand(cmd)
