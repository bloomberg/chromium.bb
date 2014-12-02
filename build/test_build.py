#!/usr/bin/python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


from optparse import OptionParser
import os
import subprocess
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), '..'))
import pynacl.platform

NATIVE_CLIENT_DIR = os.path.dirname(os.path.dirname(__file__))
TOOLCHAIN_DIR = os.path.join(NATIVE_CLIENT_DIR, 'toolchain')

"""Building verification script

This module will take the output directory path, base nexe name and
architecture and combine them to generate a sel_ldr execution command
line using irt_core of the appropriate architecture.
"""


def RelativePath(filepath):
  """Return a POSIX style relative path."""
  cwdpath = os.path.abspath(os.getcwd())
  cwdparts = cwdpath.split(os.path.sep)

  filepath = os.path.abspath(filepath)

  # For Windows, convert to POSIX style path
  if sys.platform == 'win32':
    filepath = filepath.replace('\\', '/')
  fileparts = filepath.split('/')
  for index in range(len(fileparts)):
    if index >= len(cwdparts):
      break
    if cwdparts[index] != fileparts[index]:
      break

  # Add 'cd ..' the rest of cwd that doesn't match
  out = '../' * (len(cwdparts) - index)
  out += '/'.join(fileparts[index:])
  return out


def UseWin64():
  """Check if we are on 64 bit windows."""
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
  sys.exit(1)

def Run(cmd_line):
  """Run the provided command-line returning the error code.

  Uses supbrocess to execute the command line, printing debugging information
  as well as error messages as appropriate.
  """
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


def GetToolchainPath(tool):
  """Given the requested tool, compute the path to that toolchain."""
  os_name = pynacl.platform.GetOS()

  if tool == 'newlib':
    lib_name = 'newlib'
  else:
    lib_name = 'glibc'

  return os.path.join(
      TOOLCHAIN_DIR,
      '%s_x86' % (os_name),
      'nacl_x86_%s_raw' % lib_name
  )


def GetLibPath(tool, arch):
  """For a given tool and architecture, determine the library path."""
  arch_map = {
    'ia32': 'lib32',
    'x64': 'lib',
  }
  lib = arch_map[arch]

  # For 64bit win, the arch is incorrectly reported as 32bit, so override it.
  if UseWin64():
    lib = 'lib'
  return os.path.join(GetToolchainPath(tool), 'x86_64-nacl', lib)


def GetNexe(path, name, arch, tool):
  """For a given nexe name, architecture, and tool generate a path to it."""
  arch_map = {
    'ia32': 'x32',
    'x64': 'x64',
  }
  # For 64bit win, the arch is incorrectly reported as 32bit, so override it.
  if UseWin64():
    arch = 'x64'
  return os.path.join(path, '%s_%s_%s.nexe' % (name, tool, arch_map[arch]))


def GetLdrIrtNexe(path, nexe, arch, tool):
  sel_ldr = os.path.join(path, 'sel_ldr')
  # If incorrectly reported use sel_ldr64 otherwise assume sel_ldr is already
  # the correct architecture.
  if UseWin64() and arch == 'ia32':
    sel_ldr = os.path.join(path, 'sel_ldr64')

  # Get IRT_CORE which is built with newlib
  irt_core = GetNexe(path, 'irt_core', arch, 'newlib')
  nexe = GetNexe(path, nexe, arch, tool)
  return (sel_ldr, irt_core, nexe)


def BuildCmdLine(path, nexe, arch, tool):
  (sel_ldr, irt_core, nexe) = GetLdrIrtNexe(path, nexe, arch, tool)
  if tool in ('newlib', 'pnacl_newlib'):
    return [sel_ldr, '-B', irt_core, '--', nexe]

  # For GLIBC we need to add 'runnable-ld.so' and allow sel_ldr access to
  # the file system.
  libpath = GetLibPath(tool, arch)
  ldso = os.path.join(libpath, 'runnable-ld.so')
  # Force POSIX style paths required by runnable-ld
  nexe = nexe.replace('\\', '/')
  return [sel_ldr, '-a', '-B', irt_core, '--', ldso, '--library-path',
          libpath, nexe]


