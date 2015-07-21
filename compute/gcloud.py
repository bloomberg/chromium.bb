# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library for gcloud operations."""

from __future__ import print_function

import re

from chromite.compute import compute_configs
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import timeout_util


class GCContextException(Exception):
  """Base exception for this module."""


# Wrap RunCommandError exceptions to distinguish the exceptions raised
# by commands we invoke from those that could be thrown by others who
# use RunCommand.
class GCCommandError(GCContextException, cros_build_lib.RunCommandError):
  """Thrown when an error happened we couldn't decode."""


class ZoneNotSpecifiedError(GCContextException):
  """Raised when zone is not specified for a zone-specific command."""


class GCContext(object):
  """A wrapper around the gcloud commandline tool.

  Currently supports only `gcloud compute`.
  """
  GCLOUD_BASE_COMMAND = 'gcloud'
  GCLOUD_COMPUTE_COMMAND = 'compute'

  def __init__(self, project, zone=None, quiet=False, dry_run=False):
    """Initializes GCContext.

    Args:
      project: The Google Cloud project to use.
      zone: The default zone to operate on when zone is not given for a
            zone-specific command.
      quiet: If set True, skip any user prompts and use the default value.
      dry_run: If True, don't actually interact with the GCE project.
    """
    self.project = project
    self.zone = zone
    self.quiet = quiet
    self.dry_run = dry_run

  @classmethod
  def _GetBaseComputeCommand(cls):
    """Returns the base Google Compute Engine command."""
    return [cls.GCLOUD_BASE_COMMAND, cls.GCLOUD_COMPUTE_COMMAND]

  def DoCommand(self, cmd, **kwargs):
    """Runs |cmd|.

    cmd: The command to run.
    project: The project to use. Defaults to using self.project.
    quiet: Suppress user prompts from the gcloud cli.
    zone: The zone to use. Elided by default.
    kwargs: See cros_build_lib.RunCommand.
    """
    cmd = self._GetBaseComputeCommand() + cmd
    cmd += ['--project', kwargs.pop('project', self.project)]
    if kwargs.pop('quiet', self.quiet):
      cmd += ['--quiet']
    zone = kwargs.pop('zone', None)
    if zone:
      cmd += ['--zone', zone]

    if self.dry_run:
      logging.debug('%s: Would have run: %s',
                    self.__class__.__name__, cmd)
      return

    try:
      return cros_build_lib.RunCommand(cmd, **kwargs)
    except cros_build_lib.RunCommandError as e:
      raise GCCommandError(e.msg, e.result, e.exception)


  def DoZoneSpecificCommand(self, cmd, **kwargs):
    """Runs the zone-specific |cmd|.

    cmd: The command to run.
    zone: The zone to use. Defaults to using self.zone.
    kwargs: See DoCommand.
    """
    kwargs.setdefault('zone', self.zone)
    if not kwargs.get('zone'):
      # To avoid ambiguity, force user to specify a zone (or a default
      # zone) when accessing zone-specific resources.
      raise ZoneNotSpecifiedError()
    return self.DoCommand(cmd, **kwargs)

  def CopyFilesToInstance(self, instance, src, dest, **kwargs):
    """Copies files from |src| to |dest| on |instance|.

    Args:
      instance: Name of the instance.
      src: The source path.
      dest: The destination path.
      kwargs: See DoCommand.
    """
    return self._CopyFiles(src, '%s:%s' % (instance, dest), **kwargs)

  def CopyFilesFromInstance(self, instance, src, dest, **kwargs):
    """Copies files from |src| on |instance| to local |dest|.

    Args:
      instance: Name of the instance.
      src: The source path.
      dest: The destination path.
      kwargs: See DoCommand.
    """
    return self._CopyFiles('%s:%s' % (instance, src), dest, **kwargs)

  def _CopyFiles(self, src, dest, ssh_key_file=None, user=None, **kwargs):
    """Copies files from |src| to |dest|.

    Args:
      instance: Name of the instance.
      src: The source path (a local path or user:instance@path).
      dest: The destination path (a local path or user:instance@path).
      ssh_key_file: File containing private key for SSH.
      user: User to SSH as.
      kwargs: See DoCommand.
    """
    command = ['copy-files']

    if ssh_key_file:
      command += ['--ssh-key-file', ssh_key_file]
    if user:
      dest = '%s@%s' % (user, dest)

    command += [src, dest]

    return self.DoZoneSpecificCommand(command, **kwargs)

  def SSH(self, instance, user=None, cmd=None, ssh_key_file=None, **kwargs):
    """SSH into |instance|. Run |cmd| if it is provided.

    Args:
      instance: Name of the instance.
      user: User to SSH as.
      cmd: Command to run on |instance|.
      ssh_key_file: File containing private key for SSH.
    """
    ssh_cmd = ['ssh']
    if user:
      ssh_cmd += ['%s@%s' % (user, instance)]
    else:
      ssh_cmd += [instance]
    if cmd:
      ssh_cmd += ['--command', cmd]
    if ssh_key_file:
      ssh_cmd += ['--ssh-key-file', ssh_key_file]
    return self.DoZoneSpecificCommand(ssh_cmd, **kwargs)

  def ListInstances(self, **kwargs):
    """Lists all instances."""
    return self.DoCommand(['instances', 'list'], **kwargs)

  def ListDisks(self, **kwargs):
    """Lists all disks."""
    return self.DoCommand(['disks', 'list'], **kwargs)

  def ListImages(self, **kwargs):
    """Lists all instances."""
    return self.DoCommand(['images', 'list'], **kwargs)

  def CreateImage(self, image, source_uri=None, disk=None, **kwargs):
    """Creates an image from |source_uri| or |disk|.

    Args:
      image: The name of image to create.
      source_uri: The tar.gz image file (e.g. gs://foo/bar/image.tar.gz)
      disk: The source disk to create the image from. One and only one of
         |source_uril| and |disk| should be set.
      kwargs: See DoCommand.
    """
    if source_uri and disk:
      raise GCContextException('Cannot specify both source uri and disk.')

    cmd = ['images', 'create', image]
    if disk:
      cmd += ['--source-disk', disk]
      zone = kwargs.get('zone', self.zone)
      if zone:
        # Disks are zone-specific resources.
        cmd += ['--source-disk-zone', zone]

    if source_uri:
      cmd += ['--source-uri', source_uri]

    return self.DoCommand(cmd, **kwargs)

  def GetInstanceIP(self, instance, **kwargs):
    """Returns an instance's ephemeral external IP address.

    May not work with all network configurations.
    """
    # It sure would be nice if there were an easier way to fetch
    # the instance's ephemeral IP address...
    description = self.DoZoneSpecificCommand(
        ['instances', 'describe', instance, '--format', 'text'],
        redirect_stdout=True, **kwargs).output
    match = re.search('natIP: ([.0-9]+)', description)
    if match:
      return match.group(1)
    else:
      raise GCContextException('Unable to parse instance IP.  '
                               '"gcloud compute instances describe %s": %s'
                               % (instance, description))

  def CreateInstance(self, instance, image=None, machine_type=None,
                     network=None, address=None, wait_until_sshable=True,
                     scopes=None, **kwargs):
    """Creates an |instance|.

    Additionally, if an image is provided, adds a custom metadata pair to
    identify the the image used to create the instance.

    Args:
      instance: The name of the instance to create.
      image: The source image to create |instance| from.
      machine_type: The machine type to use.
      network: An existing network to create the instance in.
      address: The external IP address to assign to |instance|.
      wait_until_sshable: After creating |instance|, wait until
        we can ssh into |instance|.
      scopes: The list (or tuple) of service account scopes.
      kwargs: See DoZoneSpecificCommand.
    """
    cmd = ['instances', 'create', instance]
    if image:
      cmd += ['--image', image]
      cmd += ['--metadata',
              '%s="%s"' % (compute_configs.METADATA_IMAGE_NAME, image)]
    if network is not None:
      cmd += ['--network', network]
    if address is not None:
      cmd += ['--address', address]
    if machine_type is not None:
      cmd += ['--machine-type', machine_type]
    if scopes is not None:
      cmd += ['--scopes', ','.join(list(scopes))]

    ret = self.DoZoneSpecificCommand(cmd, **kwargs)
    if wait_until_sshable:
      def _IsUp():
        try:
          instance_ip = self.GetInstanceIP(instance, **kwargs)
          command = ['nc', '-zv', instance_ip, '22']
          cros_build_lib.RunCommand(command, capture_output=True)
        except cros_build_lib.RunCommandError:
          return False
        else:
          return True

      try:
        logging.info('Waiting for the instance to be sshable...')
        timeout = 60 * 5
        timeout_util.WaitForReturnTrue(_IsUp, timeout, period=5)
      except timeout_util.TimeoutError:
        self.DeleteInstance(instance)
        raise GCContextException('Timed out wating to ssh into the instance')

    return ret

  def DeleteInstance(self, instance, quiet=False, keep_disks=None, **kwargs):
    """Deletes |instance|.

    Args:
      instance: Name of the instance.
      quiet: Silent delete instance without prompting the user.
      keep_disks: Keep the type of the disk; valid types are
        'boot', 'data', and 'all'.
      kwargs: See DoCommand.
    """
    cmd = ['instances', 'delete', instance]
    if quiet:
      cmd += ['-q']
    if keep_disks:
      cmd += ['--keep-disks', keep_disks]

    return self.DoZoneSpecificCommand(cmd, **kwargs)

  def DeleteImage(self, image, **kwargs):
    """Deletes |image|. User will be prompted to confirm.

    Args:
      image: Name of the image.
      kwargs: See DoCommand.
    """
    return self.DoCommand(['images', 'delete', image], **kwargs)

  def DeleteDisk(self, disk, **kwargs):
    """Deletes |disk|.

    Args:
      disk: Name of the disk.
      kwargs: See DoCommand.
    """
    cmd = ['disks', 'delete', disk]
    return self.DoZoneSpecificCommand(cmd, **kwargs)
