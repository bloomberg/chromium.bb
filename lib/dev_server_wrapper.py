# -*- coding: utf-8 -*-
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing methods and classes to interact with a devserver instance.
"""

from __future__ import print_function

import multiprocessing
import os
import re
import socket
import sys
import tempfile

from six.moves import http_client as httplib
from six.moves import urllib

from chromite.lib import constants
from chromite.cli import command
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import gs
from chromite.lib import osutils
from chromite.lib import path_util
from chromite.lib import remote_access
from chromite.lib import timeout_util


DEFAULT_PORT = 8080

DEVSERVER_PKG_DIR = os.path.join(constants.SOURCE_ROOT, 'src/platform/dev')
DEFAULT_STATIC_DIR = path_util.FromChrootPath(
    os.path.join(constants.SOURCE_ROOT, 'src', 'platform', 'dev', 'static'))

XBUDDY_REMOTE = 'remote'
XBUDDY_LOCAL = 'local'


class ImagePathError(Exception):
  """Raised when the provided path can't be resolved to an image."""


def ConvertTranslatedPath(original_path, translated_path):
  """Converts a translated xbuddy path to an xbuddy path.

  Devserver/xbuddy does not accept requests with translated xbuddy
  path (build-id/version/image-name). This function converts such a
  translated path to an xbuddy path that is suitable to used in
  devserver requests.

  Args:
    original_path: the xbuddy path before translation.
      (e.g., remote/peppy/latest-canary).
    translated_path: the translated xbuddy path
      (e.g., peppy-release/R36-5760.0.0).

  Returns:
    A xbuddy path uniquely identifies a build and can be used in devserver
      requests: {local|remote}/build-id/version/image_type
  """
  chunks = translated_path.split(os.path.sep)
  chunks[-1] = constants.IMAGE_NAME_TO_TYPE[chunks[-1]]

  if GetXbuddyPath(original_path).startswith(XBUDDY_REMOTE):
    chunks = [XBUDDY_REMOTE] + chunks
  else:
    chunks = [XBUDDY_LOCAL] + chunks

  return os.path.sep.join(chunks)


def GetXbuddyPath(path):
  """A helper function to parse an xbuddy path.

  Args:
    path: Either a path without no scheme or an xbuddy://path/for/xbuddy

  Returns:
    path/for/xbuddy if |path| is xbuddy://path/for/xbuddy; otherwise,
    returns |path|.

  Raises:
    ValueError if |path| uses any scheme other than xbuddy://.
  """
  parsed = urllib.parse.urlparse(path)

  if parsed.scheme == 'xbuddy':
    return '%s%s' % (parsed.netloc, parsed.path)
  elif parsed.scheme == '':
    logging.debug('Assuming %s is an xbuddy path.', path)
    return path
  else:
    raise ValueError('Do not support scheme %s.', parsed.scheme)


def GetImagePathWithXbuddy(path, board, version=None,
                           static_dir=DEFAULT_STATIC_DIR, lookup_only=False):
  """Gets image path and resolved XBuddy path using xbuddy.

  Ask xbuddy to translate |path|, and if necessary, download and stage the
  image, then return a translated path to the image. Also returns the resolved
  XBuddy path, which may be useful for subsequent calls in case the argument is
  an alias.

  Args:
    path: The xbuddy path.
    board: The default board to use if board is not specified in |path|.
    version: The default version to use if one is not specified in |path|.
    static_dir: Static directory to stage the image in.
    lookup_only: Caller only wants to translate the path not download the
      artifact.

  Returns:
    A tuple consisting of a translated path to the image
    (build-id/version/image_name) as well as the fully resolved XBuddy path (in
    the case where |path| is an XBuddy alias).
  """
  # Since xbuddy often wants to use gsutil from $PATH, make sure our local copy
  # shows up first.
  upath = os.environ['PATH'].split(os.pathsep)
  upath.insert(0, os.path.dirname(gs.GSContext.GetDefaultGSUtilBin()))
  os.environ['PATH'] = os.pathsep.join(upath)

  # Import xbuddy for translating, downloading and staging the image.
  if not os.path.exists(DEVSERVER_PKG_DIR):
    raise Exception('Cannot find xbuddy module. Devserver package directory '
                    'does not exist: %s' % DEVSERVER_PKG_DIR)
  sys.path.append(DEVSERVER_PKG_DIR)
  # pylint: disable=import-error
  import xbuddy
  import cherrypy

  # If we are using the progress bar, quiet the logging output of cherrypy.
  if command.UseProgressBar():
    if (hasattr(cherrypy.log, 'access_log') and
        hasattr(cherrypy.log, 'error_log')):
      cherrypy.log.access_log.setLevel(logging.NOTICE)
      cherrypy.log.error_log.setLevel(logging.NOTICE)
    else:
      cherrypy.config.update({'server.log_to_screen': False})

  xb = xbuddy.XBuddy(static_dir=static_dir, board=board, version=version,
                     log_screen=False)
  path_list = GetXbuddyPath(path).rsplit(os.path.sep)
  try:
    if lookup_only:
      build_id, file_name = xb.Translate(path_list)
    else:
      build_id, file_name = xb.Get(path_list)

    resolved_path, _ = xb.LookupAlias(os.path.sep.join(path_list))
    return os.path.join(build_id, file_name), resolved_path
  except xbuddy.XBuddyException as e:
    logging.error('Locating image "%s" failed. The path might not be valid or '
                  'the image might not exist.', path)
    raise ImagePathError('Cannot locate image %s: %s' % (path, e))


