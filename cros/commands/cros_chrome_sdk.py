# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The cros chrome-sdk command for the simple chrome workflow."""

import argparse
import collections
import contextlib
import json
import logging
import os
import distutils.version

from chromite import cros
from chromite.lib import cache
from chromite.lib import chrome_util
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import git
from chromite.lib import gs
from chromite.lib import osutils
from chromite.lib import stats
from chromite.buildbot import cbuildbot_config
from chromite.buildbot import constants


COMMAND_NAME = 'chrome-sdk'
CUSTOM_VERSION = 'custom'


def Log(*args, **kwargs):
  """Conditional logging.

  Args:
    silent: If set to True, then logs with level DEBUG.  logs with level INFO
      otherwise.  Defaults to False.
  """
  silent = kwargs.pop('silent', False)
  level = logging.DEBUG if silent else logging.INFO
  logging.log(level, *args, **kwargs)


class MissingSDK(Exception):
  """Error thrown when we cannot find an SDK."""

  def __init__(self, board, version=None):
    msg = 'Cannot find SDK for %r' % (board,)
    if version is not None:
      msg += ' with version %s' % (version,)
    Exception.__init__(self, msg)


class SDKFetcher(object):
  """Functionality for fetching an SDK environment.

  For the version of ChromeOS specified, the class downloads and caches
  SDK components.
  """
  SDK_BOARD_ENV = '%SDK_BOARD'
  SDK_PATH_ENV = '%SDK_PATH'
  SDK_VERSION_ENV = '%SDK_VERSION'

  SDKContext = collections.namedtuple(
      'SDKContext', ['version', 'target_tc', 'key_map'])

  TARBALL_CACHE = 'tarballs'
  MISC_CACHE = 'misc'

  TARGET_TOOLCHAIN_KEY = 'target_toolchain'

  def __init__(self, cache_dir, board, clear_cache=False, chrome_src=None,
               sdk_path=None, silent=False):
    """Initialize the class.

    Args:
      cache_dir: The toplevel cache dir to use.
      board: The board to manage the SDK for.
      clear_cache: Clears the sdk cache during __init__.
      chrome_src: The location of the chrome checkout.  If unspecified, the
        cwd is presumed to be within a chrome checkout.
      sdk_path: The path (whether a local directory or a gs:// path) to fetch
        SDK components from.
      silent: If set, the fetcher prints less output.
    """
    self.cache_base = os.path.join(cache_dir, COMMAND_NAME)
    if clear_cache:
      logging.warning('Clearing the SDK cache.')
      osutils.RmDir(self.cache_base, ignore_missing=True)
    self.tarball_cache = cache.TarballCache(
        os.path.join(self.cache_base, self.TARBALL_CACHE))
    self.misc_cache = cache.DiskCache(
        os.path.join(self.cache_base, self.MISC_CACHE))
    self.board = board
    self.config = cbuildbot_config.FindCanonicalConfigForBoard(board)
    self.gs_base = '%s/%s' % (constants.DEFAULT_ARCHIVE_BUCKET,
                              self.config['name'])
    self.clear_cache = clear_cache
    self.chrome_src = chrome_src
    self.sdk_path = sdk_path
    self.silent = silent

    # For external configs, there is no need to run 'gsutil config', because
    # the necessary files are all accessible to anonymous users.
    internal = self.config['internal']
    self.gs_ctx = gs.GSContext(cache_dir=cache_dir, init_boto=internal)

    if self.sdk_path is None:
      self.sdk_path = os.environ.get(self.SDK_PATH_ENV)

  def _UpdateTarball(self, url, ref):
    """Worker function to fetch tarballs"""
    with osutils.TempDir(base_dir=self.tarball_cache.staging_dir) as tempdir:
      local_path = os.path.join(tempdir, os.path.basename(url))
      Log('SDK: Fetching %s', url, silent=self.silent)
      self.gs_ctx.Copy(url, tempdir, debug_level=logging.DEBUG)
      ref.SetDefault(local_path, lock=True)

  def _GetMetadata(self, version):
    """Return metadata (in the form of a dict) for a given version."""
    raw_json = None
    version_base = self._GetVersionGSBase(version)
    with self.misc_cache.Lookup(
        self._GetCacheKeyForComponent(version, constants.METADATA_JSON)) as ref:
      if ref.Exists(lock=True):
        raw_json = osutils.ReadFile(ref.path)
      else:
        raw_json = self.gs_ctx.Cat(
            os.path.join(version_base, constants.METADATA_JSON),
                         debug_level=logging.DEBUG).output
        ref.AssignText(raw_json)

    return json.loads(raw_json)

  def _GetChromeLKGM(self, chrome_src_dir):
    """Get ChromeOS LKGM checked into the Chrome tree.

    Returns:
      Version number in format '3929.0.0'.
    """
    version = osutils.ReadFile(os.path.join(
        chrome_src_dir, constants.PATH_TO_CHROME_LKGM))
    return version

  def _GetRepoCheckoutVersion(self, repo_root):
    """Get the version specified in chromeos_version.sh.

    Returns:
      Version number in format '3929.0.0'.
    """
    chromeos_version_sh = os.path.join(repo_root, constants.VERSION_FILE)
    sourced_env = osutils.SourceEnvironment(
        chromeos_version_sh, ['CHROMEOS_VERSION_STRING'],
        env={'CHROMEOS_OFFICIAL': '1'})
    return sourced_env['CHROMEOS_VERSION_STRING']

  def _GetNewestFullVersion(self, version=None):
    """Gets the full version number of the latest build for the given |version|.

    Args:
      version: The version number or branch to look at. By default, look at
        builds on the current branch.

    Returns:
      Version number in the format 'R30-3929.0.0'.
    """
    if version is None:
      version = git.GetChromiteTrackingBranch()
    version_file = '%s/LATEST-%s' % (self.gs_base, version)
    try:
      full_version = self.gs_ctx.Cat(version_file).output
      assert full_version.startswith('R')
      return full_version
    except gs.GSNoSuchKey:
      return None

  def _GetNewestManifestVersion(self):
    """Gets the latest uploaded SDK version.

    Returns:
      Version number in the format '3929.0.0'.
    """
    full_version = self._GetNewestFullVersion()
    return None if full_version is None else full_version.split('-')[1]

  def GetDefaultVersion(self):
    """Get the default SDK version to use.

    If we are in an existing SDK shell, the default version will just be
    the current version. Otherwise, we will try to calculate the
    appropriate version to use based on the checkout.
    """
    if os.environ.get(self.SDK_BOARD_ENV) == self.board:
      sdk_version = os.environ.get(self.SDK_VERSION_ENV)
      if sdk_version is not None:
        return sdk_version

    with self.misc_cache.Lookup((self.board, 'latest')) as ref:
      if ref.Exists(lock=True):
        version = osutils.ReadFile(ref.path).strip()
        # Deal with the old version format.
        if version.startswith('R'):
          version = version.split('-')[1]
        return version
      else:
        return None

  def _SetDefaultVersion(self, version):
    """Set the new default version."""
    with self.misc_cache.Lookup((self.board, 'latest')) as ref:
      ref.AssignText(version)

  def UpdateDefaultVersion(self):
    """Update the version that we default to using.

    Returns:
      A tuple of the form (version, updated), where |version| is the
      version number in the format '3929.0.0', and |updated| indicates
      whether the version was indeed updated.
    """
    checkout_dir = self.chrome_src if self.chrome_src else os.getcwd()
    checkout = commandline.DetermineCheckout(checkout_dir)
    current = self.GetDefaultVersion() or '0'
    if checkout.chrome_src_dir:
      target = self._GetChromeLKGM(checkout.chrome_src_dir)
    elif checkout.type == commandline.CHECKOUT_TYPE_REPO:
      target = self._GetRepoCheckoutVersion(checkout.root)
      if target != current:
        lv_cls = distutils.version.LooseVersion
        if lv_cls(target) > lv_cls(current):
          # Hit the network for the newest uploaded version for the branch.
          newest = self._GetNewestManifestVersion()
          # The SDK for the version of the checkout has not been uploaded yet,
          # so fall back to the latest uploaded SDK.
          if newest is not None and lv_cls(target) > lv_cls(newest):
            target = newest
    else:
      target = self._GetNewestManifestVersion()

    if target is None:
      raise MissingSDK(self.board)

    self._SetDefaultVersion(target)
    return target, target != current

  def GetFullVersion(self, version):
    """Add the release branch to a ChromeOS platform version.

    Args:
      version: A ChromeOS platform number of the form XXXX.XX.XX, i.e.,
        3918.0.0.

    Returns:
      The version with release branch prepended.  I.e., R28-3918.0.0.
    """
    assert not version.startswith('R')

    with self.misc_cache.Lookup(('full-version', version)) as ref:
      if ref.Exists(lock=True):
        return osutils.ReadFile(ref.path).strip()
      else:
        # Find out the newest version from the LATEST (or LATEST-%s) file.
        full_version = self._GetNewestFullVersion(version=version)

        if full_version is None:
          raise MissingSDK(self.board, version)

        ref.AssignText(full_version)
        return full_version

  def _GetVersionGSBase(self, version):
    """The base path of the SDK for a particular version."""
    if self.sdk_path is not None:
      return self.sdk_path

    full_version = self.GetFullVersion(version)
    return os.path.join(self.gs_base, full_version)

  def _GetCacheKeyForComponent(self, version, component):
    """Builds the cache key tuple for an SDK component."""
    version_section = version
    if self.sdk_path is not None:
      version_section = self.sdk_path.replace('/', '__')
    return (self.board, version_section, component)

  @contextlib.contextmanager
  def Prepare(self, components, version=None, target_tc=None,
              toolchain_url=None):
    """Ensures the components of an SDK exist and are read-locked.

    For a given SDK version, pulls down missing components, and provides a
    context where the components are read-locked, which prevents the cache from
    deleting them during its purge operations.

    If both target_tc and toolchain_url arguments are provided, then this
    does not download metadata.json for the given version. Otherwise, this
    function requires metadata.json for the given version to exist.

    Args:
      gs_ctx: GSContext object.
      components: A list of specific components(tarballs) to prepare.
      version: The version to prepare.  If not set, uses the version returned by
        GetDefaultVersion().  If there is no default version set (this is the
        first time we are being executed), then we update the default version.
      target_tc: Target toolchain name to use, e.g. x86_64-cros-linux-gnu
      toolchain_url: Format pattern for path to fetch toolchain from,
        e.g. 2014/04/%(target)s-2014.04.23.220740.tar.xz

    Yields:
      An SDKFetcher.SDKContext namedtuple object.  The attributes of the
      object are:
        version: The version that was prepared.
        target_tc: Target toolchain name.
        key_map: Dictionary that contains CacheReference objects for the SDK
          artifacts, indexed by cache key.
    """
    if version is None and self.sdk_path is None:
      version = self.GetDefaultVersion()
      if version is None:
        version, _ = self.UpdateDefaultVersion()
    components = list(components)

    key_map = {}
    fetch_urls = {}

    if not target_tc or not toolchain_url:
      metadata = self._GetMetadata(version)
      target_tc = target_tc or metadata['toolchain-tuple'][0]
      toolchain_url = toolchain_url or metadata['toolchain-url']

    # Fetch toolchains from separate location.
    if self.TARGET_TOOLCHAIN_KEY in components:
      fetch_urls[self.TARGET_TOOLCHAIN_KEY] = os.path.join(
          'gs://', constants.SDK_GS_BUCKET,
          toolchain_url % {'target': target_tc})
      components.remove(self.TARGET_TOOLCHAIN_KEY)

    version_base = self._GetVersionGSBase(version)
    fetch_urls.update((t, os.path.join(version_base, t)) for t in components)
    try:
      for key, url in fetch_urls.iteritems():
        cache_key = self._GetCacheKeyForComponent(version, key)
        ref = self.tarball_cache.Lookup(cache_key)
        key_map[key] = ref
        ref.Acquire()
        if not ref.Exists(lock=True):
          # TODO(rcui): Parallelize this.  Requires acquiring locks *before*
          # generating worker processes; therefore the functionality needs to
          # be moved into the DiskCache class itself -
          # i.e.,DiskCache.ParallelSetDefault().
          self._UpdateTarball(url, ref)

      ctx_version = version
      if self.sdk_path is not None:
        ctx_version = CUSTOM_VERSION
      yield self.SDKContext(ctx_version, target_tc, key_map)
    finally:
      # TODO(rcui): Move to using cros_build_lib.ContextManagerStack()
      cros_build_lib.SafeRun([ref.Release for ref in key_map.itervalues()])


