# Copyright (c) 2011 The Chromium Authors. All rights reserved.
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


def _CheckNoInterfacesInBase(input_api, output_api):
  """Checks to make sure no files in libbase.a have |@interface|."""
  pattern = input_api.re.compile(r'@interface')
  files = []
  for f in input_api.AffectedSourceFiles(input_api.FilterSourceFile):
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


def _CommonChecks(input_api, output_api):
  """Checks common to both upload and commit."""
  results = []
  results.extend(input_api.canned_checks.PanProjectChecks(
      input_api, output_api, excluded_paths=_EXCLUDED_PATHS))
  results.extend(_CheckNoInterfacesInBase(input_api, output_api))
  return results


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
      'properties to simplify the project maintenance.\n'
      'Pro-tip: just download and install\n'
      'http://src.chromium.org/viewvc/chrome/trunk/tools/build/slave/config\n')

  try:
    lines = open(path, 'r').read().splitlines()
    # Make sure auto-props is enabled and check for 2 Chromium standard
    # auto-prop.
    if (not '*.cc = svn:eol-style=LF' in lines or
        not '*.pdf = svn:mime-type=application/pdf' in lines or
        not 'enable-auto-props = yes' in lines):
      return [
          output_api.PresubmitNotifyResult(
              'It looks like you have not configured your subversion config '
              'file or it is not up-to-date.\n' + error_msg)
      ]
  except (OSError, IOError):
    return [
        output_api.PresubmitNotifyResult(
            'Can\'t find your subversion config file.\n' + error_msg)
    ]
  return []


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
