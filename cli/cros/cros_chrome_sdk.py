# -*- coding: utf-8 -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The cros chrome-sdk command for the simple chrome workflow."""

from __future__ import print_function

import argparse
import collections
import contextlib
import datetime
import glob
import json
import os
import re
import sys

from chromite.cbuildbot import archive_lib
from chromite.cli import command
from chromite.lib import cache
from chromite.lib import config_lib
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import gs
from chromite.lib import osutils
from chromite.lib import path_util
from chromite.lib import portage_util
from chromite.lib import retry_util
from chromite.utils import memoize
from gn_helpers import gn_helpers


assert sys.version_info >= (3, 6), 'This module requires Python 3.6+'


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


class NoChromiumSrcDir(Exception):
  """Error thrown when no chromium src dir is found."""

  def __init__(self, path):
    Exception.__init__(self, 'No chromium src dir found in: %s' % (path))

class MissingLKGMFile(Exception):
  """Error thrown when we cannot get the version from CHROMEOS_LKGM."""

  def __init__(self, path):
    Exception.__init__(self, 'Cannot parse CHROMEOS_LKGM file: %s' % (path))

class MissingSDK(Exception):
  """Error thrown when we cannot find an SDK."""

  def _ConstructLegolandURL(self, config):
    """Returns a link to the given board's release builder."""
    return ('http://cros-goldeneye/chromeos/legoland/builderHistory?'
            'buildConfig=%s' % config)

  def __init__(self, config, version=None):
    msg = 'Cannot find SDK for %s' % config
    if version is not None:
      msg += ' with version %s' % version
    msg += ' from its builder: %s' % self._ConstructLegolandURL(config)
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
  SYMLINK_CACHE = 'symlinks'

  TARGET_TOOLCHAIN_KEY = 'target_toolchain'
  QEMU_BIN_PATH = 'app-emulation/qemu'
  SEABIOS_BIN_PATH = 'sys-firmware/seabios'
  TAST_CMD_PATH = 'chromeos-base/tast-cmd'
  TAST_REMOTE_TESTS_PATH = 'chromeos-base/tast-remote-tests-cros'

  CANARIES_PER_DAY = 3
  DAYS_TO_CONSIDER = 14
  VERSIONS_TO_CONSIDER = DAYS_TO_CONSIDER * CANARIES_PER_DAY

  def __init__(self, cache_dir, board, clear_cache=False, chrome_src=None,
               sdk_path=None, toolchain_path=None, silent=False,
               use_external_config=None,
               fallback_versions=VERSIONS_TO_CONSIDER):
    """Initialize the class.

    Args:
      cache_dir: The toplevel cache dir to use.
      board: The board to manage the SDK for.
      clear_cache: Clears the sdk cache during __init__.
      chrome_src: The location of the chrome checkout.  If unspecified, the
        cwd is presumed to be within a chrome checkout.
      sdk_path: The path (whether a local directory or a gs:// path) to fetch
        SDK components from.
      toolchain_path: The path (whether a local directory or a gs:// path) to
        fetch toolchain components from.
      silent: If set, the fetcher prints less output.
      use_external_config: When identifying the configuration for a board,
        force usage of the external configuration if both external and internal
        are available.
      fallback_versions: The number of versions to consider.
    """
    site_config = config_lib.GetConfig()

    self.cache_base = os.path.join(cache_dir, COMMAND_NAME)
    if clear_cache:
      logging.warning('Clearing the SDK cache.')
      osutils.RmDir(self.cache_base, ignore_missing=True)
    self.tarball_cache = cache.TarballCache(
        os.path.join(self.cache_base, self.TARBALL_CACHE))
    self.misc_cache = cache.DiskCache(
        os.path.join(self.cache_base, self.MISC_CACHE))
    self.symlink_cache = cache.DiskCache(
        os.path.join(self.cache_base, self.SYMLINK_CACHE))
    self.board = board
    self.config = site_config.FindCanonicalConfigForBoard(
        board, allow_internal=not use_external_config)
    self.gs_base = archive_lib.GetBaseUploadURI(self.config)
    self.clear_cache = clear_cache
    self.chrome_src = chrome_src
    self.sdk_path = sdk_path
    self.toolchain_path = toolchain_path
    self.fallback_versions = fallback_versions
    self.silent = silent

    # For external configs, there is no need to run 'gsutil config', because
    # the necessary files are all accessible to anonymous users.
    internal = self.config['internal']
    self.gs_ctx = gs.GSContext(cache_dir=cache_dir, init_boto=internal)

    if self.sdk_path is None:
      self.sdk_path = os.environ.get(self.SDK_PATH_ENV)

    if self.toolchain_path is None:
      self.toolchain_path = 'gs://%s' % constants.SDK_GS_BUCKET

  def _UpdateTarball(self, url, ref):
    """Worker function to fetch tarballs"""
    with osutils.TempDir(base_dir=self.tarball_cache.staging_dir) as tempdir:
      local_path = os.path.join(tempdir, os.path.basename(url))
      Log('SDK: Fetching %s', url, silent=self.silent)
      self.gs_ctx.Copy(url, tempdir, debug_level=logging.DEBUG)
      ref.SetDefault(local_path, lock=True)

  def _UpdateCacheSymlink(self, ref, source_path):
    """Adds a symlink to the cache pointing at the given source.

    Args:
      ref: cache.CacheReference of the link to be created.
      source_path: Absolute path that the symlink will point to.
    """
    with osutils.TempDir(base_dir=self.symlink_cache.staging_dir) as tempdir:
      # Make the symlink relative so the cache can be moved to different
      # locations/machines without breaking the link.
      rel_source_path = os.path.relpath(
          source_path, start=os.path.dirname(ref.path))
      link_name_path = os.path.join(tempdir, 'tmp-link')
      osutils.SafeSymlink(rel_source_path, link_name_path)
      ref.SetDefault(link_name_path, lock=True)

  def _GetMetadata(self, version):
    """Return metadata (in the form of a dict) for a given version."""
    raw_json = None
    version_base = self._GetVersionGSBase(version)
    metadata_path = os.path.join(version_base, constants.METADATA_JSON)
    partial_metadata_path = os.path.join(version_base,
                                         constants.PARTIAL_METADATA_JSON)
    with self.misc_cache.Lookup(
        self._GetTarballCacheKey(constants.PARTIAL_METADATA_JSON,
                                 partial_metadata_path)) as ref:
      if ref.Exists(lock=True):
        raw_json = osutils.ReadFile(ref.path)
      else:
        try:
          raw_json = self.gs_ctx.Cat(metadata_path,
                                     debug_level=logging.DEBUG,
                                     encoding='utf-8')
        except gs.GSNoSuchKey:
          logging.info('Could not read %s, falling back to %s',
                       metadata_path, partial_metadata_path)
          raw_json = self.gs_ctx.Cat(partial_metadata_path,
                                     debug_level=logging.DEBUG,
                                     encoding='utf-8')

        ref.AssignText(raw_json)

    return json.loads(raw_json)

  @staticmethod
  def GetChromeLKGM(chrome_src_dir=None):
    """Get ChromeOS LKGM checked into the Chrome tree.

    Args:
      chrome_src_dir: chrome source directory.

    Returns:
      Version number in format '10171.0.0'.
    """
    if not chrome_src_dir:
      chrome_src_dir = path_util.DetermineCheckout().chrome_src_dir
    if not chrome_src_dir:
      return None
    lkgm_file = os.path.join(chrome_src_dir, constants.PATH_TO_CHROME_LKGM)
    version = osutils.ReadFile(lkgm_file).rstrip()
    logging.debug('Read LKGM version from %s: %s', lkgm_file, version)
    return version

  @classmethod
  def _LookupMiscCache(cls, cache_dir, key):
    """Looks up an item in the misc cache.

    This should be used when inspecting an SDK that's already been initialized
    elsewhere.

    Args:
      cache_dir: The toplevel cache dir to search in.
      key: Key of item in the cache.

    Returns:
      Value of the item, or None if the item is missing.
    """
    misc_cache_path = os.path.join(cache_dir, COMMAND_NAME, cls.MISC_CACHE)
    misc_cache = cache.DiskCache(misc_cache_path)
    with misc_cache.Lookup(key) as ref:
      if ref.Exists(lock=True):
        return osutils.ReadFile(ref.path).strip()
    return None

  @classmethod
  def GetSDKVersion(cls, cache_dir, board):
    """Looks up the SDK version.

    Look at the environment variable, and then the misc cache.

    Args:
      cache_dir: The toplevel cache dir to search in.
      board: The board to search for.

    Returns:
      SDK version string, if found.
    """
    sdk_version = os.environ.get(cls.SDK_VERSION_ENV)
    if sdk_version:
      return sdk_version

    assert board
    return cls._LookupMiscCache(cache_dir, (board, 'latest'))

  @classmethod
  def GetCachedFullVersion(cls, cache_dir, board):
    """Get full version from the misc cache.

    Args:
      cache_dir: The toplevel cache dir to search in.
      board: The board to search for.

    Returns:
      Full version from the misc cache, if found.
    """
    assert board
    sdk_version = cls.GetSDKVersion(cache_dir, board)
    if not sdk_version:
      return None

    return cls._LookupMiscCache(cache_dir, ('full-version', board, sdk_version))

  @classmethod
  def GetCachePath(cls, key, cache_dir, board):
    """Gets the path to an item in the cache.

    This should be used when inspecting an SDK that's already been initialized
    elsewhere.

    Args:
      key: Key of item in the cache.
      cache_dir: The toplevel cache dir to search in.
      board: The board to search for.

    Returns:
      Path to the item, or None if the item is missing.
    """
    # The board is always known in the simple chrome SDK shell.
    if board is None:
      return None

    sdk_version = cls.GetSDKVersion(cache_dir, board)
    if not sdk_version:
      return None

    # Look up the cache entry in the symlink cache.
    symlink_cache_path = os.path.join(
        cache_dir, COMMAND_NAME, cls.SYMLINK_CACHE)
    symlink_cache = cache.DiskCache(symlink_cache_path)
    cache_key = (board, sdk_version, key)
    with symlink_cache.Lookup(cache_key) as ref:
      if ref.Exists():
        return ref.path
    return None

  @classmethod
  def ClearOldItems(cls, cache_dir, max_age_days=28):
    """Removes old items from the tarball cache older than max_age_days.

    Inspects the entire cache, not just a single board's items.

    Args:
      cache_dir: Location of the cache to be cleaned up.
      max_age_days: Any item in the cache not created/modified within this
        amount of time will be removed.
    """
    tarball_cache_path = os.path.join(
        cache_dir, COMMAND_NAME, cls.TARBALL_CACHE)
    tarball_cache = cache.TarballCache(tarball_cache_path)
    tarball_cache.DeleteStale(datetime.timedelta(days=max_age_days))

    # Now clean up any links in the symlink cache that are dangling due to the
    # removal of items above.
    symlink_cache_path = os.path.join(
        cache_dir, COMMAND_NAME, cls.SYMLINK_CACHE)
    symlink_cache = cache.DiskCache(symlink_cache_path)
    removed_keys = set()
    for key in symlink_cache.ListKeys():
      link_path = symlink_cache.GetKeyPath(key)
      if not os.path.exists(os.path.realpath(link_path)):
        symlink_cache.Lookup(key).Remove()
        removed_keys.add((key[0], key[1]))
    for board, version in removed_keys:
      logging.debug('Evicted SDK for %s-%s from the cache.', board, version)

  @memoize.Memoize
  def _GetSDKVersion(self, version):
    """Get SDK version from metadata.

    Args:
      version: LKGM version, e.g. 12345.0.0

    Returns:
      sdk_version, e.g. 2018.06.04.200410
    """
    return self._GetMetadata(version)['sdk-version']

  def _GetManifest(self, version):
    """Get the build manifest from the cache, downloading it if necessary.

    Args:
      version: LKGM version, e.g. 12345.0.0

    Returns:
      build manifest as a python dictionary. The build manifest contains build
      versions for packages built by the SDK builder.
    """
    with self.misc_cache.Lookup(('manifest', self.board, version)) as ref:
      if ref.Exists(lock=True):
        manifest = osutils.ReadFile(ref.path)
      else:
        manifest_path = gs.GetGsURL(
            bucket=constants.SDK_GS_BUCKET,
            suburl='cros-sdk-%s.tar.xz.Manifest' % self._GetSDKVersion(version),
            for_gsutil=True)
        manifest = self.gs_ctx.Cat(manifest_path, encoding='utf-8')
        ref.AssignText(manifest)
      return json.loads(manifest)

  def _GetBinPackageGSPath(self, version, key):
    """Get google storage path of prebuilt binary package.

    Args:
      version: LKGM version, e.g. 12345.0.0
      key: key in build manifest, for e.g. 'app-emulation/qemu'

    Returns:
      GS path, for e.g. gs://chromeos-prebuilt/board/amd64-host/
      chroot-2018.10.23.171742/packages/app-emulation/qemu-3.0.0.tbz2
    """
    if not version or not key:
      # A version and key is needed to locate the package in Google Storage.
      return None
    package_version = self._GetManifest(version)['packages'][key][0][0]
    return gs.GetGsURL(
        bucket='chromeos-prebuilt',
        suburl='board/amd64-host/chroot-%s/packages/%s-%s.tbz2' %
        (self._GetSDKVersion(version), key, package_version),
        for_gsutil=True)

  def _GetTarballCachePath(self, component, url):
    """Get a path in the tarball cache.

    Args:
      component: component name, for e.g. 'app-emulation/qemu'
      url: Google Storage url, e.g. 'gs://chromiumos-sdk/2019/some-tarball.tar'
    """
    cache_key = self._GetTarballCacheKey(component, url)
    with self.tarball_cache.Lookup(cache_key) as ref:
      if ref.Exists(lock=True):
        return ref.path
    return None

  def _FinalizePackages(self, version):
    """Finalize downloaded packages.

    Fix broken seabios symlinks in the qemu package.

    Args:
      version: LKGM version, e.g. 12345.0.0
    """
    self._CreateSeabiosFWSymlinks(version)

  def _CreateSeabiosFWSymlinks(self, version):
    """Create Seabios firmware symlinks.

    tarballs/<board>+<version>+app-emulation/qemu/usr/share/qemu/ has a number
    of broken symlinks, for example: bios.bin -> ../seabios/bios.bin
    bios.bin is in the seabios package at <cache>/seabios/usr/share/seabios/
    To resolve these symlinks, we create symlinks from
    <cache>+sys-firmware/seabios/usr/share/* to
    <cache>+app-emulation/qemu/usr/share/

    Args:
      version: LKGM version, e.g. 12345.0.0
    """
    qemu_bin_path = self._GetTarballCachePath(
        self.QEMU_BIN_PATH,
        self._GetBinPackageGSPath(version, self.QEMU_BIN_PATH))
    seabios_bin_path = self._GetTarballCachePath(
        self.SEABIOS_BIN_PATH,
        self._GetBinPackageGSPath(version, self.SEABIOS_BIN_PATH))
    if not qemu_bin_path or not seabios_bin_path:
      return

    # Symlink the directories in seabios/usr/share/* to qemu/usr/share/.
    share_dir = 'usr/share'
    seabios_share_dir = os.path.join(seabios_bin_path, share_dir)
    qemu_share_dir = os.path.join(qemu_bin_path, share_dir)
    for seabios_dir in os.listdir(seabios_share_dir):
      src_dir = os.path.relpath(
          os.path.join(seabios_share_dir, seabios_dir), qemu_share_dir)
      target_dir = os.path.join(qemu_share_dir, seabios_dir)
      if not os.path.exists(target_dir):
        os.symlink(src_dir, target_dir)

  def _GetFullVersionFromStorage(self, version_file):
    """Cat |version_file| in google storage.

    Args:
      version_file: google storage path of the version file.

    Returns:
      Version number in the format 'R30-3929.0.0' or None.
    """
    try:
      # If the version doesn't exist in google storage,
      # which isn't unlikely, don't waste time on retries.
      full_version = self.gs_ctx.Cat(version_file, retries=0, encoding='utf-8')
      assert full_version.startswith('R')
      return full_version
    except (gs.GSNoSuchKey, gs.GSCommandError):
      return None

  def _GetFullVersionFromRecentLatest(self, version):
    """Gets the full version number from a recent LATEST- file.

    If LATEST-{version} does not exist, we need to look for a recent
    LATEST- file to get a valid full version from.

    Args:
      version: The version number to look backwards from. If version is not a
      canary version (ending in .0.0), returns None.

    Returns:
      Version number in the format 'R30-3929.0.0' or None.
    """

    # If version does not end in .0.0 it is not a canary so fail.
    if not version.endswith('.0.0'):
      return None
    version_base = int(version.split('.')[0])
    version_base_min = max(version_base - self.fallback_versions, 0)

    for v in range(version_base - 1, version_base_min, -1):
      version_file = '%s/LATEST-%d.0.0' % (self.gs_base, v)
      logging.info('Trying: %s', version_file)
      full_version = self._GetFullVersionFromStorage(version_file)
      if full_version is not None:
        logging.warning(
            'Using cros version from most recent LATEST file: %s -> %s',
            version_file, full_version)
        return full_version
    logging.warning('No recent LATEST file found from %d.0.0 to %d.0.0: ',
                    version_base_min, version_base)
    return None

  def _GetFullVersionFromLatest(self, version):
    """Gets the full version number from the LATEST-{version} file.

    Args:
      version: The version number or branch to look at.

    Returns:
      Version number in the format 'R30-3929.0.0' or None.
    """
    version_file = '%s/LATEST-%s' % (self.gs_base, version)
    full_version = self._GetFullVersionFromStorage(version_file)
    if full_version is None:
      logging.warning('No LATEST file matching SDK version %s', version)
      return self._GetFullVersionFromRecentLatest(version)
    return full_version

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
    checkout = path_util.DetermineCheckout(checkout_dir)
    current = self.GetDefaultVersion() or '0'

    if not checkout.chrome_src_dir:
      raise NoChromiumSrcDir(checkout_dir)

    target = self.GetChromeLKGM(checkout.chrome_src_dir)
    if target is None:
      raise MissingLKGMFile(checkout.chrome_src_dir)

    self._SetDefaultVersion(target)
    return target, target != current

  def GetFullVersion(self, version):
    """Add the release branch and build number to a ChromeOS platform version.

    This will specify where you can get the latest build for the given version
    for the current board.

    Args:
      version: A ChromeOS platform number of the form XXXX.XX.XX, i.e.,
        3918.0.0. If a full version is provided, it will be returned unmodified.

    Returns:
      The version with release branch and build number added, as needed. E.g.
      R28-3918.0.0-b1234.
    """
    if version.startswith('R'):
      return version

    with self.misc_cache.Lookup(('full-version', self.board, version)) as ref:
      if ref.Exists(lock=True):
        return osutils.ReadFile(ref.path).strip()
      else:
        full_version = self._GetFullVersionFromLatest(version)

        if full_version is None:
          raise MissingSDK(self.config.name, version)

        ref.AssignText(full_version)
        return full_version

  def _GetVersionGSBase(self, version):
    """The base path of the SDK for a particular version."""
    if self.sdk_path is not None:
      return self.sdk_path

    full_version = self.GetFullVersion(version)
    return os.path.join(self.gs_base, full_version)

  def _GetTarballCacheKey(self, component, url):
    """Builds the cache key tuple for an SDK component.

    Returns a key based of the component name + the URL of its location in GS.
    """
    key = self.sdk_path if self.sdk_path else url.strip('gs://')
    key = key.replace('/', '-')
    return (os.path.join(component, key),)

  def _GetLinkNameForComponent(self, version, component):
    """Builds the human-readable symlink name for an SDK component."""
    version_section = version
    if self.sdk_path is not None:
      version_section = self.sdk_path.replace('/', '__').replace(':', '__')
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
          self.toolchain_path, toolchain_url % {'target': target_tc})
      components.remove(self.TARGET_TOOLCHAIN_KEY)

    # Fetch the Tast binary.
    tast_cmd_path = self._GetBinPackageGSPath(version, self.TAST_CMD_PATH)
    tast_remote_tests_path = self._GetBinPackageGSPath(
        version, self.TAST_REMOTE_TESTS_PATH)
    if tast_cmd_path and tast_remote_tests_path:
      fetch_urls[self.TAST_CMD_PATH] = tast_cmd_path
      fetch_urls[self.TAST_REMOTE_TESTS_PATH] = tast_remote_tests_path
    else:
      logging.warning('Failed to find Tast binaries to download.')

    # Also fetch QEMU binary if VM_IMAGE_TAR is specified.
    if constants.VM_IMAGE_TAR in components:
      qemu_bin_path = self._GetBinPackageGSPath(version, self.QEMU_BIN_PATH)
      seabios_bin_path = self._GetBinPackageGSPath(version,
                                                   self.SEABIOS_BIN_PATH)
      if qemu_bin_path and seabios_bin_path:
        fetch_urls[self.QEMU_BIN_PATH] = qemu_bin_path
        fetch_urls[self.SEABIOS_BIN_PATH] = seabios_bin_path
      else:
        logging.warning('Failed to find QEMU/Seabios binaries to download.')

    version_base = self._GetVersionGSBase(version)
    fetch_urls.update((t, os.path.join(version_base, t)) for t in components)
    try:
      for key, url in fetch_urls.items():
        tarball_cache_key = self._GetTarballCacheKey(key, url)
        tarball_ref = self.tarball_cache.Lookup(tarball_cache_key)
        key_map[tarball_cache_key] = tarball_ref
        tarball_ref.Acquire()
        if not tarball_ref.Exists(lock=True):
          # TODO(rcui): Parallelize this.  Requires acquiring locks *before*
          # generating worker processes; therefore the functionality needs to
          # be moved into the DiskCache class itself -
          # i.e.,DiskCache.ParallelSetDefault().
          try:
            self._UpdateTarball(url, tarball_ref)
          except gs.GSNoSuchKey:
            if key == constants.VM_IMAGE_TAR:
              logging.warning(
                  'No VM available for board %s. '
                  'Please try a different board, e.g. amd64-generic.',
                  self.board)
            else:
              raise

        # Create a symlink in a separate cache dir that points to the tarball
        # component. Since the tarball cache is keyed based off of GS URLs,
        # these symlinks can be used to identify tarball components without
        # knowing the GS URL.
        link_name = self._GetLinkNameForComponent(version, key)
        link_ref = self.symlink_cache.Lookup(link_name)
        key_map[key] = link_ref
        link_ref.Acquire()
        if not link_ref.Exists(lock=True):
          self._UpdateCacheSymlink(link_ref, tarball_ref.path)

      self._FinalizePackages(version)
      ctx_version = version
      if self.sdk_path is not None:
        ctx_version = CUSTOM_VERSION
      yield self.SDKContext(ctx_version, target_tc, key_map)
    finally:
      # TODO(rcui): Move to using cros_build_lib.ContextManagerStack()
      cros_build_lib.SafeRun(ref.Release for ref in key_map.values())


