#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import glob
import logging
import os
import shutil
import tempfile

import pyauto_functional  # Must be imported before pyauto
import pyauto
import pyauto_errors


class WebIntentsTest(pyauto.PyUITest):
  """Tests web intents functionalities."""

  def testCreateWebIntentsPicker(self):
    """Tests that the web intent picker is launched successfully."""
    # Reads the suggested extensions for mocking out Chrome Web Store.
    file_name = 'picking_image_extensions.txt'
    file_path = os.path.join(self.DataDir(), 'intents', 'functional', file_name)
    extensions = self.EvalDataFrom(file_path)
    intent_info = {
        'action': 'http://webintents.org/pick',
        'data_type': 'image/*',
        'extensions': extensions,
    }
    # Lauches a web intent picker dialog.
    self.CreateWebIntentsPicker(**intent_info)
    # Reads the info from the picker and checks.
    picker_info = self.GetWebIntentsPickerInfo()
    installed_services = picker_info['installed_services']
    suggested_extensions = picker_info['suggested_extensions']
    self.assertEqual(len(installed_services), 0,
        msg='List of installed services should be empty.')
    self.assertEqual(len(suggested_extensions), len(extensions),
        msg='Suggested extensions list has incorrect size.')
    for actual, expected in zip(suggested_extensions, extensions):
      self.assertEqual(actual['id'], expected['id'],
          msg='Mismatch ID of suggested extension.')
      self.assertEqual(actual['title'], expected['name'],
          msg='Mismatched title of suggested extension.')
      self.assertEqual(actual['average_rating'], expected['average_rating'],
          msg='Mismatched rating of suggested extension.')
      self.assertTrue(actual['has_icon'],
          msg='Missing icon for suggested extension: %s' % actual['title'])

  def testErrorsWhenLaunchingWebIntentsPicker(self):
    """Tests error handling while creating a web intent picker."""
    # Creates a web intent picker without specifying data type
    self.assertRaises(pyauto_errors.JSONInterfaceError,
                      self.CreateWebIntentsPicker,
                      action='http://webintents.org/share',
                      data_type=None,
                      extensions=[])
    # Creates a web intent picker without specifying action
    self.assertRaises(pyauto_errors.JSONInterfaceError,
                      self.CreateWebIntentsPicker,
                      action=None,
                      data_type='text/*',
                      extensions=[])

  def testAccessingWebIntentsPickerBeforeLaunching(self):
    """Tests error handling when accessing the picker before it's created."""
    self.assertRaises(pyauto_errors.JSONInterfaceError,
                      self.GetWebIntentsPickerInfo)



if __name__ == '__main__':
  pyauto_functional.Main()
