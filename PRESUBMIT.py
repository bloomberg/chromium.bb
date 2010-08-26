# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for Chromium.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into gcl.
"""

_EXCLUDED_PATHS = (
    r"breakpad[\\\/].*",
    r"net/tools/spdyshark/[\\\/].*",
    r"skia[\\\/].*",
    r"v8[\\\/].*",
)

_TEXT_FILES = (
    r".*\.txt",
    r".*\.json",
)

_LICENSE_HEADER = (
     r".*? Copyright \(c\) 20[0-9\-]{2,7} The Chromium Authors\. All rights "
       r"reserved\." "\n"
     r".*? Use of this source code is governed by a BSD-style license that can "
       "be\n"
     r".*? found in the LICENSE file\."
       "\n"
)


def _CheckConstNSObject(input_api, output_api, source_file_filter):
  """Checks to make sure no objective-c files have |const NSSomeClass*|."""
  pattern = input_api.re.compile(r'const\s+NS\w*\s*\*')
  files = []
  for f in input_api.AffectedSourceFiles(source_file_filter):
    if f.LocalPath().endswith('.h') or f.LocalPath().endswith('.mm'):
      contents = input_api.ReadFile(f)
      if pattern.search(contents):
        files.append(f)

  if len(files):
    if input_api.is_committing:
      res_type = output_api.PresubmitPromptWarning
    else:
      res_type = output_api.PresubmitNotifyResult
    return [ res_type('|const NSClass*| is wrong, see ' +
                      'http://dev.chromium.org/developers/clang-mac',
                      files) ]
  return []


def _CommonChecks(input_api, output_api):
  results = []
  # What does this code do?
  # It loads the default black list (e.g. third_party, experimental, etc) and
  # add our black list (breakpad, skia and v8 are still not following
  # google style and are not really living this repository).
  # See presubmit_support.py InputApi.FilterSourceFile for the (simple) usage.
  black_list = input_api.DEFAULT_BLACK_LIST + _EXCLUDED_PATHS
  white_list = input_api.DEFAULT_WHITE_LIST + _TEXT_FILES
  sources = lambda x: input_api.FilterSourceFile(x, black_list=black_list)
  text_files = lambda x: input_api.FilterSourceFile(x, black_list=black_list,
                                                    white_list=white_list)
  results.extend(input_api.canned_checks.CheckLongLines(
      input_api, output_api, source_file_filter=sources))
  results.extend(input_api.canned_checks.CheckChangeHasNoTabs(
      input_api, output_api, source_file_filter=sources))
  results.extend(input_api.canned_checks.CheckChangeHasNoStrayWhitespace(
      input_api, output_api, source_file_filter=sources))
  results.extend(input_api.canned_checks.CheckChangeHasBugField(
      input_api, output_api))
  results.extend(input_api.canned_checks.CheckChangeHasTestField(
      input_api, output_api))
  results.extend(input_api.canned_checks.CheckChangeSvnEolStyle(
      input_api, output_api, source_file_filter=text_files))
  results.extend(input_api.canned_checks.CheckSvnForCommonMimeTypes(
      input_api, output_api))
  results.extend(input_api.canned_checks.CheckLicense(
      input_api, output_api, _LICENSE_HEADER, source_file_filter=sources))
  results.extend(_CheckConstNSObject(
      input_api, output_api, source_file_filter=sources))
  return results


def CheckChangeOnUpload(input_api, output_api):
  results = []
  results.extend(_CommonChecks(input_api, output_api))
  return results


def CheckChangeOnCommit(input_api, output_api):
  results = []
  if not input_api.json:
    results.append(output_api.PresubmitNotifyResult(
        'You don\'t have json nor simplejson installed.\n'
        '  This is a warning that you will need to upgrade your python '
        'installation.\n'
        '  This is no big deal but you\'ll eventually need to '
        'upgrade.\n'
        '  How? Easy! You can do it right now and shut me off! Just:\n'
        '    del depot_tools\\python.bat\n'
        '    gclient\n'
        '  Thanks for your patience.'))
  results.extend(_CommonChecks(input_api, output_api))
  # TODO(thestig) temporarily disabled, doesn't work in third_party/
  #results.extend(input_api.canned_checks.CheckSvnModifiedDirectories(
  #    input_api, output_api, sources))
  # Make sure the tree is 'open'.
  results.extend(input_api.canned_checks.CheckTreeIsOpen(
      input_api,
      output_api,
      json_url='http://chromium-status.appspot.com/current?format=json'))
  results.extend(input_api.canned_checks.CheckRietveldTryJobExecution(input_api,
      output_api, 'http://codereview.chromium.org', ('win', 'linux', 'mac'),
      'tryserver@chromium.org'))

  # These builders are just too slow.
  IGNORED_BUILDERS = [
    'Chromium XP',
    'Chromium Mac',
    'Chromium Arm (dbg)',
    'Chromium Linux',
    'Chromium Linux x64',
  ]
  results.extend(input_api.canned_checks.CheckBuildbotPendingBuilds(
      input_api,
      output_api,
      'http://build.chromium.org/buildbot/waterfall/json/builders?filter=1',
      6,
      IGNORED_BUILDERS))
  return results


def GetPreferredTrySlaves():
  return ['win', 'linux', 'mac']