class GomaError(Exception):
  """Indicates error with setting up Goma."""


@command.CommandDecorator(COMMAND_NAME)
class ChromeSDKCommand(command.CliCommand):
  """Set up an environment for building Chrome on Chrome OS.

  Pulls down SDK components for building and testing Chrome for Chrome OS,
  sets up the environment for building Chrome, and runs a command in the
  environment, starting a bash session if no command is specified.

  The bash session environment is set up by a user-configurable rc file located
  at ~/.chromite/chrome_sdk.bashrc.
  """

  # Note, this URL is not accessible outside of corp.
  _GOMA_DOWNLOAD_URL = ('https://clients5.google.com/cxx-compiler-service/'
                        'download/downloadurl')
  _GOMA_TGZ = 'goma-goobuntu.tgz'

  _CHROME_CLANG_DIR = 'third_party/llvm-build/Release+Asserts/bin'
  _BUILD_ARGS_DIR = 'build/args/chromeos/'

  EBUILD_ENV_PATHS = (
      # Compiler tools.
      'CXX',
      'CC',
      'AR',
      'AS',
      'LD',
      'NM',
      'RANLIB',
      'READELF',
  )

  EBUILD_ENV = EBUILD_ENV_PATHS + (
      # Compiler flags.
      'CFLAGS',
      'CXXFLAGS',
      'CPPFLAGS',
      'LDFLAGS',

      # Misc settings.
      'GN_ARGS',
      'GOLD_SET',
      'USE',
  )

  SDK_GOMA_PORT_ENV = 'SDK_GOMA_PORT'
  SDK_GOMA_DIR_ENV = 'SDK_GOMA_DIR'

  GOMACC_PORT_CMD = ['./gomacc', 'port']

  # Override base class property to use cache related commandline options.
  use_caching_options = True

  @staticmethod
  def ValidateVersion(version):
    """Ensures that the version arg is potentially valid.

    See the argument description for supported version formats.
    """

    if (not re.match(r'^[0-9]+\.0\.0$', version) and
        not re.match(r'^R[0-9]+-[0-9]+\.[0-9]+\.[0-9]+', version)):
      raise argparse.ArgumentTypeError(
          '--version should be in the format 1234.0.0 or R56-1234.0.0')
    return version

  @classmethod
  def AddParser(cls, parser):
    super(ChromeSDKCommand, cls).AddParser(parser)
    parser.add_argument(
        '--board', required=False, help='The board SDK to use.')
    parser.add_argument(
        '--boards', required=False,
        help='Colon-separated list of boards to fetch SDKs for. Implies '
             '--no-shell since a shell is tied to a single board. Used to '
             'quickly setup cache and build dirs for multiple boards at once.')
    parser.add_argument(
        '--build-label', default='Release',
        help='The label for this build. Used as a subdirectory name under '
             'out_${BOARD}/')
    parser.add_argument(
        '--bashrc', type='path',
        default=constants.CHROME_SDK_BASHRC,
        help='A bashrc file used to set up the SDK shell environment. '
             'Defaults to %s.' % constants.CHROME_SDK_BASHRC)
    parser.add_argument(
        '--chroot', type='path',
        help='Path to a ChromeOS chroot to use. If set, '
             '<chroot>/build/<board> will be used as the sysroot that Chrome '
             'is built against. If chromeos-chrome was built, the build '
             'environment from the chroot will also be used. The version shown '
             'in the SDK shell prompt will have an asterisk prepended to it.')
    parser.add_argument(
        '--chrome-src', type='path',
        help='Specifies the location of a Chrome src/ directory.  Required if '
             'not running from a Chrome checkout.')
    parser.add_argument(
        '--cwd', type='path',
        help='Specifies a directory to switch to after setting up the SDK '
             'shell.  Defaults to the current directory.')
    parser.add_argument(
        '--internal', action='store_true', default=False,
        help='Sets up SDK for building official (internal) Chrome '
             'Chrome, rather than Chromium.')
    parser.add_argument(
        '--use-external-config', action='store_true', default=False,
        help='Use the external configuration for the specified board, even if '
             'an internal configuration is avalable.')
    parser.add_argument(
        '--sdk-path', type='local_or_gs_path',
        help='Provides a path, whether a local directory or a gs:// path, to '
             'pull SDK components from.')
    parser.add_argument(
        '--toolchain-path', type='local_or_gs_path',
        help='Provides a path, whether a local directory or a gs:// path, to '
             'pull toolchain components from.')
    parser.add_argument(
        '--no-shell', action='store_false', default=True, dest='use_shell',
        help='Skips the interactive shell. When this arg is passed, the needed '
             'toolchain will still be downloaded. However, no //out* dir will '
             'automatically be created. The args.gn file will instead be '
             'downloaded at a shareable location in //%s, and the SDK will '
             'simply exit after that.' % cls._BUILD_ARGS_DIR)
    parser.add_argument(
        '--gn-extra-args',
        help='Provides extra args to "gn gen". Uses the same format as '
             'gn gen, e.g. "foo = true bar = 1".')
    parser.add_argument(
        '--gn-gen', action='store_true', default=True, dest='gn_gen',
        help='Run "gn gen" if args.gn is stale.')
    parser.add_argument(
        '--nogn-gen', action='store_false', dest='gn_gen',
        help='Do not run "gn gen", warns if args.gn is stale.')
    parser.add_argument(
        '--nogoma', action='store_false', default=True, dest='goma',
        help='Disables Goma in the shell by removing it from the PATH and '
             'set use_goma=false to GN_ARGS.')
    parser.add_argument(
        '--nostart-goma', action='store_false', default=True, dest='start_goma',
        help='Skip starting goma and hope somebody else starts goma later.')
    parser.add_argument(
        '--gomadir', type='path',
        help='Use the goma installation at the specified PATH.')
    parser.add_argument(
        '--version', default=None, type=cls.ValidateVersion,
        help='Specify the SDK version to use. This can be a platform version '
             'ending in .0.0, e.g. 1234.0.0, in which case the full version '
             'will be extracted from the corresponding LATEST file for the '
             'specified board. If no LATEST file exists, an older version '
             'will be used if available. Alternatively, a full version may be '
             'specified, e.g. R56-1234.0.0, in which case that exact version '
             'will be used. Defaults to using the version specified in the '
             'CHROMEOS_LKGM file in the chromium checkout.')
    parser.add_argument(
        '--fallback-versions', type=int,
        default=SDKFetcher.VERSIONS_TO_CONSIDER,
        help='The number of recent LATEST files to consider in the case that '
             'the specified version is missing.')
    parser.add_argument(
        'cmd', nargs='*', default=None,
        help='The command to execute in the SDK environment.  Defaults to '
             'starting a bash shell.')
    parser.add_argument(
        '--download-vm', action='store_true', default=False,
        help='Additionally downloads a VM image from cloud storage.')
    parser.add_argument(
        '--thinlto', action='store_true', default=False,
        help='Enable ThinLTO in build.')
    parser.add_argument(
        '--cfi', action='store_true', default=False,
        help='Enable CFI in build.')

    parser.caching_group.add_argument(
        '--clear-sdk-cache', action='store_true',
        default=False,
        help='Removes everything in the SDK cache before starting.')

    group = parser.add_argument_group(
        'Metadata Overrides (Advanced)',
        description='Provide all of these overrides in order to remove '
                    'dependencies on metadata.json existence.')
    group.add_argument(
        '--target-tc', action='store', default=None,
        help='Override target toolchain name, e.g. x86_64-cros-linux-gnu')
    group.add_argument(
        '--toolchain-url', action='store', default=None,
        help='Override toolchain url format pattern, e.g. '
             '2014/04/%%(target)s-2014.04.23.220740.tar.xz')

  def __init__(self, options):
    super(ChromeSDKCommand, self).__init__(options)
    self.board = options.board
    # Lazy initialized.
    self.sdk = None
    # Initialized later based on options passed in.
    self.silent = True

  @staticmethod
  def _PS1Prefix(board, version, chroot=None):
    """Returns a string describing the sdk environment for use in PS1."""
    chroot_star = '*' if chroot else ''
    return '(sdk %s %s%s)' % (board, chroot_star, version)

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
    current_ps1 = cros_build_lib.run(
        ['bash', '-l', '-c', 'echo "$PS1"'], print_cmd=False, encoding='utf-8',
        capture_output=True).output.splitlines()
    if current_ps1:
      current_ps1 = current_ps1[-1]
    if not current_ps1:
      # Something went wrong, so use a fallback value.
      current_ps1 = r'\u@\h \w $ '
    ps1_prefix = ChromeSDKCommand._PS1Prefix(board, version, chroot)
    return '%s %s' % (ps1_prefix, current_ps1)

  def _UpdateGnArgsIfStale(self, out_dir, build_label, gn_args, board):
    """Runs 'gn gen' if gn args are stale or logs a warning."""
    if not self.options.use_shell:
      gn_args_file_path = os.path.join(
          self.options.chrome_src, self._BUILD_ARGS_DIR, board + '.gni')
      osutils.WriteFile(gn_args_file_path, gn_helpers.ToGNString(gn_args))
      return

    build_dir = os.path.join(out_dir, build_label)
    gn_args_file_path = os.path.join(
        self.options.chrome_src, build_dir, 'args.gn')

    if not self._StaleGnArgs(gn_args, gn_args_file_path):
      return

    if not self.options.gn_gen:
      logging.warning('To update gn args run:')
      logging.warning('gn gen %s --args="$GN_ARGS"', build_dir)
      return

    logging.warning('Running gn gen')
    cros_build_lib.run(
        ['gn', 'gen', build_dir,
         '--args=%s' % gn_helpers.ToGNString(gn_args)],
        print_cmd=logging.getLogger().isEnabledFor(logging.DEBUG),
        cwd=self.options.chrome_src)

  def _StaleGnArgs(self, new_gn_args, gn_args_file_path):
    """Returns True if args.gn needs to be updated."""
    if not os.path.exists(gn_args_file_path):
      logging.warning('No args.gn file: %s', gn_args_file_path)
      return True

    old_gn_args = gn_helpers.FromGNArgs(osutils.ReadFile(gn_args_file_path))
    if new_gn_args == old_gn_args:
      return False

    logging.warning('Stale args.gn file: %s', gn_args_file_path)
    self._LogArgsDiff(old_gn_args, new_gn_args)
    return True

  def _LogArgsDiff(self, cur_args, new_args):
    """Logs the differences between |cur_args| and |new_args|."""
    cur_keys = set(cur_args.keys())
    new_keys = set(new_args.keys())

    for k in new_keys - cur_keys:
      logging.info('MISSING ARG: %s = %s', k, new_args[k])

    for k in cur_keys - new_keys:
      logging.info('EXTRA ARG: %s = %s', k, cur_args[k])

    for k in new_keys & cur_keys:
      v_cur = cur_args[k]
      v_new = new_args[k]
      if v_cur != v_new:
        logging.info('MISMATCHED ARG: %s: %s != %s', k, v_cur, v_new)

  def _SetupTCEnvironment(self, options, env):
    """Sets up toolchain-related environment variables."""
    chrome_clang_path = os.path.join(options.chrome_src, self._CHROME_CLANG_DIR)

    # For host compiler, we use the compiler that comes with Chrome
    # instead of the target compiler.
    env['CC_host'] = os.path.join(chrome_clang_path, 'clang')
    env['CXX_host'] = os.path.join(chrome_clang_path, 'clang++')
    env['LD_host'] = env['CXX_host']

  def _AbsolutizeBinaryPath(self, binary, tc_path):
    """Modify toolchain path for goma build.

    This function absolutizes the path to the given toolchain binary, which
    will then be relativized in build/toolchain/cros/BUILD.gn. This ensures the
    paths are the same across different machines & checkouts, which improves
    cache hit rate in distributed build systems (i.e. goma).

    Args:
      binary: Name of toolchain binary.
      tc_path: Path to toolchain directory.

    Returns:
      Absolute path to the binary in the toolchain dir.
    """
    # If binary doesn't contain a '/', assume it's located in the toolchain dir.
    if os.path.basename(binary) == binary:
      return os.path.join(tc_path, 'bin', binary)
    return binary

  def _SetupEnvironment(self, board, sdk_ctx, options, goma_dir=None,
                        goma_port=None):
    """Sets environment variables to export to the SDK shell."""
    if options.chroot:
      sysroot = os.path.join(options.chroot, 'build', board)
      if not os.path.isdir(sysroot) and not options.cmd:
        logging.warning('Because --chroot is set, expected a sysroot to be at '
                        "%s, but couldn't find one.", sysroot)
    else:
      sysroot = sdk_ctx.key_map[constants.CHROME_SYSROOT_TAR].path

    environment = os.path.join(sdk_ctx.key_map[constants.CHROME_ENV_TAR].path,
                               'environment')
    if options.chroot:
      # Override with the environment from the chroot if available (i.e.
      # build_packages or emerge chromeos-chrome has been run for |board|).
      env_path = os.path.join(sysroot, portage_util.VDB_PATH, 'chromeos-base',
                              'chromeos-chrome-*')
      env_glob = glob.glob(env_path)
      if len(env_glob) != 1:
        logging.warning('Multiple Chrome versions in %s. This can be resolved'
                        ' by running "eclean-$BOARD -d packages". Using'
                        ' environment from: %s', env_path, environment)
      elif not os.path.isdir(env_glob[0]):
        logging.warning('Environment path not found: %s. Using enviroment from:'
                        ' %s.', env_path, environment)
      else:
        chroot_env_file = os.path.join(env_glob[0], 'environment.bz2')
        if os.path.isfile(chroot_env_file):
          # Log a warning here since this is new behavior that is not obvious.
          logging.notice('Environment fetched from: %s', chroot_env_file)
          # Uncompress enviornment.bz2 to pass to osutils.SourceEnvironment.
          chroot_cache = os.path.join(options.cache_dir, COMMAND_NAME, 'chroot')
          osutils.SafeMakedirs(chroot_cache)
          environment = os.path.join(chroot_cache, 'environment_%s' % board)
          cros_build_lib.UncompressFile(chroot_env_file, environment)

    env = osutils.SourceEnvironment(environment, self.EBUILD_ENV)
    gn_args = gn_helpers.FromGNArgs(env['GN_ARGS'])
    self._SetupTCEnvironment(options, env)

    # Add managed components to the PATH.
    env['PATH'] = '%s:%s' % (constants.CHROMITE_BIN_DIR, os.environ['PATH'])
    env['PATH'] = '%s:%s' % (os.path.dirname(self.sdk.gs_ctx.gsutil_bin),
                             env['PATH'])

    # Export internally referenced variables.
    os.environ[self.sdk.SDK_BOARD_ENV] = board
    if options.sdk_path:
      os.environ[self.sdk.SDK_PATH_ENV] = options.sdk_path
    os.environ[self.sdk.SDK_VERSION_ENV] = sdk_ctx.version

    # Add board and sdk version as gn args so that tests can bind them in
    # test wrappers generated at compile time.
    gn_args['cros_board'] = board
    gn_args['cros_sdk_version'] = sdk_ctx.version

    # Export the board/version info in a more accessible way, so developers can
    # reference them in their chrome_sdk.bashrc files, as well as within the
    # chrome-sdk shell.
    for var in [self.sdk.SDK_VERSION_ENV, self.sdk.SDK_BOARD_ENV]:
      env[var.lstrip('%')] = os.environ[var]

    # Export Goma information.
    if goma_dir:
      env[self.SDK_GOMA_DIR_ENV] = goma_dir
    if goma_port:
      env[self.SDK_GOMA_PORT_ENV] = goma_port

    # SYSROOT is necessary for Goma and the sysroot wrapper.
    env['SYSROOT'] = sysroot

    gn_args['target_sysroot'] = sysroot
    gn_args.pop('pkg_config', None)
    if options.internal:
      gn_args['is_chrome_branded'] = True
      gn_args['is_official_build'] = True
    else:
      gn_args.pop('is_chrome_branded', None)
      gn_args.pop('is_official_build', None)
      gn_args.pop('internal_gles2_conform_tests', None)

    target_tc_path = sdk_ctx.key_map[self.sdk.TARGET_TOOLCHAIN_KEY].path
    for env_path in self.EBUILD_ENV_PATHS:
      env[env_path] = self._AbsolutizeBinaryPath(env[env_path], target_tc_path)
    gn_args['cros_target_cc'] = env['CC']
    gn_args['cros_target_cxx'] = env['CXX']
    gn_args['cros_target_ld'] = env['LD']
    gn_args['cros_target_nm'] = env['NM']
    gn_args['cros_target_ar'] = env['AR']
    gn_args['cros_target_readelf'] = env['READELF']
    gn_args['cros_target_extra_cflags'] = env.get('CFLAGS', '')
    gn_args['cros_target_extra_cxxflags'] = env.get('CXXFLAGS', '')
    gn_args['cros_host_cc'] = env['CC_host']
    gn_args['cros_host_cxx'] = env['CXX_host']
    gn_args['cros_host_ld'] = env['LD_host']
    gn_args['cros_v8_snapshot_cc'] = env['CC_host']
    gn_args['cros_v8_snapshot_cxx'] = env['CXX_host']
    gn_args['cros_v8_snapshot_ld'] = env['LD_host']
    # Let Chromium's build files pick defaults for the following.
    gn_args.pop('cros_host_nm', None)
    gn_args.pop('cros_host_ar', None)
    gn_args.pop('cros_host_readelf', None)
    gn_args.pop('cros_v8_snapshot_nm', None)
    gn_args.pop('cros_v8_snapshot_ar', None)
    gn_args.pop('cros_v8_snapshot_readelf', None)
    # No need to adjust CFLAGS and CXXFLAGS for GN since the only
    # adjustment made in _SetupTCEnvironment is for split debug which
    # is done with 'use_debug_fission'.

    # Enable goma if requested.
    if not options.goma:
      # If --nogoma option is explicitly set, disable goma, even if it is
      # used in the original GN_ARGS.
      gn_args['use_goma'] = False
      gn_args.pop('goma_dir', None)
    elif goma_dir:
      gn_args['use_goma'] = True
      gn_args['goma_dir'] = goma_dir

    gn_args.pop('internal_khronos_glcts_tests', None)  # crbug.com/588080

    # Disable ThinLTO and CFI for simplechrome. Tryjob machines do not have
    # enough file descriptors to use. crbug.com/789607
    if not options.thinlto:
      gn_args['use_thin_lto'] = False
    if not options.cfi:
      gn_args['is_cfi'] = False
    # When using Goma and ThinLTO, distribute ThinLTO code generation on Goma.
    gn_args['use_goma_thin_lto'] = (
        gn_args.get('use_goma', False) and gn_args.get('use_thin_lto', False))
    # We need to remove the flag -Wl,-plugin-opt,-import-instr-limit=$num
    # from cros_target_extra_ldflags if options.thinlto is not set.
    # The format of ld flags is something like
    # '-Wl,-O1 -Wl,-O2 -Wl,--as-needed -stdlib=libc++'
    if not options.thinlto:
      extra_ldflags = gn_args.get('cros_target_extra_ldflags', '')

      ld_flags_list = extra_ldflags.split()
      ld_flags_list = (
          [f for f in ld_flags_list
           if not f.startswith('-Wl,-plugin-opt,-import-instr-limit')])
      if extra_ldflags:
        gn_args['cros_target_extra_ldflags'] = ' '.join(ld_flags_list)

    # We removed blink symbols on release builds on arm, see
    # https://crbug.com/792999. However, we want to keep the symbols
    # for simplechrome builds.
    gn_args['blink_symbol_level'] = -1

    # Remove symbol_level specified in the ebuild to use the default.
    # Currently that is 1 when is_debug=false, instead of 2 specified by the
    # ebuild. This results in faster builds in Simple Chrome.
    if 'symbol_level' in gn_args:
      symbol_level = gn_args.pop('symbol_level')
      logging.info('Removing symbol_level = %d from gn args, use '
                   '--gn-extra-args to specify a non default value.',
                   symbol_level)

    if options.gn_extra_args:
      gn_args.update(gn_helpers.FromGNArgs(options.gn_extra_args))

    gn_args_env = gn_helpers.ToGNString(gn_args)
    env['GN_ARGS'] = gn_args_env

    # PS1 sets the command line prompt and xterm window caption.
    full_version = sdk_ctx.version
    if full_version != CUSTOM_VERSION:
      full_version = self.sdk.GetFullVersion(sdk_ctx.version)
    env['PS1'] = self._CreatePS1(self.board, full_version,
                                 chroot=options.chroot)

    # Set the useful part of PS1 for users with a custom PROMPT_COMMAND.
    env['CROS_PS1_PREFIX'] = self._PS1Prefix(self.board, full_version,
                                             chroot=options.chroot)

    out_dir = 'out_%s' % self.board
    env['builddir_name'] = out_dir

    # This is used by landmines.py to prevent collisions when building both
    # chromeos and android from shared source.
    # For context, see crbug.com/407417
    env['CHROMIUM_OUT_DIR'] = os.path.join(options.chrome_src, out_dir)

    self._UpdateGnArgsIfStale(
        out_dir, options.build_label, gn_args, env['SDK_BOARD'])

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

      for key, value in env.items():
        contents.append("export %s='%s'\n" % (key, value))
      contents.append('. "%s"\n' % user_rc)

      rc_file = os.path.join(tempdir, 'rcfile')
      osutils.WriteFile(rc_file, contents)
      yield rc_file

  def _GomaPort(self, goma_dir):
    """Returns current active Goma port."""
    port = cros_build_lib.run(
        self.GOMACC_PORT_CMD, cwd=goma_dir, debug_level=logging.DEBUG,
        check=False, encoding='utf-8', capture_output=True).output.strip()
    return port

  def _FetchGoma(self):
    """Fetch, install, and start Goma, using cached version if it exists.

    Returns:
      A tuple (dir, port) containing the path to the cached goma/ dir and the
      Goma port.
    """
    common_path = os.path.join(self.options.cache_dir, constants.COMMON_CACHE)
    common_cache = cache.DiskCache(common_path)

    goma_dir = self.options.gomadir
    if not goma_dir:
      goma_dir_cmd = ['goma_ctl', 'goma_dir']
      goma_dir = cros_build_lib.run(
          goma_dir_cmd, check=False, capture_output=True,
          encoding='utf-8').stdout.strip()

    # TODO(crbug.com/1072400): Remove use of Goma in legacy location.
    if not goma_dir or not os.path.exists(os.path.join(goma_dir, 'gomacc')):
      logging.warning('Falling back to legacy Goma location')
      ref = common_cache.Lookup(('goma', '2'))
      if not ref.Exists():
        Log('Installing Goma.', silent=self.silent)
        with osutils.TempDir() as tempdir:
          goma_dir = os.path.join(tempdir, 'goma')
          osutils.SafeMakedirs(goma_dir)
          with osutils.ChdirContext(tempdir):
            try:
              result = retry_util.RunCurl(['--fail', self._GOMA_DOWNLOAD_URL],
                                          stdout=True, encoding='utf-8')
              if result.returncode:
                raise GomaError('Failed to fetch Goma Download URL')
              download_url = result.output.strip()
              result = retry_util.RunCurl(
                  ['--fail', '%s/%s' % (download_url, self._GOMA_TGZ),
                   '--output', self._GOMA_TGZ])
              if result.returncode:
                raise GomaError('Failed to fetch Goma')
            except retry_util.DownloadError:
              raise GomaError('Failed to fetch Goma')
            result = cros_build_lib.dbg_run(
                ['tar', 'xf', self._GOMA_TGZ,
                 '--strip=1', '-C', goma_dir])
            if result.returncode:
              raise GomaError('Failed to extract Goma')
            # TODO(crbug.com/1007384): Stop forcing Python 2.
            result = cros_build_lib.dbg_run(
                ['python2', os.path.join(goma_dir, 'goma_ctl.py'), 'update'],
                extra_env={'PLATFORM': 'goobuntu'})
            if result.returncode:
              raise GomaError('Failed to install Goma')
          ref.SetDefault(goma_dir)
      goma_dir = ref.path

    port = None
    if self.options.start_goma:
      Log('Starting Goma.', silent=self.silent)
      # TODO(crbug.com/1007384): Stop forcing Python 2.
      cros_build_lib.dbg_run(
          ['python2', os.path.join(goma_dir, 'goma_ctl.py'), 'ensure_start'])
      port = self._GomaPort(goma_dir)
      Log('Goma is started on port %s', port, silent=self.silent)
      if not port:
        raise GomaError('No Goma port detected')

    return goma_dir, port

  def Run(self):
    """Perform the command."""
    if bool(self.options.board) == bool(self.options.boards):
      cros_build_lib.Die('Must specify either one of --board or --boards.')

    if self.options.boards and self.options.use_shell:
      cros_build_lib.Die(
          'Must specify --no-shell when preparing multiple boards.')

    if os.environ.get(SDKFetcher.SDK_VERSION_ENV) is not None:
      cros_build_lib.Die('Already in an SDK shell.')

    src_path = self.options.chrome_src or os.getcwd()
    checkout = path_util.DetermineCheckout(src_path)
    if not checkout.chrome_src_dir:
      cros_build_lib.Die('Chrome checkout not found at %s', src_path)
    self.options.chrome_src = checkout.chrome_src_dir

    if self.options.version and self.options.sdk_path:
      cros_build_lib.Die('Cannot specify both --version and --sdk-path.')

    if self.options.cfi and not self.options.thinlto:
      cros_build_lib.Die('CFI requires ThinLTO.')

    # Remove old SDKs from the cache to avoid wasting disk space.
    SDKFetcher.ClearOldItems(self.options.cache_dir)

    if self.options.board:
      return self._RunOnceForBoard(self.options.board)
    else:
      self.options.boards = self.options.boards.split(':')
      for board in self.options.boards:
        self._RunOnceForBoard(board)

  def _RunOnceForBoard(self, board):
    """Internal implementation of Run() above for a single board."""
    self.silent = bool(self.options.cmd)
    # Lazy initialize because SDKFetcher creates a GSContext() object in its
    # constructor, which may block on user input.
    self.sdk = SDKFetcher(
        self.options.cache_dir, board,
        clear_cache=self.options.clear_sdk_cache,
        chrome_src=self.options.chrome_src,
        sdk_path=self.options.sdk_path,
        toolchain_path=self.options.toolchain_path,
        silent=self.silent,
        use_external_config=self.options.use_external_config,
        fallback_versions=self.options.fallback_versions
    )

    prepare_version = self.options.version
    if not prepare_version and not self.options.sdk_path:
      prepare_version, _ = self.sdk.UpdateDefaultVersion()

    components = [self.sdk.TARGET_TOOLCHAIN_KEY, constants.CHROME_ENV_TAR]
    if not self.options.chroot:
      components.append(constants.CHROME_SYSROOT_TAR)
    if self.options.download_vm:
      components.append(constants.VM_IMAGE_TAR)

    goma_dir = None
    goma_port = None
    if self.options.goma:
      try:
        goma_dir, goma_port = self._FetchGoma()
      except GomaError as e:
        logging.error('Goma: %s.  Bypass by running with --nogoma.', e)

    with self.sdk.Prepare(components, version=prepare_version,
                          target_tc=self.options.target_tc,
                          toolchain_url=self.options.toolchain_url) as ctx:
      env = self._SetupEnvironment(board, ctx, self.options,
                                   goma_dir=goma_dir, goma_port=goma_port)
      if not self.options.use_shell:
        return 0
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

        cmd_result = cros_build_lib.run(
            bash_cmd, print_cmd=False, debug_level=logging.CRITICAL,
            check=False, extra_env=extra_env, cwd=self.options.cwd)
        if self.options.cmd:
          return cmd_result.returncode