def RunLoader(path, nexe, arch, tools, out):
  fail = 0
  for tool in tools:
    print '\n\nRunning: %s %s %s' % (nexe, arch, tool)
    cmd_line = BuildCmdLine(path, nexe, arch, tool)
    err = Run(cmd_line)
    if err:
      fail = fail + 1
      if out:
        out.write('%s on %s built with %s, failed returning %s.\n' %
                  (nexe, arch, tool, err))
    else:
      if out:
        out.write('SUCCESS: %s on %s built with %s.\n' % (nexe, arch, tool))
  return fail


def LogLoader(path, nexe, arch, tools):
  for tool in tools:
    cmd_line = BuildCmdLine(path, nexe, arch, tool)
    print ' '.join(cmd_line)
  return 0


def LogDeps(path, nexe, arch, tools):
  out_set = set()
  out_set.add(RelativePath(__file__))

  for tool in tools:
    out_set |= set(GetLdrIrtNexe(path, nexe, arch, tool))
  for out in out_set:
    # Emit forward slashes here as that's what gyp expects.
    print out.replace('\\', '/')


def Main(argv):
  parser = OptionParser()

  # Test build modes
  parser.add_option('-i', '--input', help='Generate input list.',
      action='store_const', const='i', dest='mode', default='')
  parser.add_option('-r', '--run', help='Run the nexe.',
      action='store_const', const='r', dest='mode', default='')
  parser.add_option('-l', '--log', help='Log the command-lines to run.',
      action='store_const', const='l', dest='mode', default='')

  # Options
  parser.add_option('-v', '--verbose', dest='verbose', default=False,
                    help='Enable verbosity', action='store_true')
  parser.add_option('-o', '--output', dest='output', default='',
                    help='Set output file name.', action='store')
  parser.add_option('-p', '--path', dest='path', default='.',
                    help='Set path to sel_ldr and NEXEs.', action='store')
  parser.add_option('-n', '--name', dest='name', action='store', default='',
                    help='Base name of the NEXE.')
  parser.add_option('-x', '--arch', dest='arch', action='store', default='',
                    help='Run architecture.')
  parser.add_option('-t', '--tools', dest='tools', action='append', default=[],
                    help='Select which toolchains versions to run.')

  options, nexe_args = parser.parse_args(argv[1:])
  if not options.name:
    parser.error('Missing NEXE name.')
  if not options.mode or options.mode not in ['i', 'r', 'l']:
    parser.error('Missing run mode.')
  if not options.arch:
    parser.error('Missing run architecture.')
  if not options.tools:
    parser.error('Missing toolchains.')

  # TODO(bradnelson): Hack to prevent gyp generated path ending with \" from
  #               being interpreted as escaped quote. Here we strip the extra
  #               '/hack' we added in the gyp file.
  # http://code.google.com/p/chromium/issues/detail?id=141463
  if options.mode is 'r':
    options.path = os.path.dirname(options.path)

  if options.output:
    try:
      out = open(options.output, 'w')
    except EnvironmentError:
      print 'Failed to open %s.\n' % options.output
      return 1
  else:
    out = None

  tools = ','.join(options.tools)
  tools = tools.split(',')
  fail = 0
  for tool in tools:
    if tool not in ['newlib', 'glibc', 'pnacl_newlib']:
      parser.error('Unknown tool: %s\n' % tool)

  path = options.path
  nexe = options.name
  arch = options.arch

  if options.mode == 'r':
    fail = RunLoader(path, nexe, arch, tools, out)
  if options.mode == 'l':
    fail = LogLoader(path, nexe, arch, tools)
  if options.mode == 'i':
    fail = LogDeps('<(PRODUCT_DIR)', nexe, arch, tools)

  if out:
    out.close()
  return fail


if __name__ == '__main__':
  sys.exit(Main(sys.argv))
