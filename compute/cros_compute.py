# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script to run Chrome OS-specific GCE commands ."""

from __future__ import print_function

import logging
import os

from chromite.compute import bot_constants
from chromite.compute import compute_configs
from chromite.compute import gcloud
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import gs
from chromite.lib import osutils


# Supported targets.
TARGETS = (
    'instances',
    'images',
    'disks',
)

# Supported operations.
OPERATIONS = {
    'instances': ('create', 'delete', 'list', 'ssh',),
    'images': ('create', 'delete', 'list',),
    'disks': ('list',),
}

# All supported operations.
ALL_OPERATIONS = set(cros_build_lib.iflatten_instance(OPERATIONS.values()))


def BotifyInstance(instance, project, zone, testing=False):
  """Transforms the |instance| to a Chrome OS bot.

  Perform necessary tasks to clone the chromite repostority on the
  |instance| and run setup scripts as BUILDBOT_USER.

  The majority the setup logic is (and should be) in the scripts that
  run directly on the |instance| because `gcloud compute ssh` incurs
  addtional overhead on every invocation. (e.g. instance name to IP
  lookup, copying public key if needed, etc).

  Args:
    instance: Name of the GCE instance.
    project: GCloud Project that the |instance| belongs to.
    zone: Zone of the GCE instance.
    testing: If set, copy the current chromite directory to |instance|.
      Otherwise, `git clone` the chromite repository.
  """
  # TODO: To speed this up, we can switch to run remote commands using
  # remote_access.RemoteAgent wrapper. We'd only need to run `gcloud
  # compute ssh` once to initiate the first SSH connection (which
  # copies the public key).
  gcctx = gcloud.GCContext(project, zone=zone)
  gcctx.SSH(instance, cmd='umask 0022')
  # Set up buildbot user and grant it sudo rights.
  gcctx.SSH(instance,
            cmd=('sudo adduser --disabled-password --gecos "" %s'
                 % bot_constants.BUILDBOT_USER))
  gcctx.SSH(instance,
            cmd='sudo adduser %s sudo' % bot_constants.BUILDBOT_USER)
  gcctx.SSH(
      instance,
      cmd=('sudo awk \'BEGIN{print "%%%s ALL=NOPASSWD: ALL" >>"/etc/sudoers"}\''
           % bot_constants.BUILDBOT_USER))

  # Copy bot credentials to a temporay location.
  dest_path = bot_constants.BOT_CREDS_TMP_PATH
  src_path = os.getenv(bot_constants.BOT_CREDS_DIR_ENV_VAR)
  if not src_path:
    raise ValueError('Environment variable %s is not set. This is necessary'
                     'to set up credentials for the bot.'
                     % bot_constants.BOT_CREDS_DIR_ENV_VAR)
  gcctx.CopyFilesToInstance(instance, src_path, dest_path)
  gcctx.SSH(
      instance,
      cmd='sudo chown -R %s:%s %s' % (bot_constants.BUILDBOT_USER,
                                      bot_constants.BUILDBOT_USER,
                                      dest_path))
  # Set the credential files/directories to the correct mode.
  gcctx.SSH(
      instance,
      cmd=r'sudo find %s -type d -exec chmod 700 {} \;' % dest_path)
  gcctx.SSH(
      instance,
      cmd=r'sudo find %s -type f -exec chmod 600 {} \;' % dest_path)

  # Bootstrap by copying chromite to the temporary directory.
  base_dir = '/tmp'
  if testing:
    # Copy the current chromite directory. This allows all local
    # changes to be copied to the temporary instance for testing.
    chromite_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    with osutils.TempDir(prefix='cros_compute') as tempdir:
      chromite_tarball = os.path.join(tempdir, 'chromite.tar.gz')
      cros_build_lib.RunCommand(
          ['tar', '--exclude=.git', '--exclude=third_party',
           '--exclude=appengine', '-czf', chromite_tarball, 'chromite'],
          cwd=os.path.dirname(chromite_dir))
      dest_path = os.path.join(base_dir, os.path.basename(chromite_tarball))
      gcctx.CopyFilesToInstance(instance, chromite_tarball, dest_path)
      gcctx.SSH(instance, cmd='tar xzf %s -C %s' % (dest_path, base_dir))
  else:
    # Install git to clone chromite.
    gcctx.SSH(instance, cmd='sudo apt-get install git')
    gcctx.SSH(instance, cmd='cd %s && git clone %s' % (
        base_dir, bot_constants.CHROMITE_URL))

  # Run the setup script as BUILDBOT_USER.
  gcctx.SSH(
      instance,
      cmd='sudo su %s -c %s' % (
          bot_constants.BUILDBOT_USER,
          os.path.join(base_dir, 'chromite', 'compute', 'setup_bot')))


