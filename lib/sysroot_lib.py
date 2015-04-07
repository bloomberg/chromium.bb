# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utilities to create sysroots."""

from __future__ import print_function

import multiprocessing
import os

from chromite.cbuildbot import binhost
from chromite.cbuildbot import constants
from chromite.lib import cros_build_lib
from chromite.lib import osutils
from chromite.lib import portage_util
from chromite.lib import toolchain


_PORTAGE_WRAPPER_TEMPLATE = """#!/bin/sh

# If we try to use sudo when the sandbox is active, we get ugly warnings that
# just confuse developers.  Disable the sandbox in this case by rexecing.
if [ "${{SANDBOX_ON}}" = "1" ]; then
  SANDBOX_ON=0 exec "$0" "$@"
else
  unset LD_PRELOAD
fi

export CHROMEOS_ROOT="{source_root}"
export CHOST="{chost}"
export PORTAGE_CONFIGROOT="{sysroot}"
export SYSROOT="{sysroot}"
if [ -z "$PORTAGE_USERNAME" ]; then
  export PORTAGE_USERNAME=$(basename "${{HOME}}")
fi
export ROOT="{sysroot}"
exec sudo -E {command} "$@"
"""

_BOARD_WRAPPER_TEMPLATE = """#!/bin/sh
exec {command} --board="{board}" "$@"
"""

_PKGCONFIG_WRAPPER_TEMPLATE = """#!/bin/bash

PKG_CONFIG_LIBDIR=$(printf '%s:' "{sysroot}"/usr/*/pkgconfig)
export PKG_CONFIG_LIBDIR

export PKG_CONFIG_SYSROOT_DIR="{sysroot}"

exec pkg-config "$@"
"""

_wrapper_dir = '/usr/local/bin'

_CONFIGURATION_PATH = 'etc/make.conf.board_setup'

_CHROME_BINHOST_SUFFIX = '-LATEST_RELEASE_CHROME_BINHOST.conf'

_INTERNAL_BINHOST_DIR = os.path.join(
    constants.SOURCE_ROOT, 'src/private-overlays/chromeos-partner-overlay/'
    'chromeos/binhost/target')
_EXTERNAL_BINHOST_DIR = os.path.join(
    constants.SOURCE_ROOT, constants.CHROMIUMOS_OVERLAY_DIR,
    'chromeos/binhost/target')

_CHROMEOS_INTERNAL_BOTO_PATH = os.path.join(
    constants.SOURCE_ROOT, 'src', 'private-overlays', 'chromeos-overlay',
    'googlestorage_account.boto')

_ARCH_MAPPING = {
    'amd64': 'amd64-generic',
    'x86': 'x86-generic',
    'arm': 'arm-generic',
    'mips': 'mipsel-o32-generic',
}


def _CreateWrapper(wrapper_path, template, **kwargs):
  """Creates a wrapper from a given template.

  Args:
    wrapper_path: path to the wrapper.
    template: wrapper template.
    kwargs: fields to be set in the template.
  """
  osutils.WriteFile(wrapper_path, template.format(**kwargs), makedirs=True)
  os.chown(wrapper_path, 0, 0)
  os.chmod(wrapper_path, 0o755)


def _NotEmpty(filepath):
  """Returns True if |filepath| is not empty.

  Args:
    filepath: path to a file.
  """
  return os.path.exists(filepath) and osutils.ReadFile(filepath).strip()


def _DictToKeyValue(dictionary):
  """Formats dictionary in to a key=value string.

  Args:
    dictionary: a python dictionary.
  """
  output = []
  for key in sorted(dictionary.keys()):
    output.append('%s="%s"' % (key, dictionary[key]))

  return '\n'.join(output)