def GenerateXbuddyRequest(path, req_type):
  """Generate an xbuddy request used to retreive payloads.

  This function generates a xbuddy request based on |path| and
  |req_type|, which can be used to query the devserver. For request
  type 'image' ('update'), the devserver will repond with a URL
  pointing to the folder where the image (update payloads) is stored.

  Args:
    path: An xbuddy path (with or without xbuddy://).
    req_type: xbuddy request type ('update', 'image', or 'translate').

  Returns:
    A xbuddy request.
  """
  if req_type == 'update':
    return 'xbuddy/%s?for_update=true&return_dir=true' % GetXbuddyPath(path)
  elif req_type == 'image':
    return 'xbuddy/%s?return_dir=true' % GetXbuddyPath(path)
  elif req_type == 'translate':
    return 'xbuddy_translate/%s' % GetXbuddyPath(path)
  else:
    raise ValueError('Does not support xbuddy request type %s' % req_type)


def TranslatedPathToLocalPath(translated_path, static_dir):
  """Convert the translated path to a local path to the image file.

  Args:
    translated_path: the translated xbuddy path
      (e.g., peppy-release/R36-5760.0.0/chromiumos_image).
    static_dir: The static directory used by the devserver.

  Returns:
    A local path to the image file.
  """
  real_path = osutils.ExpandPath(os.path.join(static_dir, translated_path))

  if os.path.exists(real_path):
    return real_path
  else:
    return path_util.FromChrootPath(real_path)


def GenerateUpdateId(target, src, key, for_vm):
  """Returns a simple representation id of |target| and |src| paths.

  Args:
    target: Target image of the update payloads.
    src: Base image to of the delta update payloads.
    key: Private key used to sign the payloads.
    for_vm: Whether the update payloads are to be used in a VM .
  """
  update_id = target
  if src:
    update_id = '->'.join([src, update_id])

  if key:
    update_id = '+'.join([update_id, key])

  if not for_vm:
    update_id = '+'.join([update_id, 'patched_kernel'])

  return update_id


def GetIPv4Address(dev=None, global_ip=True):
  """Returns any global/host IP address or the IP address of the given device.

  socket.gethostname() is insufficient for machines where the host files are
  not set up "correctly."  Since some of our builders may have this issue,
  this method gives you a generic way to get the address so you are reachable
  either via a VM or remote machine on the same network.

  Args:
    dev: Get the IP address of the device (e.g. 'eth0').
    global_ip: If set True, returns a globally valid IP address. Otherwise,
      returns a local IP address (default: True).
  """
  cmd = ['ip', 'addr', 'show']
  cmd += ['scope', 'global' if global_ip else 'host']
  cmd += [] if dev is None else ['dev', dev]

  result = cros_build_lib.RunCommand(cmd, print_cmd=False, capture_output=True)
  matches = re.findall(r'\binet (\d+\.\d+\.\d+\.\d+).*', result.output)
  if matches:
    return matches[0]
  logging.warning('Failed to find ip address in %r', result.output)
  return None


class DevServerException(Exception):
  """Base exception class of devserver errors."""


class DevServerStartupError(DevServerException):
  """Thrown when the devserver fails to start up."""


class DevServerStopError(DevServerException):
  """Thrown when the devserver fails to stop."""


class DevServerResponseError(DevServerException):
  """Thrown when the devserver responds with an error."""


class DevServerConnectionError(DevServerException):
  """Thrown when unable to connect to devserver."""


