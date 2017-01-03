#!/usr/bin/python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""api_static_checks_unittest.py - Unittests for api_static_checks.py"""


import contextlib
from cStringIO import StringIO
import os
import shutil
import sys
import tempfile
import unittest

from tools import api_static_checks


ERROR_PREFIX = (
"""ERROR: Found the following calls from implementation classes through
       API classes.  These could fail if older API is used that
       does not contain newer methods.  Please call through a
       wrapper class from VersionSafeCallbacks.
""")


@contextlib.contextmanager
def capture_output():
  # A contextmanger that collects the stdout and stderr of wrapped code

  oldout,olderr = sys.stdout, sys.stderr
  try:
    out=[StringIO(), StringIO()]
    sys.stdout,sys.stderr = out
    yield out
  finally:
    sys.stdout,sys.stderr = oldout, olderr
    out[0] = out[0].getvalue()
    out[1] = out[1].getvalue()


class ApiStaticCheckUnitTest(unittest.TestCase):
  def setUp(self):
    self.temp_dir = tempfile.mkdtemp()
    os.chdir(self.temp_dir)


  def tearDown(self):
    shutil.rmtree(self.temp_dir)


  def make_jar(self, java, class_name):
    # Compile |java| wrapped in a class named |class_name| to a jar file and
    # return jar filename.

    java_filename = class_name + '.java'
    class_filename = class_name + '.class'
    jar_filename = class_name + '.jar'

    with open(java_filename, 'w') as java_file:
      java_file.write('public class %s {' % class_name)
      java_file.write(java)
      java_file.write('}')
    os.system('javac %s' % java_filename)
    os.system('jar cf %s %s' % (jar_filename, class_filename))
    return jar_filename


  def run_test(self, api_java, impl_java):
    api_jar = self.make_jar(api_java, 'Api')
    impl_jar = self.make_jar(impl_java, 'Impl')
    with capture_output() as return_output:
      return_code = api_static_checks.main(
          ['--api_jar', api_jar, '--impl_jar', impl_jar])
    return [return_code, return_output[0]]


  def test_success(self):
    # Test simple classes with functions
    self.assertEqual(self.run_test('void a(){}', 'void b(){}'), [True, ''])
    # Test simple classes with functions calling themselves
    self.assertEqual(self.run_test(
        'void a(){} void b(){a();}', 'void c(){} void d(){c();}'), [True, ''])


  def test_failure(self):
    # Test static call
    self.assertEqual(self.run_test(
        'public static void a(){}', 'void b(){Api.a();}'),
        [False, ERROR_PREFIX + 'Impl/b -> Api/a:()V\n'])
    # Test virtual call
    self.assertEqual(self.run_test(
        'public void a(){}', 'void b(){new Api().a();}'),
        [False, ERROR_PREFIX + 'Impl/b -> Api/a:()V\n'])