def CreateAndArchiveImageFromInstance(instance, project, zone, image_name):
  """Create an image from the root disk of |instance|.

  This creates the image on the |instance|, archives it to Google
  Storage, and add the image to the Compute Engine.

  The root disk needs to be smaller than 10GB. See requirement in
  https://cloud.google.com/compute/docs/images

  Args:
    instance: Name of the GCE instance.
    project: GCloud Project that the |instance| belongs to.
    zone: Zone of the GCE instance.
    image_name: Name of the image.

  """
  image_name_with_suffix = '%s%s' % (image_name, compute_configs.IMAGE_SUFFIX)
  archive_path = os.path.join(
      compute_configs.GS_IMAGE_ARCHIVE_BASE_URL,
      image_name_with_suffix)
  gsctx = gs.GSContext()
  if gsctx.Exists(archive_path):
    # We should not allow overwriting the existing image.
    cros_build_lib.Die('Image %s already exists!' % archive_path)

  logging.info('Creating image %s', image_name_with_suffix)
  gcctx = gcloud.GCContext(project, zone=zone)

  # Do not include current user's home directory.
  home_dir = gcctx.SSH(instance, cmd='cd ~ && pwd',
                       redirect_stdout=True).output.strip()
  # Create the image. gcimagebundle should have been installed in the
  # base image already.
  output_dir = '/tmp'
  cmd = ['sudo',
         'gcimagebundle',
         '--disk', compute_configs.BOOT_DISK,
         '--root', '/',
         '--output_directory', '/tmp',
         '--output_file_name', image_name_with_suffix,
         '--excludes', home_dir,
         '--loglevel=DEBUG', '--log_file=/tmp/image_bundle.log']
  gcctx.SSH(instance, cmd=' '.join(cmd))

  # Get a temporary Copy of gsutil.
  gcctx.SSH(instance,
            cmd=('curl https://storage.googleapis.com/pub/gsutil.tar.gz'
                 '| tar -xz -C /tmp'))
  # Upload the image to Google Storage.
  src_path = os.path.join(output_dir, image_name_with_suffix)
  gcctx.SSH(
      instance,
      cmd='/tmp/gsutil/gsutil cp %s %s' % (src_path, archive_path))

  # Add the image to the project.
  gcctx.CreateImage(image_name, archive_path)


def CreateImageForCrosBots(project, zone, address=None, testing=False):
  """Create a new image for cros bots."""
  gcctx = gcloud.GCContext(project, zone=zone, quiet=True)
  # The name of the image to create.
  image = compute_configs.DEFAULT_IMAGE_NAME
  if testing:
    image = '%s-testing' % image

  # Create a temporary instance and botify it.
  instance = ('chromeos-temp-%s'
              % cros_build_lib.GetRandomString())
  gcctx.CreateInstance(instance, image=compute_configs.DEFAULT_BASE_IMAGE,
                       address=address)
  try:
    BotifyInstance(instance, project, zone, testing=testing)
  except:
    # Clean up the temp instance.
    gcctx.DeleteInstance(instance)
    raise

  gcctx.DeleteInstance(instance, keep_disks='boot')
  # By default the name of the boot disk is the same as the name of
  # the instance
  disk = instance
  try:
    # Create image from source disk. By default the name of the boot
    # disk is the same as the name of the instance.
    gcctx.CreateImage(image, disk=disk)
  finally:
    gcctx.DeleteDisk(disk)


