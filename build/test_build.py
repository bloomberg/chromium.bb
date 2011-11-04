#!/usr/bin/python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import os
import subprocess
import sys


"""Building verification script

This module will take the output directory path, base nexe name and
architecture and combine them to generate a sel_ldr execution command
line using irt_core of the appropriate architecture.
"""

def UseWin64():
  if sys.platform != 'win32':
    return False
  arch32 = os.environ.get('PROCESSOR_ARCHITECTURE', 'unk')
  arch64 = os.environ.get('PROCESSOR_ARCHITEW6432', 'unk')

  if arch32 == 'AMD64' or arch64 == 'AMD64':
    return True
  return False


def ErrOut(text):
  """ErrOut prints an error message and the command-line that caused it.

  Prints to standard err, both the command-line normally, and separated by
  >>...<< to make it easier to copy and paste the command, or to
  find command formating issues.
  """
  sys.stderr.write('\n\n')
  sys.stderr.write( '>>>' + '>> <<'.join(sys.argv) + '<<\n\n')
  sys.stderr.write(' '.join(sys.argv) + '<<\n\n')
  sys.stderr.write(text + '\n')
  sys.exit(-1)


def Run(cmd_line):
  sys.stdout.write('Run: %s\n' % ' '.join(cmd_line))
  sys.stdout.flush()
  try:
    ecode = subprocess.call(cmd_line)
  except Exception, err:
    ErrOut('\nFAILED: %s\n' % str(err))

  if ecode:
    ErrOut('FAILED with return value: %d\n' % ecode)
  else:
    sys.stdout.write('Success\n')
    sys.stdout.flush()
  return ecode


def GetNexe(path, name, arch, tools):
  arch_map = {
    'ia32': 'x32',
    'x64': 'x64',
  }

  if UseWin64():
    arch = 'x64'

  return os.path.join(path, '%s_%s_%s.nexe' % (name, tools, arch_map[arch]))


def Main(argv):
  if len(argv) != 4:
    ErrOut('Expecting: %s <BUILD_PATH> <NEXE> <ARCH>\n' % argv[0])

  cmd_line = [os.path.join(argv[1], 'sel_ldr'),
              '-B',
              GetNexe(argv[1], 'irt_core', argv[3], 'newlib'),
              GetNexe(argv[1], argv[2], argv[3], 'newlib'),
             ]

  # For 64 bit Windows, we use sel_ldr64 instead of sel_ldr since we build both
  if UseWin64():
    cmd_line[0] = os.path.join(argv[1], 'sel_ldr64')
  return Run(cmd_line)


if __name__ == '__main__':
  sys.exit(Main(sys.argv))

