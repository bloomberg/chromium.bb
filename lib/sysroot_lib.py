# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utilities to create sysroots."""

from __future__ import print_function

import multiprocessing
import os

from chromite.cbuildbot import constants
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


def CreateWrapper(command_name, template, **kwargs):
  """Creates a wrapper from a given template.

  Args:
    command_name: name of the wrapper.
    template: wrapper template.
    kwargs: fields to be set in the template.
  """
  wrapper_path = os.path.join(_wrapper_dir, command_name)

  osutils.WriteFile(wrapper_path, template.format(**kwargs), makedirs=True)
  os.chown(wrapper_path, 0, 0)
  os.chmod(wrapper_path, 0o755)


def GetConfigFilePath(sysroot):
  """Returns the path to the configuration file for |sysroot|.

  sysroot: Path to the sysroot.
  """
  return os.path.join(sysroot, _CONFIGURATION_PATH)


def GetStandardField(sysroot, field):
  """Returns the value of a standard field.

  Args:
    sysroot: Path to the sysroot.
    field: Field from the standard configuration file to get.
  """
  return osutils.SourceEnvironment(GetConfigFilePath(sysroot),
                                   [field], multiline=True).get(field)


def CreateAllWrappers(sysroot, chost=None, friendly_name=None):
  """Creates all the wrappers for a given sysroot.

  Creates all portage tools wrappers, plus wrappers for gdb, cros_workon and
  pkg-config.

  Args:
    sysroot: path to the sysroot.
    chost: toolchain to use. Defaults to the sysroot's CHOST.
    friendly_name: suffix to add to the commands. Defaults to the last component
      of |sysroot|.
  """
  if chost is None:
    chost = GetStandardField(sysroot, 'CHOST')

  board = friendly_name or os.path.basename(sysroot)
  for cmd in ('ebuild', 'eclean', 'emaint', 'equery', 'portageq', 'qcheck',
              'qdepends', 'qfile', 'qlist', 'qmerge', 'qsize'):
    CreateWrapper('%s-%s' % (cmd, board), _PORTAGE_WRAPPER_TEMPLATE,
                  sysroot=sysroot, chost=chost, command=cmd,
                  source_root=constants.SOURCE_ROOT)

  CreateWrapper('emerge-%s' % board, _PORTAGE_WRAPPER_TEMPLATE,
                sysroot=sysroot, chost=chost,
                command='emerge --root-deps',
                source_root=constants.SOURCE_ROOT)

  CreateWrapper('cros_workon-%s' % board, _BOARD_WRAPPER_TEMPLATE,
                board=board, command='cros_workon')
  CreateWrapper('cros_gdb-%s' % board, _BOARD_WRAPPER_TEMPLATE,
                board=board, command='gdb')

  CreateWrapper('pkg-config-%s' % board, _PKGCONFIG_WRAPPER_TEMPLATE,
                sysroot=sysroot)

  # Create a link to the debug symbols in the chroot so that gdb can detect
  # them.
  debug_symlink = os.path.join('/usr/lib/debug', sysroot.lstrip('/'))
  sysroot_debug = os.path.join(sysroot, 'usr/lib/debug')
  osutils.SafeMakedirs(os.path.dirname(debug_symlink))
  osutils.SafeMakedirs(sysroot_debug)

  # If the symlink already exists, recreate it to be sure.
  if os.path.islink(debug_symlink):
    os.unlink(debug_symlink)

  os.symlink(sysroot_debug, debug_symlink)


def _DictToKeyValue(dictionary):
  """Formats dictionary in to a key=value string.

  Args:
    dictionary: a python dictionary.
  """
  output = []
  for key in sorted(dictionary.keys()):
    output.append('%s="%s"' % (key, dictionary[key]))

  return '\n'.join(output)


def GenerateBoardConfig(sysroot, board):
  """Generate the sysroot configuration for a given board.

  Args:
    sysroot: path to the sysroot.
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
  config['ROOT'] = sysroot + '/'
  config['PKG_CONFIG'] = 'pkg-config-%s' % board

  header = "# Created by cros_sysroot_utils from --board=%s." % board
  return '\n'.join((header, _DictToKeyValue(config)))


def GenerateBrickConfig(sysroot, brick):
  """Generate the sysroot configuration for a given brick.

  Args:
    sysroot: path to the sysroot.
    brick: brick to generate the configuration for.
  """
  config = {}
  # TODO(bsimonnet): get toolchain from config.json.
  toolchains = toolchain.GetToolchainsForBrick(brick.brick_locator)
  config['CHOST'] = toolchain.FilterToolchains(toolchains, 'default',
                                               True).keys()[0]
  config['ARCH'] = toolchain.GetArchForTarget(config['CHOST'])

  config['BOARD_OVERLAY'] = '\n'.join([b.OverlayDir()
                                       for b in brick.BrickStack()])
  config['MAKEOPTS'] = '-j%s' % str(multiprocessing.cpu_count())
  config['ROOT'] = sysroot + '/'
  config['PKG_CONFIG'] = 'pkg-config-%s' % brick.FriendlyName()

  header = ("# Created by cros_sysroot_utils from --brick=%s."
            % brick.brick_locator)
  return '\n'.join((header, _DictToKeyValue(config)))


def WriteSysrootConfig(sysroot, config):
  """Writes the configuration in |sysroot|.

  Args:
    sysroot: path to the sysroot.
    config: configuration to use.
  """
  path = os.path.join(sysroot, _CONFIGURATION_PATH)
  osutils.WriteFile(path, config, makedirs=True)