def main(argv):
  parser = commandline.ArgumentParser(description=__doc__)

  parser.add_argument(
      'target', choices=TARGETS, help='Operation target')
  parser.add_argument(
      'operation', type=str, choices=ALL_OPERATIONS,
      help='Operation type. Valid operations depend on target.')
  parser.add_argument(
      '--project', type=str, default=compute_configs.PROJECT,
      help='Project name')
  parser.add_argument(
      '--instance', type=str, default=None, help='Instance name')
  parser.add_argument(
      '--image', type=str, default=None, help='Image name')
  parser.add_argument(
      '--zone', type=str, default=compute_configs.DEFAULT_ZONE,
      help='Zone to run the command against')
  parser.add_argument(
      '--address', type=str, default=None,
      help='IP to assign to the instance')
  parser.add_argument(
      '--config', type=str, default=None,
      help='Config to create the instance from')
  parser.add_argument(
      '--chromeos', default=False, action='store_true',
      help='Turn on the offical Chrome OS flag (e.g. to create '
           'a new Chrome OS bot image)')
  parser.add_argument(
      '--testing', default=False, action='store_true',
      help='This option is mainly for testing changes to the official '
           'Chrome OS bot image creation process. If set true, it copies '
           'the current chromite directory onto the instance to preserve '
           'all local changes. It also appends the image name with -testing.')

  opts = parser.parse_args(argv)
  opts.Freeze()

  if opts.operation not in OPERATIONS[opts.target]:
    cros_build_lib.Die(
        'Unknown operation %s. Valid operations are %s' % (
            opts.target, OPERATIONS[opts.target]))

  gcctx = gcloud.GCContext(opts.project, zone=opts.zone)
  if opts.target == 'images':
    # Operations against images.
    if opts.operation == 'create':
      if opts.chromeos:
        # Create a new image for Chrome OS bots. The name of the base
        # image and the image to create are defined in compute_configs.
        CreateImageForCrosBots(opts.project, opts.zone, testing=opts.testing,
                               address=opts.address)
      else:
        if not opts.image or not opts.instance:
          cros_build_lib.Die(
              'Please specify the name of the instance (--instance) '
              'and the name of the image (--image) to create')
          CreateAndArchiveImageFromInstance(opts.instance, opts.project,
                                            opts.zone, opts.image)
    elif opts.operation == 'delete':
      gcctx.DeleteImage(opts.image)
    elif opts.operation == 'list':
      gcctx.ListImages()

  elif opts.target == 'instances':
    # Operations against instances.
    if opts.operation == 'create':
      if not opts.instance:
        cros_build_lib.Die('Please specify the instance name (--instance)')

      if ((not opts.image and not opts.config) or
          (opts.image and opts.config)):
        cros_build_lib.Die(
            'One and only one of the two options should be specified: '
            'source image (--image) or the builder (--config)')
      if opts.image:
        gcctx.CreateInstance(opts.instance, image=opts.image)
      elif opts.config:
        config = compute_configs.configs.get(opts.config, None)
        if not config:
          cros_build_lib.Die('Unkown config %s' % opts.config)
        gcctx.CreateInstance(opts.instance, address=opts.address, **config)
    elif opts.operation == 'delete':
      if not opts.instance:
        cros_build_lib.Die('Please specify the instance name (--instance)')
      gcctx.DeleteInstance(opts.instance)
    elif opts.operation == 'list':
      gcctx.ListInstances()
    elif opts.operation == 'ssh':
      if not opts.instance:
        cros_build_lib.Die('Please specify the instance name (--instance)')
      gcctx.SSH(opts.instance)

  elif opts.target == 'disks':
    if opts.operation == 'list':
      gcctx.ListDisks()
