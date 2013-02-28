# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library to make common google storage operations more reliable.
"""

import logging
import os

from chromite.buildbot import constants
from chromite.lib import cache
from chromite.lib import cros_build_lib
from chromite.lib import osutils


# Default pathway; stored here rather than usual buildbot.constants since
# we don't want to import buildbot code from here.
# Note that this value is reset after GSContext via the GetDefaultGSUtilBin
# method; we set it initially here just for the sake of making clear it
# exists.
GSUTIL_BIN = None
PUBLIC_BASE_HTTPS_URL = 'https://commondatastorage.googleapis.com/'
PRIVATE_BASE_HTTPS_URL = 'https://sandbox.google.com/storage/'
BASE_GS_URL = 'gs://'


def CanonicalizeURL(url, strict=False):
  """Convert provided URL to gs:// URL, if it follows a known format.

  Arguments:
    url: URL to canonicalize.
    strict: Raises exception if URL cannot be canonicalized.
  """
  for prefix in (PUBLIC_BASE_HTTPS_URL, PRIVATE_BASE_HTTPS_URL):
    if url.startswith(prefix):
      return url.replace(prefix, BASE_GS_URL)

  if not url.startswith(BASE_GS_URL) and strict:
    raise ValueError('Url %r cannot be canonicalized.' % url)

  return url


def GetGsURL(bucket, for_gsutil=False, public=True, suburl=''):
  """Construct a Google Storage URL

  Args:
    bucket: The Google Storage bucket to use
    for_gsutil: Do you want a URL for passing to `gsutil`?
    public: Do we want the public or private url
    suburl: A url fragment to tack onto the end
  Returns:
    The fully constructed URL
  """
  if for_gsutil:
    urlbase = BASE_GS_URL
  else:
    urlbase = PUBLIC_BASE_HTTPS_URL if public else PRIVATE_BASE_HTTPS_URL
  return '%s%s/%s' % (urlbase, bucket, suburl)


class GSContextException(Exception):
  """Thrown when expected google storage preconditions are not met."""


class GSContextPreconditionFailed(GSContextException):
  """Thrown when google storage returns code=PreconditionFailed."""

class GSNoSuchKey(GSContextException):
  """Thrown when google storage returns code=NoSuchKey."""


class GSContext(object):
  """A class to wrap common google storage operations."""

  DEFAULT_BOTO_FILE = os.path.expanduser('~/.boto')
  # This is set for ease of testing.
  DEFAULT_GSUTIL_BIN = None
  DEFAULT_GSUTIL_BUILDER_BIN = '/b/build/third_party/gsutil/gsutil'
  # How many times to retry uploads.
  DEFAULT_RETRIES = 10

  # Multiplier for how long to sleep (in seconds) between retries; will delay
  # (1*sleep) the first time, then (2*sleep), continuing via attempt * sleep.
  DEFAULT_SLEEP_TIME = 60

  GSUTIL_TAR = 'gsutil-3.10.tar.gz'
  GSUTIL_URL = PUBLIC_BASE_HTTPS_URL + 'chromeos-public/%s' % GSUTIL_TAR

  @classmethod
  def GetDefaultGSUtilBin(cls):
    if cls.DEFAULT_GSUTIL_BIN is None:
      gsutil_bin = cls.DEFAULT_GSUTIL_BUILDER_BIN
      if not os.path.exists(gsutil_bin):
        gsutil_bin = osutils.Which('gsutil')
      cls.DEFAULT_GSUTIL_BIN = gsutil_bin
    return cls.DEFAULT_GSUTIL_BIN

  @classmethod
  def Cached(cls, cache_dir, *args, **kwargs):
    """Reuses previously fetched GSUtil, performing the fetch if necessary.

    Arguments:
      cache_dir: The toplevel cache dir.
      *args, **kwargs:  Arguments that are passed through to the GSContext()
        constructor.

    Returns:
      An initialized GSContext() object.
    """
    common_path = os.path.join(cache_dir, constants.COMMON_CACHE)
    tar_cache = cache.TarballCache(common_path)
    key = (cls.GSUTIL_TAR,)

    # The common cache will not be LRU, removing the need to hold a read
    # lock on the cached gsutil.
    ref = tar_cache.Lookup(key)
    if ref.Exists():
      logging.debug('Reusing cached gsutil.')
    else:
      logging.debug('Fetching gsutil.')
      with osutils.TempDirContextManager(
          base_dir=tar_cache.staging_dir) as tempdir:
        gsutil_tar = os.path.join(tempdir, cls.GSUTIL_TAR)
        cros_build_lib.RunCurl([cls.GSUTIL_URL, '-o', gsutil_tar],
                               debug_level=logging.DEBUG)
        ref.SetDefault(gsutil_tar)

    gsutil_bin = os.path.join(ref.path, 'gsutil', 'gsutil')
    return cls(*args, gsutil_bin=gsutil_bin, **kwargs)

  def __init__(self, boto_file=None, acl_file=None, dry_run=False,
               gsutil_bin=None, init_boto=False, retries=None, sleep=None):
    """Constructor.

    Args:
      boto_file: Fully qualified path to user's .boto credential file.
      acl_file: A permission file capable of setting different permissions
        for different sets of users.
      dry_run: Testing mode that prints commands that would be run.
      gsutil_bin: If given, the absolute path to the gsutil binary.  Else
        the default fallback will be used.
      init_boto: If set to True, GSContext will check during __init__ if a
        valid boto config is configured, and if not, will attempt to ask the
        user to interactively set up the boto config.
      retries: Number of times to retry a command before failing.
      sleep: Amount of time to sleep between failures.
    """
    if gsutil_bin is None:
      gsutil_bin = self.GetDefaultGSUtilBin()
    self._CheckFile('gsutil not found', gsutil_bin)
    self.gsutil_bin = gsutil_bin

    # Prefer boto_file if specified, else prefer the env then the default.
    if boto_file is None:
      boto_file = os.environ.get('BOTO_CONFIG', self.DEFAULT_BOTO_FILE)
    self.boto_file = boto_file

    if acl_file is not None:
      self._CheckFile('Not a valid permissions file', acl_file)
    self.acl_file = acl_file

    self.dry_run = dry_run
    self._retries = self.DEFAULT_RETRIES if retries is None else int(retries)
    self._sleep_time = self.DEFAULT_SLEEP_TIME if sleep is None else int(sleep)

    if init_boto:
      self._InitBoto()
    self._CheckFile('Boto credentials not found', self.boto_file)

  def _CheckFile(self, errmsg, afile):
    """Pre-flight check for valid inputs.

    Args:
      errmsg: Error message to display.
      afile: Fully qualified path to test file existance.
    """
    if not os.path.isfile(afile):
      raise GSContextException('%s, %s is not a file' % (errmsg, afile))

  def _TestGSLs(self):
    """Quick test of gsutil functionality."""
    result = self._DoCommand(['ls'], retries=0, debug_level=logging.DEBUG,
                             redirect_stderr=True, error_code_ok=True)
    return not (result.returncode == 1 and
                'no configured credentials' in result.error)

  def _ConfigureBotoConfig(self):
    """Make sure we can access protected bits in GS."""
    print 'Configuring gsutil. **Please use your @google.com account.**'
    try:
      self._DoCommand(['config'], retries=0, debug_level=logging.CRITICAL,
                      print_cmd=False)
    finally:
      if (os.path.exists(self.boto_file) and not
          os.path.getsize(self.boto_file)):
        os.remove(self.boto_file)
        raise GSContextException('GS config could not be set up.')

  def _InitBoto(self):
    if not self._TestGSLs():
      self._ConfigureBotoConfig()

  def Cat(self, path):
    """Returns the contents of a GS object."""
    return self._DoCommand(['cat', path], redirect_stdout=True)

  def CopyInto(self, local_path, remote_dir, filename=None, acl=None,
               version=None):
    """Upload a local file into a directory in google storage.

    Args:
      local_path: Local file path to copy.
      remote_dir: Full gs:// url of the directory to transfer the file into.
      filename: If given, the filename to place the content at; if not given,
        it's discerned from basename(local_path).
      acl: If given, a canned ACL.
      version: If given, the generation; essentially the timestamp of the last
        update.  Note this is not the same as sequence-number; it's
        monotonically increasing bucket wide rather than reset per file.
        The usage of this is if we intend to replace/update only if the version
        is what we expect.  This is useful for distributed reasons- for example,
        to ensure you don't overwrite someone else's creation, a version of
        0 states "only update if no version exists".
    """
    filename = filename if filename is not None else local_path
    # Basename it even if an explicit filename was given; we don't want
    # people using filename as a multi-directory path fragment.
    return self.Copy(local_path,
                      '%s/%s' % (remote_dir, os.path.basename(filename)),
                      acl=acl, version=version)

  def _RunCommand(self, cmd, **kwargs):
    try:
      return cros_build_lib.RunCommand(cmd, **kwargs)
    # gsutil uses the same exit code for any failure, so we are left to
    # parse the output as needed.
    except cros_build_lib.RunCommandError as e:
      error = e.result.error
      if error and 'GSResponseError' in error:
        if 'code=PreconditionFailed' in error:
          raise GSContextPreconditionFailed(e)
        if 'code=NoSuchKey' in error:
          raise GSNoSuchKey(e)
      raise


  def _DoCommand(self, gsutil_cmd, headers=(), retries=None, **kwargs):
    """Run a gsutil command, suppressing output, and setting retry/sleep.

    Returns:
      A RunCommandResult object.
    """
    cmd = [self.gsutil_bin]
    for header in headers:
      cmd += ['-h', header]
    cmd.extend(gsutil_cmd)

    if retries is None:
      retries = self._retries

    extra_env = kwargs.pop('extra_env', {})
    extra_env.setdefault('BOTO_CONFIG', self.boto_file)

    if self.dry_run:
      logging.debug("%s: would've ran %r", self.__class__.__name__, cmd)
    else:
      return cros_build_lib.RetryCommand(
          self._RunCommand, retries, cmd, sleep=self._sleep_time,
          extra_env=extra_env, **kwargs)

  def Copy(self, src_path, dest_path, acl=None, version=None, **kwargs):
    """Copy to/from GS bucket.

    Canned ACL permissions can be specified on the gsutil cp command line.

    More info:
    https://developers.google.com/storage/docs/accesscontrol#applyacls

    Args:
      src_path: Fully qualified local path or full gs:// path of the src file.
      dest_path: Fully qualified local path or full gs:// path of the dest
                 file.
      acl: One of the google storage canned_acls to apply.
      version: If given, the generation; essentially the timestamp of the last
        update.  Note this is not the same as sequence-number; it's
        monotonically increasing bucket wide rather than reset per file.
        The usage of this is if we intend to replace/update only if the version
        is what we expect.  This is useful for distributed reasons- for example,
        to ensure you don't overwrite someone else's creation, a version of
        0 states "only update if no version exists".

    Raises:
      RunCommandError if the command failed despite retries.
    Returns:
      Return the CommandResult from the run.
    """
    cmd, headers = [], []

    if version is not None:
      headers = ['x-goog-if-generation-match:%d' % version]

    cmd.append('cp')

    acl = self.acl_file if acl is None else acl
    if acl is not None:
      cmd += ['-a', acl]

    cmd += ['--', src_path, dest_path]

    # For ease of testing, only pass headers if we got some.
    if headers:
      kwargs['headers'] = headers
    return self._DoCommand(cmd, redirect_stderr=True, **kwargs)

  def LS(self, path):
    """Does a directory listing of the given gs path."""
    return self._DoCommand(['ls', '--', path], redirect_stdout=True)

  def SetACL(self, upload_url, acl=None):
    """Set access on a file already in google storage.

    Args:
      upload_url: gs:// url that will have acl applied to it.
      acl: An ACL permissions file or canned ACL.
    """
    if acl is None:
      if not self.acl_file:
        raise GSContextException(
            "SetAcl invoked w/out a specified acl, nor a default acl.")
      acl = self.acl_file

    self._DoCommand(['setacl', acl, upload_url])

  def Exists(self, path):
    """Checks whether the given object exists.

    Args:
       path: Full gs:// url of the path to check.

    Returns:
      True if the path exists; otherwise returns False.
    """
    try:
      self._DoCommand(['getacl', path], redirect_stdout=True,
                      redirect_stderr=True)
    except GSNoSuchKey:
      return False
    return True

# Set GSUTIL_BIN now.
GSUTIL_BIN = GSContext.GetDefaultGSUtilBin()
