# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Stage the image on a Moblab device."""

from __future__ import print_function

import httplib
import logging
import os
import urllib2

from chromite import cros
from chromite.cbuildbot import commands, constants
from chromite.lib import cros_build_lib
from chromite.lib import dev_server_wrapper
from chromite.lib import remote_access
from chromite.lib import osutils


DEVSERVER_STATIC_DIR = cros_build_lib.FromChrootPath(
    os.path.join(constants.CHROOT_SOURCE_ROOT, 'devserver', 'static'))
MOBLAB_TMP_DIR = '/mnt/moblab/static/tmp'
BOARD_BUILD_DIR = 'usr/local/build'
DEVSERVER_STAGE_URL = ('http://%(moblab)s:8080/stage?local_path=%(staged_dir)s'
                       '&artifacts=full_payload,stateful,test_suites,'
                       'control_files,autotest_packages')


class CustomImageStagingException(Exception):
  """Thrown when there is an error staging an custom image on Moblab"""


@cros.CommandDecorator('stage-on-moblab')
class StageOnMoblabCommand(cros.CrosCommand):
  """Converts a locally generated image so that it can be staged on a Moblab.

  Moblab can only reimage and test devices if the specified image has:
    * stateful.tgz
    * update.gz
    * test_suites.tar.bz2
    * autotest_packages.tar
    * control_files.tar

  Images made in the chroot lack these artifacts and the payloads need to be
  generated.

  Once generated, the artifacts need to be simply copied over to the Moblab's
  devserver's static directory.

  The image name to then use for testing is output at the end of this
  script.
  """

  EPILOG = """
To stage a local image path onto a moblab device:
  cros stage-on-moblab <moblab> /path/to/board/build/chromiumos-test-image.bin

NOTES:
* The autotest bits used to test this image will be the latest in your
  build sysroot! I.E. if you emerge new autotest changes after producing the
  image you wish to stage, there is a chance that the changes will not match.
* The custom image will only stay on the Moblab device for 24 hours at which
  point it will be wiped.
"""

  @classmethod
  def AddParser(cls, parser):
    """Add parser arguments."""
    super(StageOnMoblabCommand, cls).AddParser(parser)
    parser.add_argument(
        'moblab', help='ssh://device_hostname[:port] or usb://{device_path}. '
        'If no device_path is given (i.e. usb://), user will be prompted to '
        'choose from a list of removable devices.')
    parser.add_argument(
        'image', nargs='?', default='latest', help='Path to image we want to '
        'stage. It should be in the format of '
        '/.../.../board/build/<image>.bin')

  def __init__(self, options):
    """Initializes cros stage-on-moblab."""
    cros.CrosCommand.__init__(self, options)
    self.board = None
    self.staged_image_name = self._GenerateImageName(self.options.image)
    self.stage_directory = os.path.join(MOBLAB_TMP_DIR, self.staged_image_name)

  def _GenerateImageName(self, image):
    """Generate the name as which |image| will be staged onto Moblab.

    Args:
      image: Path to image we want to stage. It should be in the format of
             /.../.../board/build/<image>.bin

    Returns:
      Name the image will be staged as.

    Raises:
      CustomImageStagingException: If the image name supplied is not valid.
    """
    realpath = osutils.ExpandPath(image)
    if not realpath.endswith('.bin'):
      raise CustomImageStagingException(
          'Image path: %s does not end in .bin !' % realpath)
    build_name = os.path.basename(os.path.dirname(realpath))
    # Custom builds are name with the suffix of '-a1' but the build itself
    # is missing this suffix in its filesystem. Therefore lets rename the build
    # name to match the name inside the build.
    if build_name.endswith('-a1'):
      build_name = build_name[:-len('-a1')]

    self.board = os.path.basename(os.path.dirname(os.path.dirname(realpath)))
    return '%s-custom/%s' % (self.board, build_name)

  def _GeneratePayloads(self, device, tempdir):
    """Generate the Payloads we require and copy them to the Moblab.

    Args:
      device: ChromiumOSDevice instance of the Moblab host.
      tempdir: Temporary Directory to store the generated payloads.
    """
    dev_server_wrapper.GetUpdatePayloadsFromLocalPath(
        self.options.image, tempdir, static_dir=DEVSERVER_STATIC_DIR)
    rootfs_payload = os.path.join(tempdir, dev_server_wrapper.ROOTFS_FILENAME)
    stateful_payload = os.path.join(tempdir,
                                    dev_server_wrapper.STATEFUL_FILENAME)
    device.CopyToDevice(stateful_payload, self.stage_directory)
    device.CopyToDevice(rootfs_payload, os.path.join(self.stage_directory,
                                                     'update_full_dev.bin'))

  def _GenerateTestBits(self, device, tempdir):
    """Generate and transfer to the Moblab the test bits we require.

    Args:
      device: ChromiumOSDevice instance of the Moblab host.
      tempdir: Temporary Directory to store the generated test artifacts.
    """
    build_root = cros_build_lib.GetSysroot(board=self.board)
    cwd = os.path.join(build_root, BOARD_BUILD_DIR)
    tarball_funcs = [commands.BuildAutotestControlFilesTarball,
                     commands.BuildAutotestPackagesTarball,
                     commands.BuildAutotestTestSuitesTarball]
    for tarball_func in tarball_funcs:
      tarball_path = tarball_func(build_root, cwd, tempdir)
      device.CopyToDevice(tarball_path, self.stage_directory)

  def Run(self):
    """Perform the cros stage-on-moblab command."""
    logging.info('Attempting to stage: %s on Moblab: %s as Image: %s',
                 self.options.image, self.options.moblab,
                 self.staged_image_name)
    with remote_access.ChromiumOSDeviceHandler(
        self.options.moblab) as device:
      device.RunCommand(['mkdir', '-p', self.stage_directory])
      with osutils.TempDir() as tempdir:
        self._GeneratePayloads(device, tempdir)
        self._GenerateTestBits(device, tempdir)
      device.RunCommand(['chown', '-R', 'moblab:moblab',
                         MOBLAB_TMP_DIR])
      try:
        stage_url = DEVSERVER_STAGE_URL % dict(moblab=self.options.moblab,
                                               staged_dir=self.stage_directory)
        res = urllib2.urlopen(stage_url).read()
      except (urllib2.HTTPError, httplib.HTTPException, urllib2.URLError) as e:
        logging.error('Unable to stage artifacts on moblab. Error: %s', e)
      else:
        if res == 'Success':
          logging.info('\n\nStaging Completed!')
          logging.info('Image is staged on Moblab as %s',
                       self.staged_image_name)
        else:
          logging.info('Staging failed. Error Message: %s', res)
      finally:
        device.RunCommand(['rm', '-rf', self.stage_directory])
