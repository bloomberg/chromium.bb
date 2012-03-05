# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for Chromium JS resources.

See chrome/browser/resources/PRESUBMIT.py
"""

class JSChecker(object):
  def __init__(self, input_api, output_api, file_filter=None):
    self.input_api = input_api
    self.output_api = output_api
    self.file_filter = file_filter

  def RunChecks(self):
    """Check for violations of the Chromium JavaScript style guide. See
       http://chromium.org/developers/web-development-style-guide#TOC-JavaScript
    """

    import sys
    import warnings
    old_path = sys.path
    old_filters = warnings.filters

    try:
      closure_linter_path = self.input_api.os_path.join(
          self.input_api.change.RepositoryRoot(),
          "third_party",
          "closure_linter")
      gflags_path = self.input_api.os_path.join(
          self.input_api.change.RepositoryRoot(),
          "third_party",
          "python_gflags")

      sys.path.insert(0, closure_linter_path)
      sys.path.insert(0, gflags_path)

      warnings.filterwarnings('ignore', category=DeprecationWarning)

      from closure_linter import checker, errors
      from closure_linter.common import errorhandler

    finally:
      sys.path = old_path
      warnings.filters = old_filters

    class ErrorHandlerImpl(errorhandler.ErrorHandler):
      """Filters out errors that don't apply to Chromium JavaScript code."""

      def __init__(self):
        self._errors = []

      def HandleFile(self, filename, first_token):
        self._filename = filename

      def HandleError(self, error):
        if (self._valid(error)):
          error.filename = self._filename
          self._errors.append(error)

      def GetErrors(self):
        return self._errors

      def HasErrors(self):
        return bool(self._errors)

      def _valid(self, error):
        """Check whether an error is valid. Most errors are valid, with a few
           exceptions which are listed here.
        """

        return error.code not in [
            errors.COMMA_AT_END_OF_LITERAL,
            errors.JSDOC_ILLEGAL_QUESTION_WITH_PIPE,
            errors.JSDOC_TAG_DESCRIPTION_ENDS_WITH_INVALID_CHARACTER,
        ]

    results = []

    affected_files = self.input_api.change.AffectedFiles(
        file_filter=self.file_filter)
    affected_js_files = filter(lambda f: f.LocalPath().endswith('.js'),
                               affected_files)
    for f in affected_js_files:
      # Skip options_bundle.js since it's just a bunch of <include>s
      if self.input_api.os_path.split(f.LocalPath())[1] == 'options_bundle.js':
        continue

      error_lines = []

      # check for getElementById()
      for i, line in enumerate(f.NewContents(), start=1):
        if 'getElementById' in line:
          error_lines.append('  line %d: %s\n%s' % (
              i,
              "Use $('id') instead of document.getElementById('id')",
              line))

      # Use closure_linter to check for several different errors
      error_handler = ErrorHandlerImpl()
      js_checker = checker.JavaScriptStyleChecker(error_handler)
      js_checker.Check(self.input_api.os_path.join(
          self.input_api.change.RepositoryRoot(),
          f.LocalPath()))

      for error in error_handler.GetErrors():
        error_msg = '  line %d: E%04d: %s\n%s' % (
            error.token.line_number,
            error.code,
            error.message,
            error.token.line)
        error_lines.append(error_msg)

      if error_lines:
        error_lines = [
            'Found JavaScript style violations in %s:' %
            f.LocalPath()] + error_lines
        results.append(self.output_api.PresubmitError('\n'.join(error_lines)))

    if results:
      results.append(self.output_api.PresubmitNotifyResult(
          'See the JavaScript style guide at '
          'http://www.chromium.org/developers/web-development-style-guide'
          '#TOC-JavaScript and if you have any feedback about the JavaScript '
          'PRESUBMIT check, contact tbreisacher@chromium.org'))

    return results
