#!/usr/bin/env python

# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Uses the closure compiler to check the braille ime.'''

import os
import re
import subprocess
import sys
import tempfile


# Compiler path, relative to Chromium repository root.
_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
_CLOSURE_COMPILER_JAR = os.path.normpath(
    os.path.join(
        _SCRIPT_DIR, '../../../../..',
        'third_party/WebKit/Source/devtools/scripts/closure/compiler.jar'))
# List of compilation errors to enable with the --jscomp_errors flag.
_JSCOMP_ERRORS = [ 'accessControls', 'checkTypes', 'checkVars', 'invalidCasts',
                   'missingProperties', 'undefinedNames', 'undefinedVars',
                   'visibility' ]

_java_executable = 'java'


def _error(msg):
  print >>sys.stderr, msg
  sys.exit(1)


def _execute_command(args, ignore_exit_status=False):
  try:
    return subprocess.check_output(args, stderr=subprocess.STDOUT)
  except subprocess.CalledProcessError as e:
    if ignore_exit_status and e.returncode > 0:
      return e.output
    _error('%s\nCommand \'%s\' returned non-zero exit status %d' %
           (e.output, ' '.join(e.cmd), e.returncode))
  except (OSError, IOError) as e:
    _error('Error executing %s: %s' % (_java_executable, str(e)))


def _check_java():
  global _java_executable
  java_home = os.environ.get('JAVAHOME')
  if java_home is not None:
    _java_executable = os.path.join(java_home, 'bin/java')
  output = _execute_command([_java_executable, '-version'])
  match = re.search(r'version "(?:\d+)\.(\d+)', output)
  if match is None or int(match.group(1)) < 7:
    _error('Java 7 or later is required: \n%s' % output)


def _run_closure_compiler():
  print 'Compiling Braille IME.'
  args = [_java_executable, '-jar', _CLOSURE_COMPILER_JAR]
  args.extend(['--compilation_level', 'SIMPLE_OPTIMIZATIONS'])
  args.extend(['--jscomp_error=%s' % error for error in _JSCOMP_ERRORS])
  args.extend(['--externs', 'externs.js'])
  args.extend(['--js', 'braille_ime.js'])
  args.extend(['--js', 'main.js'])
  args.extend(['--js_output_file', '/dev/null'])
  output = _execute_command(args, ignore_exit_status=True)
  success = len(output) == 0
  return success, output


def check_braille_ime():
  _check_java()
  return _run_closure_compiler()


def main(argv):
  success, output = check_braille_ime()
  print output
  return int(not success)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
