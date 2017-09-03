# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Runs the Microsoft Message Compiler (mc.exe). This Python adapter is for the
# GN build, which can only run Python and not native binaries.
#
# Usage: message_compiler.py <environment_file> [<args to mc.exe>*]

import distutils.dir_util
import filecmp
import os
import re
import shutil
import subprocess
import sys
import tempfile

def main():
  env_file, rest = sys.argv[1], sys.argv[2:]

  # Parse some argument flags.
  header_dir = None
  resource_dir = None
  input_file = None
  for i, arg in enumerate(rest):
    if arg == '-h' and len(rest) > i + 1:
      assert header_dir == None
      header_dir = rest[i + 1]
    elif arg == '-r' and len(rest) > i + 1:
      assert resource_dir == None
      resource_dir = rest[i + 1]
    elif arg.endswith('.mc') or arg.endswith('.man'):
      assert input_file == None
      input_file = arg

  # Copy checked-in outputs to final location.
  THIS_DIR = os.path.abspath(os.path.dirname(__file__))
  assert header_dir == resource_dir
  source = os.path.join(THIS_DIR, "..", "..",
      "third_party", "win_build_output",
      re.sub(r'^(?:[^/]+/)?gen/', 'mc/', header_dir))
  distutils.dir_util.copy_tree(source, header_dir, preserve_times=False)

  # On non-Windows, that's all we can do.
  if sys.platform != 'win32':
    return

  # On Windows, run mc.exe on the input and check that its outputs are
  # identical to the checked-in outputs.

  # Read the environment block from the file. This is stored in the format used
  # by CreateProcess. Drop last 2 NULs, one for list terminator, one for
  # trailing vs. separator.
  env_pairs = open(env_file).read()[:-2].split('\0')
  env_dict = dict([item.split('=', 1) for item in env_pairs])

  # mc writes to stderr, so this explicitly redirects to stdout and eats it.
  try:
    tmp_dir = tempfile.mkdtemp()
    delete_tmp_dir = True
    if header_dir:
      rest[rest.index('-h') + 1] = tmp_dir
      header_dir = tmp_dir
    if resource_dir:
      rest[rest.index('-r') + 1] = tmp_dir
      resource_dir = tmp_dir

    # This needs shell=True to search the path in env_dict for the mc
    # executable.
    subprocess.check_output(['mc.exe'] + rest,
                            env=env_dict,
                            stderr=subprocess.STDOUT,
                            shell=True)
    # We require all source code (in particular, the header generated here) to
    # be UTF-8. jinja can output the intermediate .mc file in UTF-8 or UTF-16LE.
    # However, mc.exe only supports Unicode via the -u flag, and it assumes when
    # that is specified that the input is UTF-16LE (and errors out on UTF-8
    # files, assuming they're ANSI). Even with -u specified and UTF16-LE input,
    # it generates an ANSI header, and includes broken versions of the message
    # text in the comment before the value. To work around this, for any invalid
    # // comment lines, we simply drop the line in the header after building it.
    if header_dir:
      header_file = os.path.join(
          header_dir, os.path.splitext(os.path.basename(input_file))[0] + '.h')
      header_contents = []
      with open(header_file, 'rb') as f:
        for line in f.readlines():
          if line.startswith('//') and '?' in line:
            continue
          header_contents.append(line)
      with open(header_file, 'wb') as f:
        f.write(''.join(header_contents))

    # mc.exe invocation and post-processing are complete, now compare the output
    # in tmp_dir to the checked-in outputs.
    diff = filecmp.dircmp(tmp_dir, source)
    if diff.diff_files or set(diff.left_list) != set(diff.right_list):
      print >>sys.stderr, 'mc.exe output different from files in %s, see %s' % (
          source, tmp_dir)
      delete_tmp_dir = False
      sys.exit(1)
  except subprocess.CalledProcessError as e:
    print e.output
    sys.exit(e.returncode)
  finally:
    if os.path.exists(tmp_dir) and delete_tmp_dir:
      shutil.rmtree(tmp_dir)

if __name__ == '__main__':
  main()
