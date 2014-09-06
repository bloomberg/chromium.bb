# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Presubmit for Chromium HTML resources. See chrome/browser/PRESUBMIT.py.
"""

import regex_check


class HtmlChecker(object):
  def __init__(self, input_api, output_api, file_filter=None):
    self.input_api = input_api
    self.output_api = output_api
    self.file_filter = file_filter

  def LabelCheck(self, line_number, line):
    return regex_check.RegexCheck(self.input_api.re, line_number, line,
        "(for=)",
        "Avoid 'for' attribute on <label>. Place the input within the <label>, "
        "or use aria-labelledby for <select>.")

  def RunChecks(self):
    """Check for violations of the Chromium web development style guide. See
       http://chromium.org/developers/web-development-style-guide
    """
    results = []

    affected_files = self.input_api.change.AffectedFiles(
        file_filter=self.file_filter, include_deletes=False)

    for f in affected_files:
      errors = []

      for line_number, line in f.ChangedContents():
        error = self.LabelCheck(line_number, line)
        if error:
          errors.append(error)

      if errors:
        abs_local_path = f.AbsoluteLocalPath()
        file_indicator = 'Found HTML style issues in %s' % abs_local_path
        prompt_msg = file_indicator + '\n\n' + '\n'.join(errors) + '\n'
        results.append(self.output_api.PresubmitPromptWarning(prompt_msg))

    return results
