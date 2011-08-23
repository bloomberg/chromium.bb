#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse
import os
import re
import shutil
import subprocess
import sys


# Where things are in relation to this script.
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SRC_DIR = os.path.dirname(SCRIPT_DIR)
NACL_DIR = os.path.join(SRC_DIR, 'native_client')

# Pathing to the two command_buffer directories (relative to native_client).
NACL_CMD_BUFFER_DIR = os.path.join('src', 'shared',
                                   'ppapi_proxy', 'command_buffer')
GPU_CMD_BUFFER_DIR = os.path.join('..', 'gpu', 'command_buffer')


def RelativePath(path, base):
  """Find the relative path.

  Arguments:
    path: path we want a relative path to.
    base: path we want a relative path from.
  Returns:
    The relative path from base to path.
  """
  path = os.path.abspath(path)
  base = os.path.abspath(base)
  path_parts = path.split(os.sep)
  base_parts = base.split(os.sep)
  while path_parts and base_parts and path_parts[0] == base_parts[0]:
    path_parts = path_parts[1:]
    base_parts = base_parts[1:]
  rel_parts = ['..'] * len(base_parts) + path_parts
  return os.sep.join(rel_parts)


def PrintInputs(platforms):
  """Print all the transitive inputs required to build the IRT.

  Arguments:
    platforms: list of platform names to build for.
  """
  inputs = set()
  for platform in platforms:
    # Invoke scons to get dependency tree.
    cmd = [
        sys.executable, 'scons.py', '-n', '--tree=all',
        '--mode=nacl', 'platform=' + platform,
        'scons-out/nacl_irt-' + platform + '/staging/irt.nexe',
    ]
    p = subprocess.Popen(cmd, cwd=NACL_DIR,
                         stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE)
    (p_stdout, p_stderr) = p.communicate()
    if p.returncode != 0:
      sys.exit(2)
    # Extract unique inputs.
    for line in p_stdout.splitlines():
      m = re.match('^[ -+|]*\+\-(.+)', line)
      if not m:
        continue
      filename = m.group(1)
      if '[' in filename:
        continue
      if filename.startswith('scons-out'):
        continue
      if filename.endswith('.nexe'):
        continue
      # Apply the underlay of gpu/command_buffer (to match scons).
      if filename.startswith(NACL_CMD_BUFFER_DIR + os.sep):
        filename = GPU_CMD_BUFFER_DIR + filename[len(NACL_CMD_BUFFER_DIR):]
      inputs.add(filename)
  # Check that everything exists and make it script relative.
  # Exclude things above SRC_DIR.
  rel_inputs = set()
  for f in inputs:
    nf = os.path.join(NACL_DIR, f)
    if not os.path.exists(nf):
      raise Exception('missing input file "%s"' % nf)
    # If the relative path from SRC_DIR to the file starts with ../ ignore it.
    # (i.e. the file is outside the client).
    if RelativePath(nf, SRC_DIR).startswith('..' + os.sep):
      continue
    rel_inputs.add(RelativePath(nf, SCRIPT_DIR))
  # Print it sorted.
  rel_inputs = sorted(list(rel_inputs))
  for f in rel_inputs:
    print f


def BuildIRT(platforms, out_dir):
  """Build the IRT for several platforms.

  Arguments:
    platforms: list of platform names to build for.
    out_dir: directory to output the IRT to.
  """
  # Make out_dir absolute.
  out_dir = os.path.abspath(out_dir)
  # Clean.
  scons_out = os.path.join(NACL_DIR, 'scons-out')
  if os.path.exists(scons_out):
    shutil.rmtree(scons_out)
  # Build for each platform.
  for platform in platforms:
    cmd = [
        sys.executable, 'scons.py', '--verbose', '-j8',
        '--mode=nacl', 'platform=' + platform,
        'scons-out/nacl_irt-' + platform + '/staging/irt.nexe',
    ]
    print 'Running: ' + ' '.join(cmd)
    # Work around the fact that python's readline module (used by scons),
    # attempts to alter file handle state on stdin in a way that blocks if
    # a process is not a member of a foreground job on a tty on OSX.
    # e.g. On a Mac:
    #
    # hydric:test mseaborn$ python -c 'import readline' &
    # [1] 67058
    # hydric:test mseaborn$
    # [1]+  Stopped                 python -c 'import readline'
    #
    # i.e. the process receives a stop signal when it's a background job.
    if sys.platform == 'darwin':
      devnull = open(os.devnull, 'r')
    else:
      devnull = None
    p = subprocess.Popen(cmd, cwd=NACL_DIR, stdin=devnull)
    p.wait()
    if p.returncode != 0:
      sys.exit(3)
  # Copy out each platform after stripping.
  for platform in platforms:
    uplatform = platform.replace('-', '_')
    platform2 = {'x86-32': 'i686', 'x86-64': 'x86_64'}.get(platform, platform)
    cplatform = {
        'win32': 'win',
        'cygwin': 'win',
        'darwin': 'mac',
    }.get(sys.platform, 'linux')
    nexe = os.path.join(out_dir, 'nacl_irt_' + uplatform + '.nexe')
    cmd = [
      '../native_client/toolchain/' + cplatform + '_x86_newlib/bin/' +
          platform2 + '-nacl-strip',
      '--strip-debug',
      '../native_client/scons-out/nacl_irt-' + platform + '/staging/irt.nexe',
      '-o', nexe
    ]
    print 'Running: ' + ' '.join(cmd)
    p = subprocess.Popen(cmd, cwd=SCRIPT_DIR)
    p.wait()
    if p.returncode != 0:
      sys.exit(4)


def Main(argv):
  parser = optparse.OptionParser()
  parser.add_option('--inputs', dest='inputs', default=False,
                    action='store_true',
                    help='only emit the transitive inputs to the irt build')
  parser.add_option('--platform', dest='platforms', action='append',
                    default=[],
                    help='add a platform to build for (x86-32|x86-64)')
  parser.add_option('--outdir', dest='outdir',
                    help='directory to out irt to')
  (options, args) = parser.parse_args(argv[1:])
  if args or not options.platforms or (
      not options.inputs and not options.outdir):
    parser.print_help()
    sys.exit(1)

  if options.inputs:
    PrintInputs(options.platforms)
  else:
    BuildIRT(options.platforms, options.outdir)


if __name__ == '__main__':
  Main(sys.argv)
