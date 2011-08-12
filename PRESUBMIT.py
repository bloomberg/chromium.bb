# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Documentation on PRESUBMIT.py can be found at:
# http://www.chromium.org/developers/how-tos/depottools/presubmit-scripts

import os.path
import sys


TOP_DIR = 'native_client'
MAIN_DEPS = os.path.join(TOP_DIR, 'DEPS')

# List of directories to not apply presubmit project checks, relative
# to the NaCl top directory
EXCLUDE_PROJECT_CHECKS_DIRS = [
    # The following contain test data (including automatically generated),
    # and do not follow our conventions.
    'src/trusted/validator_x86/testdata/32',
    'src/trusted/validator_x86/testdata/64',
    # The following directories contains automatically generated source,
    # which may not follow our conventions.
    'src/trusted/validator_x86/gen',
    'src/trusted/validator/x86/decoder/gen',
    ]

def NaclTopDir():
  cwd = os.getcwd()
  pos = cwd.rfind(TOP_DIR)
  if pos < 0:
    print 'ERROR: expected to be called from with %s' % TOP_DIR
  return cwd[:pos + len(TOP_DIR)]

def _CommonChecks(input_api, output_api):
  """Checks for both upload and commit."""
  results = []
  results.extend(input_api.canned_checks.PanProjectChecks(
      input_api, output_api, project_name='Native Client',
      excluded_paths=tuple(EXCLUDE_PROJECT_CHECKS_DIRS)))
  return results

def IsFileInDirectories(f, dirs):
  """ Returns true if f is in list of directories"""
  for d in dirs:
    if d is os.path.commonprefix([f , d]):
      return True
  return False

def CheckChangeOnUpload(input_api, output_api):
  """Verifies all changes in all files.
  Args:
    input_api: the limited set of input modules allowed in presubmit.
    output_api: the limited set of output modules allowed in presubmit.
  """
  report = []
  report.extend(_CommonChecks(input_api, output_api))

  # The commit queue assumes PRESUBMIT.py is standalone.
  # TODO(bradnelson): Migrate code_hygiene to a common location so that
  # it can be used by the commit queue.
  old_sys_path = list(sys.path)
  try:
    sys.path.append(os.path.join(NaclTopDir(), 'tools'))
    sys.path.append(os.path.join(NaclTopDir(), 'build'))
    import code_hygiene
    import validate_chrome_revision
  finally:
    sys.path = old_sys_path
    del old_sys_path

  affected_files = input_api.AffectedFiles(include_deletes=False)
  exclude_dirs = [ NaclTopDir() + '/' + x + '/'
                   for x in EXCLUDE_PROJECT_CHECKS_DIRS ]
  for filename in affected_files:
    filename = filename.AbsoluteLocalPath()
    if not IsFileInDirectories(filename, exclude_dirs):
      errors, warnings = code_hygiene.CheckFile(filename, False)
      for e in errors:
        report.append(output_api.PresubmitError(e, items=errors[e]))
      for w in warnings:
        report.append(output_api.PresubmitPromptWarning(w, items=warnings[w]))
    if filename.endswith(MAIN_DEPS):
      try:
        e = validate_chrome_revision.ValidateChromeRevision(filename)
        if e is not None:
          report.append(output_api.PresubmitError(filename, items=[e]))
      except Exception, e:
        msg = "Could not validate Chrome revision: %s" % repr(e)
        report.append(output_api.PresubmitError(filename, items=[msg]))

  return report


def CheckChangeOnCommit(input_api, output_api):
  """Verifies all changes in all files and verifies that the
  tree is open and can accept a commit.
  Args:
    input_api: the limited set of input modules allowed in presubmit.
    output_api: the limited set of output modules allowed in presubmit.
  """
  report = []
  report.extend(CheckChangeOnUpload(input_api, output_api))
  report.extend(input_api.canned_checks.CheckTreeIsOpen(
      input_api, output_api,
      json_url='http://nativeclient-status.appspot.com/current?format=json'))
  return report


def GetPreferredTrySlaves():
  return [
      'nacl-lucid32_newlib_dbg',
      'nacl-lucid32_newlib_opt',
      'nacl-lucid32_glibc_opt',
      'nacl-lucid64_newlib_dbg',
      'nacl-lucid64_newlib_opt',
      'nacl-lucid64_glibc_opt',
      'nacl-lucid64-pnacl1',
      'nacl-lucid64-pnacl2',
      'nacl-lucid64_newlib_dbg_valgrind',
      'nacl-lucid64_glibc_dbg_valgrind',
      'nacl-mac10.5_newlib_opt',
      'nacl-mac10.5_glibc_opt',
      'nacl-mac10.6_newlib_opt',
      'nacl-mac10.6_glibc_opt',
      'nacl-win32_newlib_opt',
      'nacl-win32_glibc_opt',
      'nacl-win64_newlib_dbg',
      'nacl-win64_newlib_opt',
      'nacl-win64_glibc_opt',
      'nacl-arm_opt',
  ]