class Sysroot(object):
  """Class that encapsulate the interaction with sysroots."""

  def __init__(self, path):
    self.path = path
    self._config_file = os.path.join(path, _CONFIGURATION_PATH)

  def GetStandardField(self, field):
    """Returns the value of a standard field.

    Args:
      field: Field from the standard configuration file to get.
    """
    return osutils.SourceEnvironment(self._config_file,
                                     [field], multiline=True).get(field)

  def _WrapperPath(self, command, friendly_name=None):
    """Returns the path to the wrapper for |command|.

    Args:
      command: command to wrap.
      friendly_name: suffix to add to the command name. If None, the wrapper
        will be created in the sysroot.
    """
    if friendly_name:
      return os.path.join(_wrapper_dir, '%s-%s' % (command, friendly_name))
    return os.path.join(self.path, 'build', 'bin', command)

  def CreateAllWrappers(self, friendly_name=None):
    """Creates all the wrappers.

    Creates all portage tools wrappers, plus wrappers for gdb, cros_workon and
    pkg-config.

    Args:
      friendly_name: if not None, create friendly wrappers with |friendly_name|
        added to the command.
    """
    chost = self.GetStandardField('CHOST')
    for cmd in ('ebuild', 'eclean', 'emaint', 'equery', 'portageq', 'qcheck',
                'qdepends', 'qfile', 'qlist', 'qmerge', 'qsize'):
      args = {'sysroot': self.path, 'chost': chost, 'command': cmd,
              'source_root': constants.SOURCE_ROOT}
      if friendly_name:
        _CreateWrapper(self._WrapperPath(cmd, friendly_name),
                       _PORTAGE_WRAPPER_TEMPLATE, **args)
      _CreateWrapper(self._WrapperPath(cmd),
                     _PORTAGE_WRAPPER_TEMPLATE, **args)

    if friendly_name:
      _CreateWrapper(self._WrapperPath('emerge', friendly_name),
                     _PORTAGE_WRAPPER_TEMPLATE, sysroot=self.path, chost=chost,
                     command='emerge --root-deps',
                     source_root=constants.SOURCE_ROOT)

      _CreateWrapper(self._WrapperPath('cros_workon', friendly_name),
                     _BOARD_WRAPPER_TEMPLATE, board=friendly_name,
                     command='cros_workon')
      _CreateWrapper(self._WrapperPath('gdb', friendly_name),
                     _BOARD_WRAPPER_TEMPLATE, board=friendly_name,
                     command='cros_gdb')
      _CreateWrapper(self._WrapperPath('pkg-config', friendly_name),
                     _PKGCONFIG_WRAPPER_TEMPLATE, sysroot=self.path)

    _CreateWrapper(self._WrapperPath('pkg-config'),
                   _PKGCONFIG_WRAPPER_TEMPLATE, sysroot=self.path)
    _CreateWrapper(self._WrapperPath('emerge'), _PORTAGE_WRAPPER_TEMPLATE,
                   sysroot=self.path, chost=chost, command='emerge --root-deps',
                   source_root=constants.SOURCE_ROOT)

    # Create a link to the debug symbols in the chroot so that gdb can detect
    # them.
    debug_symlink = os.path.join('/usr/lib/debug', self.path.lstrip('/'))
    sysroot_debug = os.path.join(self.path, 'usr/lib/debug')
    osutils.SafeMakedirs(os.path.dirname(debug_symlink))
    osutils.SafeMakedirs(sysroot_debug)

    # If the symlink already exists, recreate it to be sure.
    if os.path.islink(debug_symlink):
      os.unlink(debug_symlink)

    os.symlink(sysroot_debug, debug_symlink)

  def GenerateBoardConfig(self, board):
    """Generates the configuration for a given board.

    Args:
      board: board name to use to generate the configuration.
    """
    config = {}
    toolchains = toolchain.GetToolchainsForBoard(board)
    config['CHOST'] = toolchain.FilterToolchains(toolchains, 'default',
                                                 True).keys()[0]
    config['ARCH'] = toolchain.GetArchForTarget(config['CHOST'])

    # Compute the overlay list.
    overlay_list = portage_util.FindOverlays(constants.BOTH_OVERLAYS, board)
    prefix = os.path.join(constants.SOURCE_ROOT, 'src', 'third_party')
    config['BOARD_OVERLAY'] = '\n'.join([o for o in overlay_list
                                         if not o.startswith(prefix)])

    config['MAKEOPTS'] = '-j%s' % str(multiprocessing.cpu_count())
    config['BOARD_USE'] = board
    config['ROOT'] = self.path + '/'
    config['PKG_CONFIG'] = self._WrapperPath('pkg-config')

    header = "# Created by cros_sysroot_utils from --board=%s." % board
    return '\n'.join((header, _DictToKeyValue(config)))

  def GenerateBrickConfig(self, brick, bsp=None):
    """Generates the configuration for a given brick.

    Args:
      brick: brick to generate the configuration for.
      bsp: BSP to use.
    """
    config = {}
    brick_list = brick.BrickStack()
    if bsp:
      brick_list.extend(bsp.BrickStack())

    # TODO(bsimonnet): get toolchain from config.json.
    base_brick = bsp or brick
    toolchains = toolchain.GetToolchainsForBrick(base_brick.brick_locator)
    config['CHOST'] = toolchain.FilterToolchains(toolchains, 'default',
                                                 True).keys()[0]
    config['ARCH'] = toolchain.GetArchForTarget(config['CHOST'])

    config['BOARD_OVERLAY'] = '\n'.join([b.OverlayDir() for b in brick_list])
    config['MAKEOPTS'] = '-j%s' % str(multiprocessing.cpu_count())
    config['ROOT'] = self.path + '/'
    config['PKG_CONFIG'] = self._WrapperPath('pkg-config')

    header = ("# Created by cros_sysroot_utils from --brick=%s."
              % brick.brick_locator)
    return '\n'.join((header, _DictToKeyValue(config)))

  def WriteConfig(self, config):
    """Writes the configuration.

    Args:
      config: configuration to use.
    """
    path = os.path.join(self.path, _CONFIGURATION_PATH)
    osutils.WriteFile(path, config, makedirs=True)

  def GenerateMakeConf(self, accepted_licenses=None):
    """Generates the board specific make.conf.

    Args:
      accepted_licenses: Licenses accepted by portage as a string.
    """
    config = ["""# AUTO-GENERATED FILE. DO NOT EDIT.

  # Source make.conf from each overlay."""]

    overlay_list = self.GetStandardField('BOARD_OVERLAY')
    boto_config = ''
    for overlay in overlay_list.splitlines():
      make_conf = os.path.join(overlay, 'make.conf')
      boto_file = os.path.join(overlay, 'googlestorage_account.boto')
      if os.path.isfile(make_conf):
        config.append('source %s' % make_conf)

      if os.path.isfile(boto_file):
        boto_config = boto_file

    # If there is a boto file in the chromeos internal overlay, use it as it
    # will have access to the most stuff.
    if os.path.isfile(_CHROMEOS_INTERNAL_BOTO_PATH):
      boto_config = _CHROMEOS_INTERNAL_BOTO_PATH

    gs_fetch_binpkg = os.path.join(constants.SOURCE_ROOT, 'chromite', 'bin',
                                   'gs_fetch_binpkg')
    gsutil_cmd = '%s \\"${URI}\\" \\"${DISTDIR}/${FILE}\\"' % gs_fetch_binpkg
    config.append('BOTO_CONFIG="%s"' % boto_config)
    config.append('FETCHCOMMAND_GS="bash -c \'BOTO_CONFIG=%s %s\'"'
                  % (boto_config, gsutil_cmd))
    config.append('RESUMECOMMAND_GS="$FETCHCOMMAND_GS"')

    if accepted_licenses:
      config.append('ACCEPT_LICENSE="%s"' % accepted_licenses)

    return '\n'.join(config)

  def GenerateBinhostConf(self, chrome_only=False, local_only=False):
    """Returns the binhost configuration.

    Args:
      chrome_only: If True, generate only the binhost for chrome.
      local_only: If True, use binary packages from local boards only.
    """
    board = self.GetStandardField('BOARD_USE')
    if local_only:
      if not board:
        return ''
      # TODO(bsimonnet): Refactor cros_generate_local_binhosts into a function
      # here and remove the following call.
      local_binhosts = cros_build_lib.RunCommand(
          [os.path.join(constants.CHROMITE_BIN_DIR,
                        'cros_generate_local_binhosts'), '--board=%s' % board],
          print_cmd=False, capture_output=True).output
      return '\n'.join([local_binhosts,
                        'PORTAGE_BINHOST="$LOCAL_BINHOST"'])

    config = []
    chrome_binhost = board and self._ChromeBinhost(board)
    preflight_binhost, preflight_binhost_internal = self._PreflightBinhosts(
        board)

    if chrome_only:
      if chrome_binhost:
        return '\n'.join(['source %s' % chrome_binhost,
                          'PORTAGE_BINHOST="$LATEST_RELEASE_CHROME_BINHOST"'])
      else:
        return ''

    config.append("""
# FULL_BINHOST is populated by the full builders. It is listed first because it
# is the lowest priority binhost. It is better to download packages from the
# preflight binhost because they are fresher packages.
PORTAGE_BINHOST="$FULL_BINHOST"
""")

    if preflight_binhost:
      config.append("""
# PREFLIGHT_BINHOST is populated by the preflight builders. If the same
# package is provided by both the preflight and full binhosts, the package is
# downloaded from the preflight binhost.
source %s
PORTAGE_BINHOST="$PORTAGE_BINHOST $PREFLIGHT_BINHOST"
""" % preflight_binhost)

    if preflight_binhost_internal:
      config.append("""
# The internal PREFLIGHT_BINHOST is populated by the internal preflight
# builders. It takes priority over the public preflight binhost.
source %s
PORTAGE_BINHOST="$PORTAGE_BINHOST $PREFLIGHT_BINHOST"
""" % preflight_binhost_internal)

    if chrome_binhost:
      config.append("""
# LATEST_RELEASE_CHROME_BINHOST provides prebuilts for chromeos-chrome only.
source %s
PORTAGE_BINHOST="$PORTAGE_BINHOST $LATEST_RELEASE_CHROME_BINHOST"
""" % chrome_binhost)

    return '\n'.join(config)

  def _ChromeBinhost(self, board):
    """Gets the latest chrome binhost for |board|.

    Args:
      board: The board to use.
    """
    extra_useflags = os.environ.get('USE', '').split()
    compat_id = binhost.CalculateCompatId(board, extra_useflags)
    internal_config = binhost.PrebuiltMapping.GetFilename(
        constants.SOURCE_ROOT, 'chrome')
    external_config = binhost.PrebuiltMapping.GetFilename(
        constants.SOURCE_ROOT, 'chromium', internal=False)
    binhost_dirs = (_INTERNAL_BINHOST_DIR, _EXTERNAL_BINHOST_DIR)

    if os.path.exists(internal_config):
      pfq_configs = binhost.PrebuiltMapping.Load(internal_config)
    elif os.path.exists(external_config):
      pfq_configs = binhost.PrebuiltMapping.Load(external_config)
    else:
      return None

    for key in pfq_configs.GetPrebuilts(compat_id):
      for binhost_dir in binhost_dirs:
        binhost_file = os.path.join(binhost_dir,
                                    key.board + _CHROME_BINHOST_SUFFIX)
        # Make sure the binhost file is not empty. We sometimes empty the file
        # to force clients to use another binhost.
        if _NotEmpty(binhost_file):
          return binhost_file

    return None

  def _PreflightBinhosts(self, board=None):
    """Returns the preflight binhost to use.

    Args:
      board: Board name.
    """
    prefixes = []
    arch = self.GetStandardField('ARCH')
    if arch in _ARCH_MAPPING:
      prefixes.append(_ARCH_MAPPING[arch])

    if board:
      prefixes = [board, board.split('_')[0]] + prefixes

    filenames = ['%s-PREFLIGHT_BINHOST.conf' % p for p in prefixes]

    external = internal = None
    for filename in filenames:
      # The binhost file must exist and not be empty, both for internal and
      # external binhosts.
      # When a builder is deleted and no longer publishes prebuilts, we need
      # developers to pick up the next set of prebuilts. Clearing the binhost
      # files triggers this.
      candidate = os.path.join(_INTERNAL_BINHOST_DIR, filename)
      if not internal and _NotEmpty(candidate):
        internal = candidate

      candidate = os.path.join(_EXTERNAL_BINHOST_DIR, filename)
      if not external and _NotEmpty(candidate):
        external = candidate

    return external, internal