class GomaError(Exception):
  """Indicates error with setting up Goma."""


class ClangError(Exception):
  """Indicates error with setting up Clang."""
  pass


@cros.CommandDecorator(COMMAND_NAME)
class ChromeSDKCommand(cros.CrosCommand):
  """Set up an environment for building Chrome on Chrome OS.

  Pulls down SDK components for building and testing Chrome for Chrome OS,
  sets up the environment for building Chrome, and runs a command in the
  environment, starting a bash session if no command is specified.

  The bash session environment is set up by a user-configurable rc file located
  at ~/.chromite/chrome_sdk.bashrc.
  """

  # Note, this URL is not accessible outside of corp.
  _GOMA_URL = ('https://clients5.google.com/cxx-compiler-service/'
               'download/goma_ctl.py')

  _CLANG_DIR = 'third_party/llvm-build/Release+Asserts/bin'
  _CLANG_UPDATE_SH = 'tools/clang/scripts/update.sh'

  EBUILD_ENV = (
      'CXX',
      'CC',
      'AR',
      'AS',
      'LD',
      'RANLIB',
      'GOLD_SET',
      'GYP_DEFINES',
  )

  SDK_GOMA_PORT_ENV = 'SDK_GOMA_PORT'
  SDK_GOMA_DIR_ENV = 'SDK_GOMA_DIR'

  GOMACC_PORT_CMD  = ['./gomacc', 'port']
  FETCH_GOMA_CMD  = ['wget', _GOMA_URL]

  # Override base class property to enable stats upload.
  upload_stats = True

  # Override base class property to use cache related commandline options.
  use_caching_options = True

  @property
  def upload_stats_timeout(self):
    # Give a longer timeout for interactive SDK shell invocations, since the
    # user will not notice a longer wait because it's happening in the
    # background.
    if self.options.cmd:
      return super(ChromeSDKCommand, self).upload_stats_timeout
    else:
      return stats.StatsUploader.UPLOAD_TIMEOUT

  @staticmethod
  def ValidateVersion(version):
    if version.startswith('R') or len(version.split('.')) != 3:
      raise argparse.ArgumentTypeError(
          '--version should be in the format 3912.0.0')
    return version

  @classmethod
  def AddParser(cls, parser):
    def ExpandGSPath(path):
      """Expand a path, possibly a gs:// URL."""
      if path.startswith(gs.BASE_GS_URL):
        return path
      return osutils.ExpandPath(path)

    super(ChromeSDKCommand, cls).AddParser(parser)
    parser.add_argument(
        '--board', required=True, help='The board SDK to use.')
    parser.add_argument(
        '--bashrc', type=osutils.ExpandPath,
        default=constants.CHROME_SDK_BASHRC,
        help='A bashrc file used to set up the SDK shell environment. '
             'Defaults to %s.' % constants.CHROME_SDK_BASHRC)
    parser.add_argument(
        '--chroot', type=osutils.ExpandPath,
        help='Path to a ChromeOS chroot to use.  If set, '
             '<chroot>/build/<board> will be used as the sysroot that Chrome '
             'is built against.  The version shown in the SDK shell prompt '
             'will then have an asterisk prepended to it.')
    parser.add_argument(
        '--chrome-src', type=osutils.ExpandPath,
        help='Specifies the location of a Chrome src/ directory.  Required if '
             'running with --clang if not running from a Chrome checkout.')
    parser.add_argument(
        '--clang', action='store_true', default=False,
        help='Sets up the environment for building with clang.  Due to a bug '
             'with ninja, requires --make and possibly --chrome-src to be set.')
    parser.add_argument(
        '--cwd', type=osutils.ExpandPath,
        help='Specifies a directory to switch to after setting up the SDK '
             'shell.  Defaults to the current directory.')
    parser.add_argument(
        '--internal', action='store_true', default=False,
        help='Sets up SDK for building official (internal) Chrome '
             'Chrome, rather than Chromium.')
    parser.add_argument(
        '--sdk-path', type=ExpandGSPath,
        help='Provides a path, whether a local directory or a gs:// path, to '
             'pull SDK components from.')
    parser.add_argument(
        '--make', action='store_true', default=False,
        help='If set, gyp_chromium will generate Make files instead of Ninja '
             'files.  Note: Make files are spread out through the source tree, '
             'and not concentrated in the out_<board> directory, so you can '
             'only have one Make config running at a time.')
    parser.add_argument(
        '--nogoma', action='store_false', default=True, dest='goma',
        help="Disables Goma in the shell by removing it from the PATH.")
    parser.add_argument(
        '--version', default=None, type=cls.ValidateVersion,
        help="Specify version of SDK to use, in the format '3912.0.0'.  "
             "Defaults to determining version based on the type of checkout "
             "(Chrome or ChromeOS) you are executing from.")
    parser.add_argument(
        'cmd', nargs='*', default=None,
        help='The command to execute in the SDK environment.  Defaults to '
              'starting a bash shell.')

    parser.add_option_to_group(
        parser.caching_group, '--clear-sdk-cache', action='store_true',
        default=False,
        help='Removes everything in the SDK cache before starting.')

    group = parser.add_option_group('Metadata Overrides (Advanced)',
        description='Provide all of these overrides in order to remove '
                    'dependencies on metadata.json existence.')
    parser.add_option_to_group(
        group, '--target-tc', action='store', default=None,
        help='Override target toolchain name, e.g. x86_64-cros-linux-gnu')
    parser.add_option_to_group(
        group, '--toolchain-url', action='store', default=None,
        help='Override toolchain url format pattern, e.g. '
             '2014/04/%%(target)s-2014.04.23.220740.tar.xz')

  def __init__(self, options):
    cros.CrosCommand.__init__(self, options)
    self.board = options.board
    # Lazy initialized.
    self.sdk = None
    # Initialized later based on options passed in.
    self.silent = True

  @staticmethod
  def _CreatePS1(board, version, chroot=None):
    """Returns PS1 string that sets commandline and xterm window caption.

    If a chroot path is set, then indicate we are using the sysroot from there
    instead of the stock sysroot by prepending an asterisk to the version.

    Args:
      board: The SDK board.
      version: The SDK version.
      chroot: The path to the chroot, if set.
    """
    custom = '*' if chroot else ''
    sdk_version = '(sdk %s %s%s)' % (board, custom, version)
    label = '\\u@\\h: \\w'
    window_caption = "\\[\\e]0;%(sdk_version)s %(label)s \\a\\]"
    command_line = "%(sdk_version)s \\[\\e[1;33m\\]%(label)s \\$ \\[\\e[m\\]"
    ps1 = window_caption + command_line
    return (ps1 % {'sdk_version': sdk_version,
                   'label': label})

  def _FixGoldPath(self, var_contents, toolchain_path):
    """Point to the gold linker in the toolchain tarball.

    Accepts an already set environment variable in the form of '<cmd>
    -B<gold_path>', and overrides the gold_path to the correct path in the
    extracted toolchain tarball.

    Args:
      var_contents: The contents of the environment variable.
      toolchain_path: Path to the extracted toolchain tarball contents.

    Returns:
      Environment string that has correct gold path.
    """
    cmd, _, gold_path = var_contents.partition(' -B')
    gold_path = os.path.join(toolchain_path, gold_path.lstrip('/'))
    return '%s -B%s' % (cmd, gold_path)

  def _SetupTCEnvironment(self, sdk_ctx, options, env):
    """Sets up toolchain-related environment variables."""
    target_tc_path = sdk_ctx.key_map[self.sdk.TARGET_TOOLCHAIN_KEY].path
    tc_bin_path = os.path.join(target_tc_path, 'bin')
    env['PATH'] = '%s:%s' % (tc_bin_path, os.environ['PATH'])

    for var in ('CXX', 'CC', 'LD'):
      env[var] = self._FixGoldPath(env[var], target_tc_path)

    if options.clang:
      # clang++ requires C++ header paths to be explicitly specified.
      # See discussion on crbug.com/86037.
      target_tc = sdk_ctx.target_tc
      gcc_path = os.path.join(tc_bin_path, '%s-gcc' % target_tc)
      gcc_version = cros_build_lib.DebugRunCommand(
          [gcc_path, '-dumpversion'], redirect_stdout=True).output.strip()
      gcc_lib = 'usr/lib/gcc/%(targ)s/%(ver)s/include/g++-v%(major_ver)s' % {
          'targ': target_tc,
          'ver': gcc_version,
          'major_ver': gcc_version[0],
      }
      tc_gcc_lib = os.path.join(target_tc_path, gcc_lib)
      includes = []
      for p in ('',  target_tc, 'backward'):
        includes.append('-isystem %s' % os.path.join(tc_gcc_lib, p))
      env['CC'] = 'clang'
      env['CXX'] = 'clang++ %s' % ' '.join(includes)

    env['CXX_host'] = 'g++'
    env['CC_host'] = 'gcc'

    if options.clang:
      clang_path = os.path.join(options.chrome_src, self._CLANG_DIR)
      env['PATH'] = '%s:%s' % (clang_path, env['PATH'])

  def _SetupEnvironment(self, board, sdk_ctx, options, goma_dir=None,
                        goma_port=None):
    """Sets environment variables to export to the SDK shell."""
    if options.chroot:
      sysroot = os.path.join(options.chroot, 'build', board)
      if not os.path.isdir(sysroot) and not options.cmd:
        logging.warning("Because --chroot is set, expected a sysroot to be at "
                        "%s, but couldn't find one.", sysroot)
    else:
      sysroot = sdk_ctx.key_map[constants.CHROME_SYSROOT_TAR].path

    environment = os.path.join(sdk_ctx.key_map[constants.CHROME_ENV_TAR].path,
                               'environment')
    env = osutils.SourceEnvironment(environment, self.EBUILD_ENV)
    self._SetupTCEnvironment(sdk_ctx, options, env)

    # Add managed components to the PATH.
    env['PATH'] = '%s:%s' % (constants.CHROMITE_BIN_DIR, env['PATH'])
    env['PATH'] = '%s:%s' % (os.path.dirname(self.sdk.gs_ctx.gsutil_bin),
                             env['PATH'])

    # Export internally referenced variables.
    os.environ[self.sdk.SDK_BOARD_ENV] = board
    if self.options.sdk_path:
      os.environ[self.sdk.SDK_PATH_ENV] = self.options.sdk_path
    os.environ[self.sdk.SDK_VERSION_ENV] = sdk_ctx.version

    # Export the board/version info in a more accessible way, so developers can
    # reference them in their chrome_sdk.bashrc files, as well as within the
    # chrome-sdk shell.
    for var in [self.sdk.SDK_VERSION_ENV, self.sdk.SDK_BOARD_ENV]:
      env[var.lstrip('%')] = os.environ[var]

    # Export Goma information.
    if goma_dir:
      env[self.SDK_GOMA_DIR_ENV] = goma_dir
      env[self.SDK_GOMA_PORT_ENV] = goma_port

    # SYSROOT is necessary for Goma and the sysroot wrapper.
    env['SYSROOT'] = sysroot
    gyp_dict = chrome_util.ProcessGypDefines(env['GYP_DEFINES'])
    gyp_dict['sysroot'] = sysroot
    gyp_dict.pop('order_text_section', None)
    if options.clang:
      gyp_dict['clang'] = 1
      gyp_dict['werror'] = ''
      gyp_dict['clang_use_chrome_plugins'] = 0
      gyp_dict['linux_use_tcmalloc'] = 0
    if options.internal:
      gyp_dict['branding'] = 'Chrome'
      gyp_dict['buildtype'] = 'Official'
    else:
      gyp_dict.pop('branding', None)
      gyp_dict.pop('buildtype', None)

    # Enable goma if requested.
    if goma_dir:
      gyp_dict['use_goma'] = 1
      gyp_dict['gomadir'] = goma_dir

    env['GYP_DEFINES'] = chrome_util.DictToGypDefines(gyp_dict)

    # PS1 sets the command line prompt and xterm window caption.
    full_version = sdk_ctx.version
    if full_version != CUSTOM_VERSION:
      full_version = self.sdk.GetFullVersion(sdk_ctx.version)
    env['PS1'] = self._CreatePS1(self.board, full_version,
                                 chroot=options.chroot)

    out_dir = 'out_%s' % self.board
    env['builddir_name'] = out_dir
    env['GYP_GENERATORS'] = 'make' if options.make else 'ninja'
    env['GYP_GENERATOR_FLAGS'] = 'output_dir=%s' % out_dir
    return env

  @staticmethod
  def _VerifyGoma(user_rc):
    """Verify that the user has no goma installations set up in user_rc.

    If the user does have a goma installation set up, verify that it's for
    ChromeOS.

    Args:
      user_rc: User-supplied rc file.
    """
    user_env = osutils.SourceEnvironment(user_rc, ['PATH'])
    goma_ctl = osutils.Which('goma_ctl.py', user_env.get('PATH'))
    if goma_ctl is not None:
      logging.warning(
          '%s is adding Goma to the PATH.  Using that Goma instead of the '
          'managed Goma install.', user_rc)

  @staticmethod
  def _VerifyChromiteBin(user_rc):
    """Verify that the user has not set a chromite bin/ dir in user_rc.

    Args:
      user_rc: User-supplied rc file.
    """
    user_env = osutils.SourceEnvironment(user_rc, ['PATH'])
    chromite_bin = osutils.Which('parallel_emerge', user_env.get('PATH'))
    if chromite_bin is not None:
      logging.warning(
          '%s is adding chromite/bin to the PATH.  Remove it from the PATH to '
          'use the the default Chromite.', user_rc)

  @staticmethod
  def _VerifyClang(user_rc):
    """Verify that the user has not set a clang bin/ dir in user_rc.

    Args:
      user_rc: User-supplied rc file.
    """
    user_env = osutils.SourceEnvironment(user_rc, ['PATH'])
    clang_bin = osutils.Which('clang', user_env.get('PATH'))
    if clang_bin is not None:
      clang_dir = os.path.dirname(clang_bin)
      if not osutils.Which('goma_ctl.py', clang_dir):
        logging.warning(
            '%s is adding Clang to the PATH.  Because of this, Goma is being '
            'bypassed.  Remove it from the PATH to use Goma with the default '
            'Clang.', user_rc)

  @contextlib.contextmanager
  def _GetRCFile(self, env, user_rc):
    """Returns path to dynamically created bashrc file.

    The bashrc file sets the environment variables contained in |env|, as well
    as sources the user-editable chrome_sdk.bashrc file in the user's home
    directory.  That rc file is created if it doesn't already exist.

    Args:
      env: A dictionary of environment variables that will be set by the rc
        file.
      user_rc: User-supplied rc file.
    """
    if not os.path.exists(user_rc):
      osutils.Touch(user_rc, makedirs=True)

    self._VerifyGoma(user_rc)
    self._VerifyChromiteBin(user_rc)
    if self.options.clang:
      self._VerifyClang(user_rc)

    # We need a temporary rc file to 'wrap' the user configuration file,
    # because running with '--rcfile' causes bash to ignore bash special
    # variables passed through subprocess.Popen, such as PS1.  So we set them
    # here.
    #
    # Having a wrapper rc file will also allow us to inject bash functions into
    # the environment, not just variables.
    with osutils.TempDir() as tempdir:
      # Only source the user's ~/.bashrc if running in interactive mode.
      contents = [
          '[[ -e ~/.bashrc && $- == *i* ]] && . ~/.bashrc\n',
      ]

      for key, value in env.iteritems():
        contents.append("export %s='%s'\n" % (key, value))
      contents.append('. "%s"\n' % user_rc)

      rc_file = os.path.join(tempdir, 'rcfile')
      osutils.WriteFile(rc_file, contents)
      yield rc_file

  def _GomaPort(self, goma_dir):
    """Returns current active Goma port."""
    port = cros_build_lib.RunCommand(
        self.GOMACC_PORT_CMD, cwd=goma_dir, debug_level=logging.DEBUG,
        error_code_ok=True, capture_output=True).output.strip()
    return port

  def _FetchGoma(self):
    """Fetch, install, and start Goma, using cached version if it exists.

    Returns:
      A tuple (dir, port) containing the path to the cached goma/ dir and the
      Goma port.
    """
    common_path = os.path.join(self.options.cache_dir, constants.COMMON_CACHE)
    common_cache = cache.DiskCache(common_path)

    ref = common_cache.Lookup(('goma', '2'))
    if not ref.Exists():
      Log('Installing Goma.', silent=self.silent)
      with osutils.TempDir() as tempdir:
        goma_dir = os.path.join(tempdir, 'goma')
        os.mkdir(goma_dir)
        result = cros_build_lib.DebugRunCommand(
            self.FETCH_GOMA_CMD, cwd=goma_dir, error_code_ok=True)
        if result.returncode:
          raise GomaError('Failed to fetch Goma')
       # Update to latest version of goma. We choose the outside-chroot version
       # ('goobuntu') over the chroot version ('chromeos') by supplying
       # input='1' to the following prompt:
       #
       # What is your platform?
       #  1. Goobuntu  2. Precise (32bit)  3. Lucid (32bit)  4. Debian
       #  5. Chrome OS  6. MacOS ? -->
        cros_build_lib.DebugRunCommand(
            ['python', 'goma_ctl.py', 'update'], cwd=goma_dir, input='1\n')
        ref.SetDefault(goma_dir)
    goma_dir = ref.path

    Log('Starting Goma.', silent=self.silent)
    cros_build_lib.DebugRunCommand(
        ['python', 'goma_ctl.py', 'ensure_start'], cwd=goma_dir)
    port = self._GomaPort(goma_dir)
    Log('Goma is started on port %s', port, silent=self.silent)
    if not port:
      raise GomaError('No Goma port detected')

    return goma_dir, port

  def _SetupClang(self):
    """Install clang if needed."""
    clang_path = os.path.join(self.options.chrome_src, self._CLANG_DIR)
    if not os.path.exists(clang_path):
      try:
        update_sh = os.path.join(self.options.chrome_src, self._CLANG_UPDATE_SH)
        if not os.path.isfile(update_sh):
          raise ClangError('%s not found.' % update_sh)
        results = cros_build_lib.DebugRunCommand(
            [update_sh], cwd=self.options.chrome_src, error_code_ok=True)
        if results.returncode:
          raise ClangError('Clang update failed with error code %s' %
                           (results.returncode,))
        if not os.path.exists(clang_path):
          raise ClangError('%s not found.' % clang_path)
      except ClangError as e:
        logging.error('Encountered errors while installing/updating clang: %s',
                      e)

  def Run(self):
    """Perform the command."""
    if os.environ.get(SDKFetcher.SDK_VERSION_ENV) is not None:
      cros_build_lib.Die('Already in an SDK shell.')

    if self.options.clang and not self.options.make:
      cros_build_lib.Die('--clang requires --make to be set.')

    if not self.options.chrome_src:
      checkout = commandline.DetermineCheckout(os.getcwd())
      self.options.chrome_src = checkout.chrome_src_dir
    else:
      checkout = commandline.DetermineCheckout(self.options.chrome_src)
      if not checkout.chrome_src_dir:
        cros_build_lib.Die('Chrome checkout not found at %s',
                           self.options.chrome_src)
      self.options.chrome_src = checkout.chrome_src_dir

    if self.options.clang and not self.options.chrome_src:
      cros_build_lib.Die('--clang requires --chrome-src to be set.')

    if self.options.version and self.options.sdk_path:
      cros_build_lib.Die('Cannot specify both --version and --sdk-path.')

    self.silent = bool(self.options.cmd)
    # Lazy initialize because SDKFetcher creates a GSContext() object in its
    # constructor, which may block on user input.
    self.sdk = SDKFetcher(self.options.cache_dir, self.options.board,
                          clear_cache=self.options.clear_sdk_cache,
                          chrome_src=self.options.chrome_src,
                          sdk_path=self.options.sdk_path,
                          silent=self.silent)

    prepare_version = self.options.version
    if not prepare_version and not self.options.sdk_path:
      prepare_version, _ = self.sdk.UpdateDefaultVersion()

    components = [self.sdk.TARGET_TOOLCHAIN_KEY, constants.CHROME_ENV_TAR]
    if not self.options.chroot:
      components.append(constants.CHROME_SYSROOT_TAR)

    goma_dir = None
    goma_port = None
    if self.options.goma:
      try:
        goma_dir, goma_port = self._FetchGoma()
      except GomaError as e:
        logging.error('Goma: %s.  Bypass by running with --nogoma.', e)

    if self.options.clang:
      self._SetupClang()

    with self.sdk.Prepare(components, version=prepare_version,
                          target_tc=self.options.target_tc,
                          toolchain_url=self.options.toolchain_url) as ctx:
      env = self._SetupEnvironment(self.options.board, ctx, self.options,
                                   goma_dir=goma_dir, goma_port=goma_port)
      with self._GetRCFile(env, self.options.bashrc) as rcfile:
        bash_cmd = ['/bin/bash']

        extra_env = None
        if not self.options.cmd:
          bash_cmd.extend(['--rcfile', rcfile, '-i'])
        else:
          # The '"$@"' expands out to the properly quoted positional args
          # coming after the '--'.
          bash_cmd.extend(['-c', '"$@"', '--'])
          bash_cmd.extend(self.options.cmd)
          # When run in noninteractive mode, bash sources the rc file set in
          # BASH_ENV, and ignores the --rcfile flag.
          extra_env = {'BASH_ENV': rcfile}

        # Bash behaves differently when it detects that it's being launched by
        # sshd - it ignores the BASH_ENV variable.  So prevent ssh-related
        # environment variables from being passed through.
        os.environ.pop('SSH_CLIENT', None)
        os.environ.pop('SSH_CONNECTION', None)
        os.environ.pop('SSH_TTY', None)

        cmd_result = cros_build_lib.RunCommand(
            bash_cmd, print_cmd=False, debug_level=logging.CRITICAL,
            error_code_ok=True, extra_env=extra_env, cwd=self.options.cwd)
        if self.options.cmd:
          return cmd_result.returncode
