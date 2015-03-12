# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility functions for calculating compatible binhosts."""

from __future__ import print_function

import collections
import os

from chromite.cbuildbot import cbuildbot_config
from chromite.cbuildbot import constants
from chromite.lib import cros_build_lib


# A unique identifier for looking up CompatIds by board/useflags.
BoardKey = collections.namedtuple('BoardKey', ['board', 'useflags'])


def GetBoardKey(config, board=None):
  """Get the BoardKey associated with a given config.

  Args:
    config: A cbuildbot_config._config object.
    board: Board to use. Defaults to the first board in the config.
      Optional if len(config.boards) == 1.
  """
  if board is None:
    assert len(config.boards) == 1
    board = config.boards[0]
  else:
    assert board in config.boards
  return BoardKey(board, tuple(config.useflags))


def GetAllBoardKeys():
  """Get a list of all board keys used in a top-level config."""
  boards = set()
  for config in cbuildbot_config.config.values():
    for board in config.boards:
      boards.add(GetBoardKey(config, board))
  return boards


def GetChromePrebuiltConfigs():
  """Get a mapping of the boards used in the Chrome PFQ.

  Returns:
    A dict mapping BoardKey objects to configs.
  """
  boards = {}
  master_chromium_pfq = cbuildbot_config.config['master-chromium-pfq']
  for config in cbuildbot_config.GetSlavesForMaster(master_chromium_pfq):
    if config.prebuilts:
      for board in config.boards:
        boards[GetBoardKey(config, board)] = config
  return boards


def GetChromeUseFlags(board, extra_useflags):
  """Get a list of the use flags turned on for Chrome on a given board.

  This function requires that the board has been set up first (e.g. using
  GenConfigsForBoard)

  Args:
    board: The board to use.
    extra_useflags: A sequence of use flags to enable or disable.

  Returns:
    A tuple of the use flags that are enabled for Chrome on the given board.
    Use flags that are disabled are not listed.
  """
  assert os.path.exists('/build/%s' % board)
  extra_env = {'USE': ' '.join(extra_useflags)}
  cmd = ['equery-%s' % board, 'uses', constants.CHROME_CP]
  chrome_useflags = cros_build_lib.RunCommand(
      cmd, capture_output=True, print_cmd=False,
      extra_env=extra_env).output.rstrip().split()
  return tuple(x[1:] for x in chrome_useflags if x.startswith('+'))


def GenConfigsForBoard(board, regen, error_code_ok):
  """Set up the configs for the specified board.

  This must be run from within the chroot. It sets up the board but does not
  fully initialize it (it skips the initialization of the toolchain and the
  board packages)

  Args:
    board: Board to set up.
    regen: Whether to regen configs if the board already exists.`
    error_code_ok: Whether errors are acceptable. We set this to True in some
      tests for configs that are not on the waterfall.
  """
  if regen or not os.path.exists('/build/%s' % board):
    cmd = ['%s/src/scripts/setup_board' % constants.CHROOT_SOURCE_ROOT,
           '--board=%s' % board, '--regen_configs', '--skip_toolchain_update',
           '--skip_chroot_upgrade', '--skip_board_pkg_init', '--quiet']
    cros_build_lib.RunCommand(cmd, error_code_ok=error_code_ok)


CompatId = collections.namedtuple('CompatId', ['arch', 'useflags', 'cflags'])


def CalculateCompatId(board, extra_useflags):
  """Calculate the CompatId for board with the specified extra useflags.

  This function requires that the board has been set up first (e.g. using
  GenConfigsForBoard)

  Args:
    board: The board to use.
    extra_useflags: A sequence of use flags to enable or disable.

  Returns:
    A CompatId object for the board with the specified extra_useflags.
  """
  useflags = GetChromeUseFlags(board, extra_useflags)
  cmd = ['portageq-%s' % board, 'envvar', 'ARCH', 'CFLAGS']
  arch_cflags = cros_build_lib.RunCommand(
      cmd, print_cmd=False, capture_output=True).output.rstrip()
  arch, cflags = arch_cflags.split('\n', 1)
  return CompatId(arch, useflags, tuple(cflags.split()))
