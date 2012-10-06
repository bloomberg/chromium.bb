# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This package contains all valid cros commands and their unittests.

All commands can be either imported directly or looked up using this module.
This modules contains a dictionary called commands mapping
command names -> command classes e.g. image->cros_image.ImageCommand.
"""

import glob
import imp
import os

# Cyclic imports get resolved by just breaking/ignoring the backwards edge of
# the cycle. So when we're trying to |import command| from within a file we're
# importing, that file will only be able to see the modules we've imported thus
# far within __init__.py. Therefore, we need to |import command| first because
# then every command we're about to import can successfully do |import command|.
import command


def _FindModules(subdir_path):
  """Returns a list of all the relevant python modules in |sub_dir_path|"""
  # We only load cros_[!unittest] modules.
  modules = []
  for file_path in glob.glob(subdir_path + '/cros_*.py'):
    if not file_path.endswith('_unittest.py'):
      modules.append(file_path)

  return modules


def _ImportCommands():
  """Directly imports all cros_[!unittest] python modules.

  This method imports the cros_[!unittest] modules which may contain
  commands. When these modules are loaded, declared commands (those that use
  the command.CommandDecorator) will automatically get added to
  command._commands.
  """
  subdir_path = os.path.dirname(__file__)
  for file_path in _FindModules(subdir_path):
    file_name = os.path.basename(file_path)
    mod_name = os.path.splitext(file_name)[0]
    imp.load_module(mod_name, *imp.find_module(mod_name, [subdir_path]))


_ImportCommands()

commands = command._commands

# Clear the namespace.
del command, glob, imp, os
