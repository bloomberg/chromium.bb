# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utilities to create sysroots."""

from __future__ import print_function

import os

from chromite.cbuildbot import constants
from chromite.lib import osutils


_PORTAGE_WRAPPER_TEMPLATE = """#!/bin/sh

# If we try to use sudo when the sandbox is active, we get ugly warnings that
# just confuse developers.  Disable the sandbox in this case by rexecing.
if [ "${{SANDBOX_ON}}" = "1" ]; then
  SANDBOX_ON=0 exec "$0" "$@"
else
  unset LD_PRELOAD
fi

export CHROMEOS_ROOT="{source_root}"
export CHOST="{toolchain}"
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


def CreateAllWrappers(sysroot, toolchain, friendly_name=None):
  """Creates all the wrappers for a given sysroot.

  Creates all portage tools wrappers, plus wrappers for gdb, cros_workon and
  pkg-config.

  Args:
    sysroot: path to the sysroot.
    toolchain: toolchain to use.
    friendly_name: suffix to add to the commands. Defaults to the last component
      of |sysroot|.
  """
  board = friendly_name or os.path.basename(sysroot)
  for cmd in ('ebuild', 'eclean', 'emaint', 'equery', 'portageq', 'qcheck',
              'qdepends', 'qfile', 'qlist', 'qmerge', 'qsize'):
    CreateWrapper('%s-%s' % (cmd, board), _PORTAGE_WRAPPER_TEMPLATE,
                  sysroot=sysroot, toolchain=toolchain, command=cmd,
                  source_root=constants.SOURCE_ROOT)

  CreateWrapper('emerge-%s' % board, _PORTAGE_WRAPPER_TEMPLATE,
                sysroot=sysroot, toolchain=toolchain,
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