class DevServerWrapper(multiprocessing.Process):
  """A Simple wrapper around a dev server instance."""

  # Wait up to 15 minutes for the dev server to start. It can take a
  # while to start when generating payloads in parallel.
  DEV_SERVER_TIMEOUT = 900
  KILL_TIMEOUT = 10

  def __init__(self, static_dir=None, port=None, log_dir=None, src_image=None,
               board=None):
    """Initialize a DevServerWrapper instance.

    Args:
      static_dir: The static directory to be used by the devserver.
      port: The port to used by the devserver.
      log_dir: Directory to store the log files.
      src_image: The path to the image to be used as the base to
        generate delta payloads.
      board: Override board to pass to the devserver for xbuddy pathing.
    """
    super(DevServerWrapper, self).__init__()
    self.devserver_bin = 'start_devserver'
    # Set port if it is given. Otherwise, devserver will start at any
    # available port.
    self.port = None if not port else port
    self.src_image = src_image
    self.board = board
    self.tempdir = None
    self.log_dir = log_dir
    if not self.log_dir:
      self.tempdir = osutils.TempDir(
          base_dir=path_util.FromChrootPath('/tmp'),
          prefix='devserver_wrapper',
          sudo_rm=True)
      self.log_dir = self.tempdir.tempdir
    self.static_dir = static_dir
    self.log_file = os.path.join(self.log_dir, 'dev_server.log')
    self.port_file = os.path.join(self.log_dir, 'dev_server.port')
    self._pid_file = self._GetPIDFilePath()
    self._pid = None

  @classmethod
  def DownloadFile(cls, url, dest):
    """Download the file from the URL to a local path."""
    if os.path.isdir(dest):
      dest = os.path.join(dest, os.path.basename(url))

    logging.info('Downloading %s to %s', url, dest)
    osutils.WriteFile(dest, DevServerWrapper.OpenURL(url), mode='wb')

  def GetURL(self, sub_dir=None):
    """Returns the URL of this devserver instance."""
    return self.GetDevServerURL(port=self.port, sub_dir=sub_dir)

  @classmethod
  def GetDevServerURL(cls, ip=None, port=None, sub_dir=None):
    """Returns the dev server url.

    Args:
      ip: IP address of the devserver. If not set, use the IP
        address of this machine.
      port: Port number of devserver.
      sub_dir: The subdirectory of the devserver url.
    """
    ip = GetIPv4Address() if not ip else ip
    # If port number is not given, assume 8080 for backward
    # compatibility.
    port = DEFAULT_PORT if not port else port
    url = 'http://%(ip)s:%(port)s' % {'ip': ip, 'port': str(port)}
    if sub_dir:
      url += '/' + sub_dir

    return url


  @classmethod
  def OpenURL(cls, url, ignore_url_error=False, timeout=60):
    """Returns the HTTP response of a URL."""
    logging.debug('Retrieving %s', url)
    try:
      res = urllib.request.urlopen(url, timeout=timeout)
    except (urllib.error.HTTPError, httplib.HTTPException) as e:
      logging.error('Devserver responded with HTTP error (%s)', e)
      raise DevServerResponseError(e)
    except (urllib.error.URLError, socket.timeout) as e:
      if not ignore_url_error:
        logging.error('Cannot connect to devserver (%s)', e)
        raise DevServerConnectionError(e)
    else:
      return res.read()

  @classmethod
  def WipeStaticDirectory(cls, static_dir):
    """Cleans up |static_dir|.

    Args:
      static_dir: path to the static directory of the devserver instance.
    """
    # Wipe the payload cache.
    cls.WipePayloadCache(static_dir=static_dir)
    logging.info('Cleaning up directory %s', static_dir)
    osutils.RmDir(static_dir, ignore_missing=True, sudo=True)

  @classmethod
  def WipePayloadCache(cls, devserver_bin='start_devserver', static_dir=None):
    """Cleans up devserver cache of payloads.

    Args:
      devserver_bin: path to the devserver binary.
      static_dir: path to use as the static directory of the devserver instance.
    """
    logging.info('Cleaning up previously generated payloads.')
    cmd = [devserver_bin, '--clear_cache', '--exit']
    if static_dir:
      cmd.append('--static_dir=%s' % path_util.ToChrootPath(static_dir))

    cros_build_lib.SudoRunCommand(
        cmd, enter_chroot=True, print_cmd=False, combine_stdout_stderr=True,
        redirect_stdout=True, redirect_stderr=True, cwd=constants.SOURCE_ROOT)

  def _ReadPortNumber(self):
    """Read port number from file."""
    if not self.is_alive():
      raise DevServerStartupError('Devserver is dead and has no port number')

    try:
      timeout_util.WaitForReturnTrue(os.path.exists,
                                     func_args=[self.port_file],
                                     timeout=self.DEV_SERVER_TIMEOUT,
                                     period=5)
    except timeout_util.TimeoutError:
      self.terminate()
      raise DevServerStartupError('Timeout (%s) waiting for devserver '
                                  'port_file' % self.DEV_SERVER_TIMEOUT)

    self.port = int(osutils.ReadFile(self.port_file).strip())

  def IsReady(self):
    """Check if devserver is up and running."""
    if not self.is_alive():
      raise DevServerStartupError('Devserver is not ready because it died')

    url = os.path.join('http://%s:%d' % (remote_access.LOCALHOST_IP, self.port),
                       'check_health')
    if self.OpenURL(url, ignore_url_error=True, timeout=2):
      return True

    return False

  def _GetPIDFilePath(self):
    """Returns pid file path."""
    return tempfile.NamedTemporaryFile(prefix='devserver_wrapper',
                                       dir=self.log_dir,
                                       delete=False).name

  def _GetPID(self):
    """Returns the pid read from the pid file."""
    # Pid file was passed into the chroot.
    return osutils.ReadFile(self._pid_file).rstrip()

  def _WaitUntilStarted(self):
    """Wait until the devserver has started."""
    if not self.port:
      self._ReadPortNumber()

    try:
      timeout_util.WaitForReturnTrue(self.IsReady,
                                     timeout=self.DEV_SERVER_TIMEOUT,
                                     period=5)
    except timeout_util.TimeoutError:
      self.terminate()
      raise DevServerStartupError('Devserver did not start')

  def run(self):
    """Kicks off devserver in a separate process and waits for it to finish."""
    # Truncate the log file if it already exists.
    if os.path.exists(self.log_file):
      osutils.SafeUnlink(self.log_file, sudo=True)

    path_resolver = path_util.ChrootPathResolver()

    port = self.port if self.port else 0
    cmd = [self.devserver_bin,
           '--pidfile', path_resolver.ToChroot(self._pid_file),
           '--logfile', path_resolver.ToChroot(self.log_file),
           '--port=%d' % port,
           '--critical_update']

    if not self.port:
      cmd.append('--portfile=%s' % path_resolver.ToChroot(self.port_file))

    if self.static_dir:
      cmd.append(
          '--static_dir=%s' % path_resolver.ToChroot(self.static_dir))

    if self.src_image:
      cmd.append('--src_image=%s' % path_resolver.ToChroot(self.src_image))

    if self.board:
      cmd.append('--board=%s' % self.board)

    chroot_args = ['--no-ns-pid']
    # The chromite bin directory is needed for cros_generate_update_payload.
    extra_env = {
        'PATH': '%s:%s' % (os.environ['PATH'],
                           path_resolver.ToChroot(constants.CHROMITE_BIN_DIR))}
    result = self._RunCommand(
        cmd, enter_chroot=True, chroot_args=chroot_args,
        cwd=constants.SOURCE_ROOT, extra_env=extra_env, error_code_ok=True,
        redirect_stdout=True, combine_stdout_stderr=True)
    if result.returncode != 0:
      msg = ('Devserver failed to start!\n'
             '--- Start output from the devserver startup command ---\n'
             '%s'
             '--- End output from the devserver startup command ---' %
             result.output)
      logging.error(msg)

  def Start(self):
    """Starts a background devserver and waits for it to start.

    Starts a background devserver and waits for it to start. Will only return
    once devserver has started and running pid has been read.
    """
    self.start()
    self._WaitUntilStarted()
    self._pid = self._GetPID()

  def Stop(self):
    """Kills the devserver instance with SIGTERM and SIGKILL if SIGTERM fails"""
    if not self._pid:
      logging.debug('No devserver running.')
      return

    logging.debug('Stopping devserver instance with pid %s', self._pid)
    if self.is_alive():
      self._RunCommand(['kill', self._pid], error_code_ok=True)
    else:
      logging.debug('Devserver not running')
      return

    self.join(self.KILL_TIMEOUT)
    if self.is_alive():
      logging.warning('Devserver is unstoppable. Killing with SIGKILL')
      try:
        self._RunCommand(['kill', '-9', self._pid])
      except cros_build_lib.RunCommandError as e:
        raise DevServerStopError('Unable to stop devserver: %s' % e)

  def PrintLog(self):
    """Print devserver output to stdout."""
    print(self.TailLog(num_lines='+1'))

  def TailLog(self, num_lines=50):
    """Returns the most recent |num_lines| lines of the devserver log file."""
    fname = self.log_file
    # We use self._RunCommand here to check the existence of the log file.
    if self._RunCommand(
        ['test', '-f', fname], error_code_ok=True).returncode == 0:
      result = self._RunCommand(['tail', '-n', str(num_lines), fname],
                                capture_output=True)
      output = '--- Start output from %s ---' % fname
      output += result.output
      output += '--- End output from %s ---' % fname
      return output

  def _RunCommand(self, *args, **kwargs):
    """Runs a shell commmand."""
    kwargs.setdefault('debug_level', logging.DEBUG)
    return cros_build_lib.SudoRunCommand(*args, **kwargs)
