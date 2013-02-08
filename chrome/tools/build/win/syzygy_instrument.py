#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A utility script to help building Syzygy-instrumented Chrome binaries."""

import logging
import optparse
import os
import shutil
import subprocess
import sys


# The default instrument executable to use to instrument binaries.
_DEFAULT_INSTRUMENTER = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '../../../..',
    'third_party/syzygy/binaries/exe/instrument.exe'))

_LOGGER = logging.getLogger()

def _Shell(*cmd, **kw):
  """Shells out to "cmd". Returns a tuple of cmd's stdout, stderr."""
  _LOGGER.info('Running command "%s".', cmd)
  prog = subprocess.Popen(cmd, **kw)

  stdout, stderr = prog.communicate()
  if prog.returncode != 0:
    raise RuntimeError('Command "%s" returned %d.' % (cmd, prog.returncode))

  return stdout, stderr


def _InstrumentBinary(instrument_exe, mode, executable, symbol, dst_dir):
  """Instruments the executable found in input_dir, and writes the resultant
  instrumented executable and symbol files to dst_dir.
  """
  cmd = [instrument_exe,
         '--overwrite',
         '--mode=%s' % mode,
         '--input-image=%s' % executable,
         '--input-pdb=%s' % symbol,
         '--output-image=%s' % os.path.abspath(
             os.path.join(dst_dir, os.path.basename(executable))),
         '--output-pdb=%s' % os.path.abspath(
             os.path.join(dst_dir, os.path.basename(symbol))),]

  return _Shell(*cmd)

def _CopyAgentDLL(agent_dll, destination_dir):
  """Copy the agent DLL to the destination directory."""
  dirname, agent_name = os.path.split(agent_dll);
  agent_dst_name = os.path.join(destination_dir, agent_name);
  shutil.copyfile(agent_dll, agent_dst_name)

def main(options):
  logging.basicConfig(level=logging.INFO)

  # Make sure the destination directory exists.
  if not os.path.isdir(options.destination_dir):
    _LOGGER.info('Creating destination directory "%s".',
                 options.destination_dir)
    os.makedirs(options.destination_dir)

  # Instruments the binaries into the destination directory.
  _InstrumentBinary(options.instrumenter,
                    options.mode,
                    options.input_executable,
                    options.input_symbol,
                    options.destination_dir)

  # Copy the agent DLL to the destination directory.
  _CopyAgentDLL(options.agent_dll, options.destination_dir);


def _ParseOptions():
  option_parser = optparse.OptionParser()
  option_parser.add_option('--input_executable',
      help='The path to the input executable.')
  option_parser.add_option('--input_symbol',
      help='The path to the input symbol file.')
  option_parser.add_option('--mode',
      help='Specifies which instrumentation mode is to be used.')
  option_parser.add_option('--agent_dll',
      help='The agent DLL used by this instrumentation.')
  option_parser.add_option('--instrumenter', default=_DEFAULT_INSTRUMENTER,
      help='Instrumenter executable to use, defaults to "%s"'
          % _DEFAULT_INSTRUMENTER)
  option_parser.add_option('-d', '--destination_dir',
      help='Destination directory for instrumented files, defaults to '
           'the subdirectory "instrumented" in the output_dir.')
  options, args = option_parser.parse_args()

  if not options.mode:
    option_parser.error('You must provide an instrumentation mode.')
  if not options.agent_dll:
    option_parser.error('You must provide an agent DLL.')
  if not options.input_executable:
    option_parser.error('You must provide an input executable.')
  if not options.input_symbol:
    option_parser.error('You must provide an input symbol file.')

  if not options.destination_dir:
    options.destination_dir = os.path.join(options.output_dir, 'instrumented')

  return options


if '__main__' == __name__:
  sys.exit(main(_ParseOptions()))
