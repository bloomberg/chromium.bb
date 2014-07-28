# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for Chromium JS resources.

See chrome/browser/PRESUBMIT.py
"""

import regex_check


class JSChecker(object):
  def __init__(self, input_api, output_api, file_filter=None):
    self.input_api = input_api
    self.output_api = output_api
    self.file_filter = file_filter

  def RegexCheck(self, line_number, line, regex, message):
    return regex_check.RegexCheck(
        self.input_api.re, line_number, line, regex, message)

  def ChromeSendCheck(self, i, line):
    """Checks for a particular misuse of 'chrome.send'."""
    return self.RegexCheck(i, line, r"chrome\.send\('[^']+'\s*(, \[\])\)",
        'Passing an empty array to chrome.send is unnecessary')

  def ConstCheck(self, i, line):
    """Check for use of the 'const' keyword."""
    if self.input_api.re.search(r'\*\s+@const', line):
      # Probably a JsDoc line
      return ''

    return self.RegexCheck(i, line, r'(?:^|\s|\()(const)\s',
        'Use /** @const */ var varName; instead of const varName;')

  def EndJsDocCommentCheck(self, i, line):
    msg = 'End JSDoc comments with */ instead of **/'
    def _check(regex):
      return self.RegexCheck(i, line, regex, msg)
    return _check(r'^\s*(\*\*/)\s*$') or _check(r'/\*\* @[a-zA-Z]+.* (\*\*/)')

  def GetElementByIdCheck(self, i, line):
    """Checks for use of 'document.getElementById' instead of '$'."""
    return self.RegexCheck(i, line, r"(document\.getElementById)\('",
        "Use $('id'), from chrome://resources/js/util.js, instead of "
        "document.getElementById('id')")

  def InheritDocCheck(self, i, line):
    """Checks for use of '@inheritDoc' instead of '@override'."""
    return self.RegexCheck(i, line, r"\* (@inheritDoc)",
        "@inheritDoc is deprecated, use @override instead")

  def WrapperTypeCheck(self, i, line):
    """Check for wrappers (new String()) instead of builtins (string)."""
    return self.RegexCheck(i, line,
        r"(?:/\*)?\*.*?@(?:param|return|type) ?"     # /** @param/@return/@type
        r"{[^}]*\b(String|Boolean|Number)\b[^}]*}",  # {(Boolean|Number|String)}
        "Don't use wrapper types (i.e. new String() or @type {String})")

  def VarNameCheck(self, i, line):
    """See the style guide. http://goo.gl/uKir6"""
    return self.RegexCheck(i, line,
        r"var (?!g_\w+)([a-z]*[_$][\w_$]*)(?<! \$)",
        "Please use var namesLikeThis <http://goo.gl/uKir6>")

  def _GetErrorHighlight(self, start, length):
    """Takes a start position and a length, and produces a row of '^'s to
       highlight the corresponding part of a string.
    """
    return start * ' ' + length * '^'

  def _MakeErrorOrWarning(self, error_text, filename):
    """Takes a few lines of text indicating a style violation and turns it into
       a PresubmitError (if |filename| is in a directory where we've already
       taken out all the style guide violations) or a PresubmitPromptWarning
       (if it's in a directory where we haven't done that yet).
    """
    # TODO(tbreisacher): Once we've cleaned up the style nits in all of
    # resources/ we can get rid of this function.
    path = self.input_api.os_path
    resources = path.join(self.input_api.PresubmitLocalPath(), 'resources')
    dirs = (
        path.join(resources, 'bookmark_manager'),
        path.join(resources, 'extensions'),
        path.join(resources, 'file_manager'),
        path.join(resources, 'help'),
        path.join(resources, 'history'),
        path.join(resources, 'memory_internals'),
        path.join(resources, 'net_export'),
        path.join(resources, 'net_internals'),
        path.join(resources, 'network_action_predictor'),
        path.join(resources, 'ntp4'),
        path.join(resources, 'options'),
        path.join(resources, 'password_manager_internals'),
        path.join(resources, 'print_preview'),
        path.join(resources, 'profiler'),
        path.join(resources, 'sync_promo'),
        path.join(resources, 'tracing'),
        path.join(resources, 'uber'),
    )
    if filename.startswith(dirs):
      return self.output_api.PresubmitError(error_text)
    else:
      return self.output_api.PresubmitPromptWarning(error_text)

  def ClosureLint(self, file_to_lint, source=None):
    """Lints |file_to_lint| and returns the errors."""

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

      from closure_linter import errors, runner
      from closure_linter.common import errorhandler

    finally:
      sys.path = old_path
      warnings.filters = old_filters

    class ErrorHandlerImpl(errorhandler.ErrorHandler):
      """Filters out errors that don't apply to Chromium JavaScript code."""

      def __init__(self, re):
        self._errors = []
        self.re = re

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

        is_grit_statement = bool(
            self.re.search("</?(include|if)", error.token.line))

        # Ignore missing spaces before "(" until Promise#catch issue is solved.
        # http://crbug.com/338301
        if (error.code == errors.MISSING_SPACE and error.token.string == '(' and
           'catch(' in error.token.line):
          return False

        # Ignore "}.bind(" errors. http://crbug.com/397697
        if (error.code == errors.MISSING_SEMICOLON_AFTER_FUNCTION and
            '}.bind(' in error.token.line):
          return False

        return not is_grit_statement and error.code not in [
            errors.COMMA_AT_END_OF_LITERAL,
            errors.JSDOC_ILLEGAL_QUESTION_WITH_PIPE,
            errors.LINE_TOO_LONG,
            errors.MISSING_JSDOC_TAG_THIS,
        ]

    error_handler = ErrorHandlerImpl(self.input_api.re)
    runner.Run(file_to_lint, error_handler, source=source)
    return error_handler.GetErrors()

  def RunChecks(self):
    """Check for violations of the Chromium JavaScript style guide. See
       http://chromium.org/developers/web-development-style-guide#TOC-JavaScript
    """
    results = []

    affected_files = self.input_api.change.AffectedFiles(
        file_filter=self.file_filter,
        include_deletes=False)
    affected_js_files = filter(lambda f: f.LocalPath().endswith('.js'),
                               affected_files)
    for f in affected_js_files:
      error_lines = []

      # Check for the following:
      # * document.getElementById()
      # * the 'const' keyword
      # * Passing an empty array to 'chrome.send()'
      for i, line in enumerate(f.NewContents(), start=1):
        error_lines += filter(None, [
            self.ChromeSendCheck(i, line),
            self.ConstCheck(i, line),
            self.GetElementByIdCheck(i, line),
            self.InheritDocCheck(i, line),
            self.WrapperTypeCheck(i, line),
            self.VarNameCheck(i, line),
        ])

      # Use closure linter to check for several different errors.
      lint_errors = self.ClosureLint(self.input_api.os_path.join(
          self.input_api.change.RepositoryRoot(), f.LocalPath()))

      for error in lint_errors:
        highlight = self._GetErrorHighlight(
            error.token.start_index, error.token.length)
        error_msg = '  line %d: E%04d: %s\n%s\n%s' % (
            error.token.line_number,
            error.code,
            error.message,
            error.token.line.rstrip(),
            highlight)
        error_lines.append(error_msg)

      if error_lines:
        error_lines = [
            'Found JavaScript style violations in %s:' %
            f.LocalPath()] + error_lines
        results.append(self._MakeErrorOrWarning(
            '\n'.join(error_lines), f.AbsoluteLocalPath()))

    if results:
      results.append(self.output_api.PresubmitNotifyResult(
          'See the JavaScript style guide at '
          'http://www.chromium.org/developers/web-development-style-guide'
          '#TOC-JavaScript and if you have any feedback about the JavaScript '
          'PRESUBMIT check, contact tbreisacher@chromium.org or '
          'dbeam@chromium.org'))

    return results
