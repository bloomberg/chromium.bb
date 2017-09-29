#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A tool to extract size information for chrome, executed by buildbot.

When this is run, the current directory (cwd) should be the outer build
directory (e.g., chrome-release/build/).

For a list of command-line options, call this script with '--help'.
"""

import errno
import json
import platform
import optparse
import os
import re
import stat
import subprocess
import sys
import tempfile

from slave import build_directory

SRC_DIR = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..', '..', '..', '..'))

EXPECTED_LINUX_SI_COUNTS = {
  'chrome': 7,
  'nacl_helper': 7,
  'nacl_helper_bootstrap': 7,
}

EXPECTED_MAC_SI_COUNT = 0


def run_process(result, command):
  p = subprocess.Popen(command, stdout=subprocess.PIPE)
  stdout = p.communicate()[0]
  if p.returncode != 0:
    print 'ERROR from command "%s": %d' % (' '.join(command), p.returncode)
    if result == 0:
      result = p.returncode
  return result, stdout


def print_si_fail_hint(path_to_tool):
  """Print a hint regarding how to handle a static initializer failure."""
  print '# HINT: To get this list, run %s' % path_to_tool
  print '# HINT: diff against the log from the last run to see what changed'


def main_mac(options):
  """Print appropriate size information about built Mac targets.

  Returns the first non-zero exit status of any command it executes,
  or zero on success.
  """
  build_dir = build_directory.GetBuildOutputDirectory(SRC_DIR)
  target_dir = os.path.join(build_dir, options.target)

  result = 0
  failures = []
  # Work with either build type.
  base_names = ('Chromium', 'Google Chrome')
  for base_name in base_names:
    app_bundle = base_name + '.app'
    framework_name = base_name + ' Framework'
    framework_bundle = framework_name + '.framework'
    framework_dsym_bundle = framework_bundle + '.dSYM'
    framework_unstripped_name = framework_name + '.unstripped'

    chromium_app_dir = os.path.join(target_dir, app_bundle)
    chromium_executable = os.path.join(chromium_app_dir,
                                       'Contents', 'MacOS', base_name)

    chromium_framework_dir = os.path.join(target_dir, framework_bundle)
    chromium_framework_executable = os.path.join(chromium_framework_dir,
                                                 framework_name)

    chromium_framework_dsym_dir = os.path.join(target_dir,
                                               framework_dsym_bundle)
    chromium_framework_dsym = os.path.join(chromium_framework_dsym_dir,
                                           'Contents', 'Resources', 'DWARF',
                                           framework_name)
    chromium_framework_unstripped = os.path.join(target_dir,
                                                 framework_unstripped_name)
    if os.path.exists(chromium_executable):
      # Count the number of files with at least one static initializer.
      pipes = [['otool', '-l', chromium_framework_executable],
               ['grep', '__mod_init_func', '-C', '5'],
               ['grep', 'size']]
      last_stdout = None
      for pipe in pipes:
        p = subprocess.Popen(pipe, stdin=last_stdout, stdout=subprocess.PIPE)
        last_stdout = p.stdout
      stdout = p.communicate()[0]
      initializers = re.search('0x([0-9a-f]+)', stdout)
      if initializers:
        initializers_s = initializers.group(1)
        if result == 0:
          result = p.returncode
      else:
        initializers_s = '0'
      word_size = 8  # Assume 64 bit
      si_count = int(initializers_s, 16) / word_size
      if si_count != EXPECTED_MAC_SI_COUNT:
        failures.append(
            'Binary contains %d static initializers (expected %d)' %
            (si_count, EXPECTED_MAC_SI_COUNT))

      # Use dump-static-initializers.py to print the list of SIs.
      if si_count > 0:
        print '\n# Static initializers in %s:' % chromium_framework_executable

        # First look for a dSYM to get information about the initializers. If
        # one is not present, check if there is an unstripped copy of the build
        # output.
        mac_tools_path = os.path.join(
            os.path.dirname(build_dir), 'tools', 'mac')
        if os.path.exists(chromium_framework_dsym):
          dump_static_initializers = os.path.join(
              mac_tools_path, 'dump-static-initializers.py')
          result, stdout = run_process(
              result, [dump_static_initializers, chromium_framework_dsym])
          print_si_fail_hint('tools/mac/dump-static-initializers.py')
          print stdout
        else:
          show_mod_init_func = os.path.join(
              mac_tools_path, 'show_mod_init_func.py')
          args = [show_mod_init_func]
          if os.path.exists(chromium_framework_unstripped):
            args.append(chromium_framework_unstripped)
          else:
            print '# Warning: Falling back to potentially stripped output.'
            args.append(chromium_framework_executable)
          result, stdout = run_process(result, args)
          print_si_fail_hint('tools/mac/show_mod_init_func.py')
          print stdout

      # Found a match, don't check the other base_names.
      return result, failures
  # If no base_names matched, fail script.
  return 66, failures


def check_linux_binary(target_dir, binary_name):
  """Collect appropriate size information about the built Linux binary given.

  Returns a tuple (result, failures).  result is the first non-zero exit
  status of any command it executes, or zero on success.  failures is a list
  of strings containing any error messages relating to failures of the checks.
  """
  binary_file = os.path.join(target_dir, binary_name)

  if not os.path.exists(binary_file):
    # Don't print anything for missing files.
    return 0, []

  result = 0
  failures = []

  def get_elf_section_size(readelf_stdout, section_name):
    # Matches: .ctors PROGBITS 000000000516add0 5169dd0 000010 00 WA 0 0 8
    match = re.search(r'\.%s.*$' % re.escape(section_name), readelf_stdout,
                      re.MULTILINE)
    if not match:
      return (False, -1)
    size_str = re.split(r'\W+', match.group(0))[5]
    return (True, int(size_str, 16))

  # Find the number of files with at least one static initializer.
  # First determine if we're 32 or 64 bit
  result, stdout = run_process(result, ['readelf', '-h', binary_file])
  elf_class_line = re.search('Class:.*$', stdout, re.MULTILINE).group(0)
  elf_class = re.split(r'\W+', elf_class_line)[1]
  if elf_class == 'ELF32':
    word_size = 4
  else:
    word_size = 8

  # Then find the number of files with global static initializers.
  # NOTE: this is very implementation-specific and makes assumptions
  # about how compiler and linker implement global static initializers.
  si_count = 0
  result, stdout = run_process(result, ['readelf', '-SW', binary_file])
  has_init_array, init_array_size = get_elf_section_size(stdout, 'init_array')
  if has_init_array:
    si_count = init_array_size / word_size
    # In newer versions of gcc crtbegin.o inserts frame_dummy into .init_array
    # but we don't want to count this entry, since its always present and not
    # related to our code.
    assert (si_count > 0)
    si_count -= 1

  if (si_count != EXPECTED_LINUX_SI_COUNTS[binary_name]):
    failures.append('%s contains %d static initializers (expected %d)' %
        (binary_name, si_count, EXPECTED_LINUX_SI_COUNTS[binary_name]))
    result = 125

  # Use dump-static-initializers.py to print the list of static initializers.
  if si_count > 0:
    build_dir = os.path.dirname(target_dir)
    dump_static_initializers = os.path.join(
        os.path.dirname(build_dir), 'tools', 'linux',
        'dump-static-initializers.py')
    result, stdout = run_process(result,
                                 [dump_static_initializers, '-d', binary_file])
    print '\n# Static initializers in %s:' % binary_file
    print_si_fail_hint('tools/linux/dump-static-initializers.py')
    print stdout

  # Determine if the binary has the DT_TEXTREL marker.
  result, stdout = run_process(result, ['readelf', '-Wd', binary_file])
  if re.search(r'\bTEXTREL\b', stdout) is None:
    # Nope, so the count is zero.
    count = 0
  else:
    # There are some, so count them.
    result, stdout = run_process(result, ['eu-findtextrel', binary_file])
    count = stdout.count('\n')
    failures.append('%s contains %d TEXTREL relocations (expected 0)' %
                    (binary_name, count))

  return result, failures


def main_linux(options):
  """Print appropriate size information about built Linux targets.

  Returns the first non-zero exit status of any command it executes,
  or zero on success.
  """
  build_dir = build_directory.GetBuildOutputDirectory(SRC_DIR)
  target_dir = os.path.join(build_dir, options.target)

  binaries = EXPECTED_LINUX_SI_COUNTS.keys()

  result = 0
  failures = []

  for binary in binaries:
    this_result, this_failures = check_linux_binary(target_dir, binary)
    if result == 0:
      result = this_result
    failures.extend(this_failures)

  return result, failures


def main():
  if sys.platform.startswith('darwin'):
    default_platform = 'mac'
  elif sys.platform.startswith('linux'):
    default_platform = 'linux'
  else:
    default_platform = None

  main_map = {
      'linux': main_linux,
      'mac': main_mac,
  }
  platforms = sorted(main_map.keys())

  option_parser = optparse.OptionParser()
  option_parser.add_option(
      '--target',
      default='Release',
      help='build target (Debug, Release) [default: %default]')
  option_parser.add_option(
      '--platform',
      default=default_platform,
      help='specify platform (%s) [default: %%default]' % ', '.join(platforms))
  option_parser.add_option('--json', help='Path to JSON output file')

  options, _ = option_parser.parse_args()

  real_main = main_map.get(options.platform)
  if not real_main:
    sys.stderr.write('Unsupported sys.platform %s.\n' % repr(sys.platform))
    msg = 'Use the --platform= option to specify a supported platform:\n'
    sys.stderr.write(msg + '    ' + ' '.join(platforms) + '\n')
    return 2

  rc, failures = real_main(options)

  if options.json:
    with open(options.json, 'w') as f:
      json.dump(failures, f)

  return rc


if '__main__' == __name__:
  sys.exit(main())
