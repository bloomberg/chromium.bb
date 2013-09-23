# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from PIL import Image
import json
import os
import sys
import unittest

sys.path.append(os.path.join(os.path.dirname(__file__), os.pardir))
from tools import api_command_runner
from tools import image_tools
import auth_constants

class APICommandRunnerFunctionalTest(unittest.TestCase):
  def setUp(self):
    self.u = auth_constants.api_username
    self.p = auth_constants.api_password

  def Run(self, command, params, expected):
    res = api_command_runner.RunAPICommand(command, params, self.u, self.p)
    for item in expected.items():
      self.assertTrue(res.has_key(item[0]))
      self.assertEquals(res[item[0]], item[1])

  def testRunner(self):
    batch_test = {'batch_name': 'batch', 'test_name': 'test'}
    white = image_tools.SerializeImage(Image.new(
        'RGBA', (25, 25), (255, 255, 255, 255)))
    black = image_tools.SerializeImage(Image.new(
        'RGBA', (25, 25), (0, 0, 0, 255)))
    pink = image_tools.SerializeImage(Image.new(
        'RGBA', (25, 25), (255, 71, 191, 255)))
    test_set = json.dumps([white, white])
    # Removing a test.
    self.Run('remove_test', batch_test, {'success': True})
    # Uploading a test.
    self.Run('upload_test', dict(batch_test.items() + [('images', test_set)]),
             {'success': True})
    # Check that the test we uploaded is there.
    self.Run('test_exists', batch_test, {'success': True, 'exists': True})
    # Check that we can run the test we uploaded.
    self.Run('run_test', dict(batch_test.items() + [('image', black)]),
             {'success': True})
    # Check that the test we ran failed.
    self.Run('failure_exists', batch_test, {'success': True, 'exists': True})
    # Remove the failure.
    self.Run('remove_failure', batch_test, {'success': True})
    # Check that the failure was removed.
    self.Run('failure_exists', batch_test, {'success': True, 'exists': False})
    # Remove the test we added.
    self.Run('remove_test', batch_test, {'success': True})
    # Check that the test was removed.
    self.Run('test_exists', batch_test, {'success': True, 'exists': False})
    # Check that a bad command returns a failure.
    self.Run('bad_command', {}, {'success': False})
    # Check that missing parameters returns a failure.
    self.Run('run_test', batch_test, {'success': False})
    # Check that running a bad test returns a failure.
    self.Run('run_test', {'batch_name': 'batch', 'test_name': 'doesnt_exist',
                          'image': white},
             {'success': False})
    # Upload a test with the pinkout workaround (pink).
    self.Run('upload_test_pink_out',
             dict(batch_test.items() +
                  [('images', test_set), ('pink_out', pink),
                   ('RGB', json.dumps([255, 71, 191, 255]))]),
             {'success': True})
    # Check that the test is there.
    self.Run('test_exists', batch_test, {'success': True})
    # Run the test against a black image (traditionally fails).
    self.Run('run_test', dict(batch_test.items() + [('image', black)]),
             {'success': True})
    # Check that the failure exists.
    self.Run('failure_exists', batch_test, {'success': True, 'exists': True})
    # Remove the test.
    self.Run('remove_test', batch_test, {'success': True})
    # Upload a test with the pinkout workaround (not pink).
    self.Run('upload_test_pink_out',
             dict(batch_test.items() +
                  [('images', test_set), ('pink_out', black),
                   ('RGB', json.dumps([255, 71, 191, 255]))]),
             {'success': True})
    # Check that the test exists.
    self.Run('test_exists', batch_test, {'success': True})
    # Run the test against a black image (traditionally fails).
    self.Run('run_test', dict(batch_test.items() + [('image', black)]),
             {'success': True})
    # Check that a failure doesn't exist.
    self.Run('failure_exists', batch_test, {'success': True, 'exists': False})
    # Remove the test.
    self.Run('remove_test', batch_test, {'success': True})
    # Check that hte test doesn't exist.
    self.Run('test_exists', batch_test, {'success': True, 'exists': False})
    # Add a test.
    self.Run('upload_test', dict(batch_test.items() + [('images', test_set)]),
             {'success': True})
    # At this point the mask is black, add a white image to the mask.
    self.Run('add_to_test_mask', dict(batch_test.items() + [('mask', white)]),
             {'success': True})
    # Run the test against a black image.
    self.Run('run_test', dict(batch_test.items() + [('image', black)]),
             {'success': True})
    # Check that a failure doesn't exist (because of the added mask).
    self.Run('failure_exists', batch_test, {'success': True, 'exists': False})
    # Remove the test.
    self.Run('remove_test', batch_test, {'success': True})

if __name__ == '__main__':
  unittest.main()
