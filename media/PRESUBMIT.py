# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for Chromium media component.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into gcl.
"""


def _CheckForUseOfWrongClock(input_api, output_api):
  """Make sure new lines of media code don't use a clock susceptible to skew."""

  def FilterFile(affected_file):
    """Return true if the file could contain code referencing base::Time."""
    return affected_file.LocalPath().endswith(
        ('.h', '.cc', '.cpp', '.cxx', '.mm'))

  # Regular expression that should detect any explicit references to the
  # base::Time type (or base::Clock/DefaultClock), whether in using decls,
  # typedefs, or to call static methods.
  base_time_type_pattern = r'base::(Time|Clock|DefaultClock)(\W|$)'

  # Regular expression that should detect references to the base::Time class
  # members, such as a call to base::Time::Now.  Exceptions: References to the
  # kXXX constants are ignored.
  base_time_member_pattern = r'(^|\W)(Time|Clock|DefaultClock)::[^k]'

  problem_re = input_api.re.compile(
      r'(' + base_time_type_pattern + r')|(' + base_time_member_pattern + r')')
  problems = []
  for f in input_api.AffectedSourceFiles(FilterFile):
    for line_number, line in f.ChangedContents():
      if problem_re.search(line):
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


def _CheckChange(input_api, output_api):
  results = []
  results.extend(_CheckForUseOfWrongClock(input_api, output_api))
  return results


def CheckChangeOnUpload(input_api, output_api):
  return _CheckChange(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CheckChange(input_api, output_api)
