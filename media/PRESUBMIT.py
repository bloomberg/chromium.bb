# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for Chromium media component.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into gcl.
"""

def _FilterFile(affected_file):
  """Return true if the file could contain code requiring a presubmit check."""
  return affected_file.LocalPath().endswith(
      ('.h', '.cc', '.cpp', '.cxx', '.mm'))


def _CheckForUseOfWrongClock(input_api, output_api):
  """Make sure new lines of media code don't use a clock susceptible to skew."""

  # Regular expression that should detect any explicit references to the
  # base::Time type (or base::Clock/DefaultClock), whether in using decls,
  # typedefs, or to call static methods.
  base_time_type_pattern = r'(^|\W)base::(Time|Clock|DefaultClock)(\W|$)'

  # Regular expression that should detect references to the base::Time class
  # members, such as a call to base::Time::Now.
  base_time_member_pattern = r'(^|\W)(Time|Clock|DefaultClock)::'

  # Regular expression to detect "using base::Time" declarations.  We want to
  # prevent these from triggerring a warning.  For example, it's perfectly
  # reasonable for code to be written like this:
  #
  #   using base::Time;
  #   ...
  #   int64 foo_us = foo_s * Time::kMicrosecondsPerSecond;
  using_base_time_decl_pattern = r'^\s*using\s+(::)?base::Time\s*;'

  # Regular expression to detect references to the kXXX constants in the
  # base::Time class.  We want to prevent these from triggerring a warning.
  base_time_konstant_pattern = r'(^|\W)Time::k\w+'

  problem_re = input_api.re.compile(
      r'(' + base_time_type_pattern + r')|(' + base_time_member_pattern + r')')
  exception_re = input_api.re.compile(
      r'(' + using_base_time_decl_pattern + r')|(' +
      base_time_konstant_pattern + r')')
  problems = []
  for f in input_api.AffectedSourceFiles(_FilterFile):
    for line_number, line in f.ChangedContents():
      if problem_re.search(line):
        if not exception_re.search(line):
          problems.append(
              '  %s:%d\n    %s' % (f.LocalPath(), line_number, line.strip()))

  if problems:
    return [output_api.PresubmitPromptOrNotify(
        'You added one or more references to the base::Time class and/or one\n'
        'of its member functions (or base::Clock/DefaultClock). In media\n'
        'code, it is rarely correct to use a clock susceptible to time skew!\n'
        'Instead, could you use base::TimeTicks to track the passage of\n'
        'real-world time?\n\n' +
        '\n'.join(problems))]
  else:
    return []


def _CheckForMessageLoopProxy(input_api, output_api):
  """Make sure media code only uses MessageLoopProxy for accessing the current
  loop."""

  message_loop_proxy_re = input_api.re.compile(
      r'\bMessageLoopProxy(?!::current\(\))')

  problems = []
  for f in input_api.AffectedSourceFiles(_FilterFile):
    for line_number, line in f.ChangedContents():
      if message_loop_proxy_re.search(line):
        problems.append('%s:%d' % (f.LocalPath(), line_number))

  if problems:
    return [output_api.PresubmitError(
      'MessageLoopProxy should only be used for accessing the current loop.\n'
      'Use the TaskRunner interfaces instead as they are more explicit about\n'
      'the run-time characteristics. In most cases, SingleThreadTaskRunner\n'
      'is a drop-in replacement for MessageLoopProxy.', problems)]

  return []


def _CheckChange(input_api, output_api):
  results = []
  results.extend(_CheckForUseOfWrongClock(input_api, output_api))
  results.extend(_CheckForMessageLoopProxy(input_api, output_api))
  return results


def CheckChangeOnUpload(input_api, output_api):
  return _CheckChange(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CheckChange(input_api, output_api)
