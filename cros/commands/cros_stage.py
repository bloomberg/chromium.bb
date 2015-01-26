# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Stage a custom image on a Moblab device or in Google Storage."""

from __future__ import print_function

import httplib
import logging
import os
import shutil
import urllib2

from chromite import cros
from chromite.cbuildbot import commands, constants
from chromite.lib import cros_build_lib
from chromite.lib import dev_server_wrapper
from chromite.lib import gs
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
  """Thrown when there is an error staging an custom image."""


@cros.CommandDecorator('stage')
class StageCommand(cros.CrosCommand):
  """Converts a locally generated image so that it can be staged remotely.

  The image can be staged either on Google Storage or on a local Moblab device.

  Moblab can only reimage and test devices if the specified image has:
    * stateful.tgz
    * update.gz
    * test_suites.tar.bz2
    * autotest_packages.tar
    * control_files.tar

  Images made in the chroot lack these artifacts and the payloads need to be
  generated.

  Once generated, the artifacts need to be simply copied over to the Moblab's
  devserver's static directory or into the specified Google Storage Bucket.

  The image name to then use for testing is output at the end of this
  script.
  """

  EPILOG = """
To stage a local image path onto a moblab device:
  cros stage /path/to/board/build/chromiumos-test-image.bin <moblab>

To stage a local image path into a Google Storage Bucket:
  cros stage /path/to/board/build/chromiumos-test-image.bin <gs_base_path>
    --boto_file=<boto_file_path>

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
    super(StageCommand, cls).AddParser(parser)
    parser.add_argument(
        'image', nargs='?', default='latest', help='Path to image we want to '
        'stage. It should be in the format of '
        '/.../.../board/build/<image>.bin')
    parser.add_argument(
        'remote', help='MobLab device that has password-less SSH set up via '
        'the chroot already. Or Google Storage Bucket in the form of '
        'gs://bucket-name/')
    parser.add_argument(
        '--boto_file', dest='boto_file', default=None,
        help='Path to boto file to use when uploading to Google Storage. If '
        'none the default chroot boto file is used.')

  def __init__(self, options):
    """Initializes cros stage."""
    cros.CrosCommand.__init__(self, options)
    self.board = None
    self.staged_image_name = self._GenerateImageName(self.options.image)
    self.stage_directory = os.path.join(MOBLAB_TMP_DIR, self.staged_image_name)
    if self.options.remote.startswith('gs://'):
      self._remote_is_moblab = False
    else:
      self._remote_is_moblab = True

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

  def _GeneratePayloads(self, tempdir):
    """Generate the update payloads we require.

    Args:
      tempdir: Temporary Directory to store the generated payloads.
    """
    dev_server_wrapper.GetUpdatePayloadsFromLocalPath(
        self.options.image, tempdir, static_dir=DEVSERVER_STATIC_DIR)
    rootfs_payload = os.path.join(tempdir, dev_server_wrapper.ROOTFS_FILENAME)
    # Devservers will look for a file named *_full_*.
    shutil.move(rootfs_payload, os.path.join(tempdir, 'update_full_dev.bin'))

  def _GenerateTestBits(self, tempdir):
    """Generate and transfer to the Moblab the test bits we require.

    Args:
      tempdir: Temporary Directory to store the generated test artifacts.
    """
    build_root = cros_build_lib.GetSysroot(board=self.board)
    cwd = os.path.join(build_root, BOARD_BUILD_DIR)
    tarball_funcs = [commands.BuildAutotestControlFilesTarball,
                     commands.BuildAutotestPackagesTarball,
                     commands.BuildAutotestTestSuitesTarball]
    for tarball_func in tarball_funcs:
      tarball_func(build_root, cwd, tempdir)

  def _StageOnMoblab(self, tempdir):
    """Stage the generated payloads and test bits on a moblab device.

    Args:
      tempdir: Temporary Directory that contains the generated payloads and
               test bits.
    """
    with remote_access.ChromiumOSDeviceHandler(self.options.remote) as device:
      device.RunCommand(['mkdir', '-p', self.stage_directory])
      for f in os.listdir(tempdir):
        device.CopyToDevice(os.path.join(tempdir, f), self.stage_directory)
      device.RunCommand(['chown', '-R', 'moblab:moblab',
                         MOBLAB_TMP_DIR])
      try:
        stage_url = DEVSERVER_STAGE_URL % dict(moblab=self.options.remote,
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

  def _StageOnGS(self, tempdir):
    """Stage the generated payloads and test bits into a Google Storage bucket.

    Args:
      tempdir: Temporary Directory that contains the generated payloads and
               test bits.
    """
    gs_context = gs.GSContext(boto_file=self.options.boto_file)
    for f in os.listdir(tempdir):
      gs_context.CopyInto(os.path.join(tempdir, f), os.path.join(
          self.options.remote, self.staged_image_name))
    logging.info('\n\nStaging Completed!')
    logging.info('Image is staged in Google Storage as %s',
                 self.staged_image_name)

  def Run(self):
    """Perform the cros stage command."""
    logging.info('Attempting to stage: %s as Image: %s at Location: %s',
                 self.options.image, self.staged_image_name,
                 self.options.remote)
    with osutils.TempDir() as tempdir:
      self._GeneratePayloads(tempdir)
      self._GenerateTestBits(tempdir)
      if self._remote_is_moblab:
        self._StageOnMoblab(tempdir)
      else:
        self._StageOnGS(tempdir)
