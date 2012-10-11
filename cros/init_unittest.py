#!/usr/bin/python

# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module tests the command module."""

import argparse
import os
import sys

sys.path.insert(0, os.path.abspath('%s/../..' % os.path.dirname(__file__)))

from chromite.lib import cros_test_lib
from chromite import cros

_COMMAND_NAME = 'superAwesomeCommandOfFunness'


@cros.CommandDecorator(_COMMAND_NAME)
class TestCommand(cros.CrosCommand):
  """A fake command."""
  def Run(self):
    print 'Just testing'


# pylint: disable=W0212
class CommandTest(cros_test_lib.TestCase):
  """This test class tests that Commands method."""

  def testParserSetsCrosClass(self):
    """Tests that our parser sets cros_class correctly."""
    my_parser = argparse.ArgumentParser()
    cros.CrosCommand.AddParser(my_parser)
    ns = my_parser.parse_args([])
    self.assertEqual(ns.cros_class, cros.CrosCommand)

  def testCommandDecorator(self):
    """Tests that our decorator correctly adds TestCommand to _commands."""
    # Note this exposes an implementation detail of _commands.
    self.assertEqual(cros._commands[_COMMAND_NAME], TestCommand)

  def testBadUseOfCommandDecorator(self):
    """Tests that our decorator correctly rejects bad test commands."""
    try:
      # pylint: disable=W0612
      @cros.CommandDecorator('bad')
      class BadTestCommand(object):
        """A command that wasn't implemented correctly."""
        pass

    except cros.InvalidCommandError:
      pass
    else:
      self.fail('Invalid command was accepted by the CommandDecorator')


if __name__ == '__main__':
  cros_test_lib.main()
