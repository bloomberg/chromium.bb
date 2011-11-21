#!/usr/bin/python2.6
#
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for nmf.py."""

import exceptions
import json
import optparse
import os
import subprocess
import sys
import tempfile
import unittest

from build_tools import build_utils
from build_tools.nacl_sdk_scons.site_tools import create_nmf

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
X86_32 = 'x86-32'
X86_64 = 'x86-64'
ARCHS = [X86_32, X86_64]


def CallCreateNmf(args):
  '''Calls the create_nmf.py utility and returns stdout as a string

  Args:
    args: command-line arguments as a list (not including program name)

  Returns:
    string containing stdout

  Raises:
    subprocess.CalledProcessError: non-zero return code from sdk_update'''
  command = [sys.executable, os.path.join(SCRIPT_DIR, 'site_tools',
                                          'create_nmf.py')] + args
  process = subprocess.Popen(stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE,
                             args=command)
  output, error_output = process.communicate()

  retcode = process.poll() # Note - calling wait() can cause a deadlock
  if retcode != 0:
    print "\nCallCreateNmf(%s)" % command
    print "stdout=%s\nstderr=%s" % (output, error_output)
    sys.stdout.flush()
    raise subprocess.CalledProcessError(retcode, command)
  return output


def GlobalInit(options, temp_files):
  '''Global initialization for testing nmf file creation.

  Args:
    options: object containing gcc and gpp defines
    files: (output) A list that will contain all generated nexes.  Each
        nexe is a value in a map, keyed by architecture.

  Returns:
    A list of filemaps, where each filemap is a dict with key=architecture
        and value=filename
  '''
  hello_world_c = (
      '#include <stdio.h>\n'
      'int main(void) { printf("Hello World!\\n"); return 0; }\n')
  hello_world_cc = (
      '#include <iostream>\n'
      'int main(void) { std::cout << "Hello World!\\n"; return 0; }\n')

  files = []
  def MakeNexe(contents, suffix, compiler):
    file_map = {}
    for arch in ARCHS:
      source_filename = None
      with tempfile.NamedTemporaryFile(delete=False,
                                       suffix=suffix) as temp_file:
        source_filename = temp_file.name
        temp_files.append(source_filename)
        temp_file.write(contents)
      nexe_filename = os.path.splitext(source_filename)[0] + '.nexe'
      file_map[arch] = nexe_filename
      temp_files.append(nexe_filename)
      subprocess.check_call([compiler,
                             '-m32' if arch == X86_32 else '-m64',
                             source_filename,
                             '-o', nexe_filename])
    files.append(file_map)

  c_file = MakeNexe(hello_world_c, '.c', options.gcc)
  cc_file = MakeNexe(hello_world_cc, '.cc', options.gpp)
  return files


def TestingClosure(toolchain_dir, file_map):
  '''Closure to provide variables to the test cases

  Args:
    toolchain_dir: path to toolchain that we are testing
    file_map: dict of nexe files by architecture.
  '''

  class TestNmf(unittest.TestCase):
    ''' Test basic functionality of the sdk_update package '''

    def setUp(self):
      self.objdump = (os.path.join(toolchain_dir,
                                   'bin',
                                   'x86_64-nacl-objdump'))
      self.library_paths = [os.path.join(toolchain_dir, 'x86_64-nacl', lib)
                            for lib in ['lib32', 'lib']]
      self.lib_options = ['--library-path=%s' % lib
                          for lib in self.library_paths]

    def testRunCreateNmf(self):
      json_text = CallCreateNmf(
          self.lib_options + ['--objdump', self.objdump] + file_map.values())
      obj = json.loads(json_text)
      # For now, just do a simple sanity check that there is a file
      # and program section.
      self.assertTrue(obj.get('files'))
      self.assertTrue(obj.get('program'))
      for arch in ['x86-32', 'x86-64']:
        self.assertEqual(obj['program'][arch]['url'],
                         '%s/runnable-ld.so' % arch)
        self.assertEqual(obj['files']['main.nexe'][arch]['url'],
                         os.path.basename(file_map[arch]))
        for filename, rest in obj['files'].items():
          if filename == 'main.nexe':
            continue
          self.assertEqual('/'.join([arch, filename]),
                           rest[arch]['url'])

    def testGenerateManifest(self):
      nmf = create_nmf.NmfUtils(
          objdump=self.objdump,
          main_files=file_map.values(),
          lib_path=self.library_paths)
      nmf_json = nmf.GetManifest()
      for arch in ['x86-32', 'x86-64']:
        self.assertEqual(nmf_json['program'][arch]['url'],
                         '%s/runnable-ld.so' % arch)
        self.assertEqual(nmf_json['files']['main.nexe'][arch]['url'],
                         os.path.basename(file_map[arch]))
        for filename, rest in nmf_json['files'].items():
          if filename == 'main.nexe':
            continue
          self.assertEqual('/'.join([arch, filename]),
                           rest[arch]['url'])

  return TestNmf


def GlobalTeardown(temp_files):
  '''Remove all the temporary files in temp_files'''
  for filename in temp_files:
    if os.path.exists(filename):
      os.remove(filename)


def main(argv):
  '''Usage: %prog [options]

  Runs the unit tests on the nmf utility'''
  parser = optparse.OptionParser(usage=main.__doc__)

  parser.add_option(
    '-t', '--toolchain-dir', dest='toolchain_dir',
    help='(required) root directory of toolchain')

  (options, args) = parser.parse_args(argv)

  options.gcc = os.path.join(options.toolchain_dir, 'bin', 'x86_64-nacl-gcc')
  options.gpp = os.path.join(options.toolchain_dir, 'bin', 'x86_64-nacl-g++')
  options.objdump = os.path.join(options.toolchain_dir, 'bin',
                                 'x86_64-nacl-objdump')

  success = True
  temp_files = []
  try:
    nexe_maps = GlobalInit(options, temp_files)
    for file_map in nexe_maps:
      suite = unittest.TestLoader().loadTestsFromTestCase(
          TestingClosure(toolchain_dir=options.toolchain_dir,
                         file_map=file_map))
      result = unittest.TextTestRunner(verbosity=2).run(suite)
      success = result.wasSuccessful() and success
  finally:
    GlobalTeardown(temp_files)

  return int(not success)  # 0 = success, 1 = failure

if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
