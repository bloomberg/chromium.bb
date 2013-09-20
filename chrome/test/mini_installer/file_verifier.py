# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

import verifier


class FileVerifier(verifier.Verifier):
  """Verifies that the current files match the expectation dictionaries."""

  def Verify(self, files, variable_expander):
    """Overridden from verifier.Verifier.

    This method will throw an AssertionError if file state doesn't match the
    provided expectation.

    Args:
      files: A dictionary whose keys are file paths and values are expectation
          dictionaries. An expectation dictionary is a dictionary with the
          following key and value:
              'exists' a boolean indicating whether the file should exist.
      variable_expander: A VariableExpander object.
    """
    for file_path, expectation in files.iteritems():
      file_expanded_path = variable_expander.Expand(file_path)
      file_exists = os.path.exists(file_expanded_path)
      assert expectation['exists'] == file_exists, \
          ('File %s exists' % file_expanded_path) if file_exists else \
          ('File %s is missing' % file_expanded_path)
