#!/usr/bin/python
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library to make common google storage operations more reliable.
"""

import logging
import os

from chromite.lib import cros_build_lib

BOTO_DEFAULT = os.path.expanduser('~/.boto')

class GSContextException(Exception):
  """Thrown when expected google storage preconditions are not met."""

class GSContextPreconditionFailed(Exception):
  """Thrown when google storage returns code=PreconditionFailed."""

class GSContext(object):
  """A class to wrap common google storage operations."""

  # How many times to retry uploads.
  _RETRIES = 10

  # Multiplier for how long to sleep (in seconds) between retries; will delay
  # (1*sleep) the first time, then (2*sleep), continuing via attempt * sleep.
  _SLEEP_TIME = 60

  def __init__(self, gsutil_bin, boto_file=None, acl_file=None,
               dry_run=False):
    """Constructor.

    Args:
      gsutil_bin: Fully qualified path to the gsutil utility.
      boto_file: Fully qualified path to user's .boto credential file.
      acl_file: A permission file capable of setting different permissions
                for differnt sets of users.
      dry_run: Testing mode that prints commands that would be run.
    """
    self.gsutil_bin = gsutil_bin
    self._CheckFile('gsutil not found', self.gsutil_bin)

    self.boto_file = boto_file

    # Prefer any boto file specified on the command line first
    if self.boto_file is not None:
      self._CheckFile('Boto credentials not found', self.boto_file)

    # If told which boto to use, abide; else use the default boto value
    if self.boto_file is None:
      self.boto_file = os.environ.get('BOTO_CONFIG', BOTO_DEFAULT)
    self._CheckFile('Boto credentials not found', self.boto_file)

    self.acl_file = acl_file
    if self.acl_file is not None:
      self._CheckFile('Not a valid permissions file', self.acl_file)

    self.dry_run = dry_run

    cmd = [self.gsutil_bin, 'version']
    cros_build_lib.RunCommand(cmd, debug_level=logging.DEBUG,
                              extra_env={'BOTO_CONFIG': self.boto_file},
                              log_output=True)

  def _CheckFile(self, errmsg, afile):
    """Pre-flight check for valid inputs.

    Args:
      errmsg: Error message to display.
      afile: Fully qualified path to test file existance.
    """
    if not os.path.isfile(afile):
      raise GSContextException('%s, %s is not a file' % (errmsg, afile))

  def _Upload(self, local_file, remote_file, acl=None, retries=_RETRIES,
              gsutil_opts=()):
    """Upload to GS bucket.

    Canned ACL permissions can be specified on the gsutil cp command line.

    More info:
    https://developers.google.com/storage/docs/accesscontrol#applyacls

    Args:
       local_file: Fully qualified path of local file.
       remote_file: Full gs:// path of remote_file.
       acl: One of the google storage canned_acls to apply.
       retries: Number of times to retry the google storage operation.
       gsutil_opts: Additional gsutil command line switch behavior.

    Returns:
       Return the arg tuple of two if the upload failed.
    """
    cp_opts = ['cp']
    if acl is not None:
      cp_opts += ['-a', acl]

    cmd = [self.gsutil_bin] + list(gsutil_opts) + cp_opts + [local_file,
                                                             remote_file]
    if not self.dry_run:
      try:
        cros_build_lib.RunCommandWithRetries(
            retries, cmd, sleep=self._SLEEP_TIME,
            extra_env={'BOTO_CONFIG': self.boto_file},
            debug_level=logging.DEBUG, log_output=True,
            combine_stdout_stderr=True)

      # gsutil uses the same exit code for any failure, so we are left to
      # parse the output as needed.
      except cros_build_lib.RunCommandError as e:
        if 'code=PreconditionFailed' in e.result.output:
          raise GSContextPreconditionFailed(e)
        raise
    else:
      logging.debug('RunCommand: %r', cmd)

  def Upload(self, local_path, upload_url, filename=None, acl=None):
    """Upload a file to google storage and possibly set a canned ACL.

    Args:
      local_path: Local path (directory) to artifact.
      upload_url: Path portion of gs:// url that artifact will be copied to.
      filename: Required. Filename of artifact to upload.
      acl: A canned ACL.
    """
    full_filename = os.path.join(local_path, filename)
    full_url = os.path.join(upload_url, filename)
    self._Upload(full_filename, full_url, acl)

  def SetACL(self, upload_url, acl=None):
    """Set access on a file already in google storage.

    Args:
      upload_url: gs:// url that will have acl applied to it.
      acl: An ACL permissions file or canned ACL.
    """
    if acl is None:
      acl = self.acl_file

    acl_cmd = [self.gsutil_bin, 'setacl', acl, upload_url]
    if not self.dry_run:
      cros_build_lib.RunCommandWithRetries(
          self._RETRIES, acl_cmd, sleep=self._SLEEP_TIME,
          debug_level=logging.DEBUG, extra_env={'BOTO_CONFIG': self.boto_file})
    else:
      logging.debug('RunCommand: %r', acl_cmd)

  def UploadOnMatch(self, local_path, upload_url, filename=None, acl=None,
                    version=0):
    """Copy if the preconditions are met.

    Args:
      local_path: Local path (directory) to artifact.
      upload_url: Path portion of gs:// url that artifact will be copied to.
      filename: Required. Filename of artifact to upload.
      acl: A canned ACL.
      sequence: Useful to determine if a file exists yet or not with value 0
                Should be positive integer values.
    """
    full_filename = os.path.join(local_path, filename)
    full_url = os.path.join(upload_url, filename)
    gsutil_opts = ['-h', 'x-goog-if-sequence-number-match:%d' % version]
    self._Upload(full_filename, full_url, acl=acl, retries=0,
                 gsutil_opts=gsutil_opts)
