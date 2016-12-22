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

  def ExtraDotInGenericCheck(self, i, line):
    return self.RegexCheck(i, line, r"((?:Array|Object|Promise)\.<)",
        "Don't use a dot after generics (Object.<T> should be Object<T>).")

  def GetElementByIdCheck(self, i, line):
    """Checks for use of 'document.getElementById' instead of '$'."""
    return self.RegexCheck(i, line, r"(document\.getElementById)\('",
        "Use $('id') or getSVGElement('id') from chrome://resources/js/util.js "
        "instead of document.getElementById('id')")

  def InheritDocCheck(self, i, line):
    """Checks for use of '@inheritDoc' instead of '@override'."""
    return self.RegexCheck(i, line, r"\* (@inheritDoc)",
        "@inheritDoc is deprecated, use @override instead")

  def PolymerLocalIdCheck(self, i, line):
    """Checks for use of element.$.localId."""
    return self.RegexCheck(i, line, r"(?<!this)(\.\$)[\[\.]",
        "Please only use this.$.localId, not element.$.localId")

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
            self.EndJsDocCommentCheck(i, line),
            self.ExtraDotInGenericCheck(i, line),
            self.InheritDocCheck(i, line),
            self.PolymerLocalIdCheck(i, line),
            self.WrapperTypeCheck(i, line),
            self.VarNameCheck(i, line),
        ])

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
          '#TOC-JavaScript'))

    return results
