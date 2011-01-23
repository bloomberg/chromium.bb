# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for Chromium.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into gcl.
"""

_EXCLUDED_PATHS = (
    r"^breakpad[\\\/].*",
    r"^net/tools/spdyshark/[\\\/].*",
    r"^skia[\\\/].*",
    r"^v8[\\\/].*",
    r".*MakeFile$",
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

def _CheckNoInterfacesInBase(input_api, output_api, source_file_filter):
  """Checks to make sure no files in libbase.a have |@interface|."""
  pattern = input_api.re.compile(r'@interface')
  files = []
  for f in input_api.AffectedSourceFiles(source_file_filter):
    if (f.LocalPath().find('base/') != -1 and
        f.LocalPath().find('base/test/') == -1):
      contents = input_api.ReadFile(f)
      if pattern.search(contents):
        files.append(f)

  if len(files):
    return [ output_api.PresubmitError(
        'Objective-C interfaces or categories are forbidden in libbase. ' +
        'See http://groups.google.com/a/chromium.org/group/chromium-dev/' +
        'browse_thread/thread/efb28c10435987fd',
        files) ]
  return []

def _CheckSingletonInHeaders(input_api, output_api, source_file_filter):
  """Checks to make sure no header files have |Singleton<|."""
  pattern = input_api.re.compile(r'Singleton<')
  files = []
  for f in input_api.AffectedSourceFiles(source_file_filter):
    if (f.LocalPath().endswith('.h') or f.LocalPath().endswith('.hxx') or
        f.LocalPath().endswith('.hpp') or f.LocalPath().endswith('.inl')):
      contents = input_api.ReadFile(f)
      if pattern.search(contents):
        files.append(f)

  if len(files):
    return [ output_api.PresubmitError(
        'Found Singleton<T> in the following header files.\n' +
        'Please move them to an appropriate source file so that the ' +
        'template gets instantiated in a single compilation unit.',
        files) ]
  return []


def _CheckSubversionConfig(input_api, output_api):
  """Verifies the subversion config file is correctly setup.

  Checks that autoprops are enabled, returns an error otherwise.
  """
  join = input_api.os_path.join
  if input_api.platform == 'win32':
    appdata = input_api.environ.get('APPDATA', '')
    if not appdata:
      return [output_api.PresubmitError('%APPDATA% is not configured.')]
    path = join(appdata, 'Subversion', 'config')
  else:
    home = input_api.environ.get('HOME', '')
    if not home:
      return [output_api.PresubmitError('$HOME is not configured.')]
    path = join(home, '.subversion', 'config')

  error_msg = (
      'Please look at http://dev.chromium.org/developers/coding-style to\n'
      'configure your subversion configuration file. This enables automatic\n'
      'properties to simplify the project maintenance.')

  try:
    lines = open(path, 'r').read().splitlines()
    # Make sure auto-props is enabled and check for 2 Chromium standard
    # auto-prop.
    if (not '*.cc = svn:eol-style=LF' in lines or
        not '*.pdf = svn:mime-type=application/pdf' in lines or
        not 'enable-auto-props = yes' in lines):
      return [
          output_api.PresubmitError(
              'It looks like you have not configured your subversion config '
              'file.\n' + error_msg)
      ]
  except (OSError, IOError):
    return [
        output_api.PresubmitError(
            'Can\'t find your subversion config file.\n' + error_msg)
    ]
  return []


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
  results.extend(input_api.canned_checks.CheckChangeSvnEolStyle(
      input_api, output_api, source_file_filter=text_files))
  results.extend(input_api.canned_checks.CheckSvnForCommonMimeTypes(
      input_api, output_api))
  results.extend(input_api.canned_checks.CheckLicense(
      input_api, output_api, _LICENSE_HEADER, source_file_filter=sources))
  results.extend(_CheckConstNSObject(
      input_api, output_api, source_file_filter=sources))
  results.extend(_CheckSingletonInHeaders(
      input_api, output_api, source_file_filter=sources))
  results.extend(_CheckNoInterfacesInBase(
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
      'http://build.chromium.org/p/chromium/json/builders?filter=1',
      6,
      IGNORED_BUILDERS))
  results.extend(input_api.canned_checks.CheckChangeHasBugField(
      input_api, output_api))
  results.extend(input_api.canned_checks.CheckChangeHasTestField(
      input_api, output_api))
  results.extend(_CheckSubversionConfig(input_api, output_api))
  return results


def GetPreferredTrySlaves():
  return ['win', 'linux', 'mac']
