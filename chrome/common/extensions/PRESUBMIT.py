# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for changes affecting extensions.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into gcl.
"""
import fnmatch
import os
import re

EXTENSIONS_PATH = os.path.join('chrome', 'common', 'extensions')
SERVER2_PATH = os.path.join(EXTENSIONS_PATH, 'docs', 'server2')
API_PATH = os.path.join(EXTENSIONS_PATH, 'api')
TEMPLATES_PATH = os.path.join(SERVER2_PATH, 'templates')
PRIVATE_TEMPLATES_PATH = os.path.join(TEMPLATES_PATH, 'private')
PUBLIC_TEMPLATES_PATH = os.path.join(TEMPLATES_PATH, 'public')
INTROS_PATH = os.path.join(TEMPLATES_PATH, 'intros')
ARTICLES_PATH = os.path.join(TEMPLATES_PATH, 'articles')

LOCAL_PUBLIC_TEMPLATES_PATH = os.path.join('docs',
                                           'server2',
                                           'templates',
                                           'public')

def _ListFilesInPublic():
  all_files = []
  for path, dirs, files in os.walk(LOCAL_PUBLIC_TEMPLATES_PATH):
    all_files.extend(
        os.path.join(path, filename)[len(LOCAL_PUBLIC_TEMPLATES_PATH + os.sep):]
        for filename in files)
  return all_files

def _UnixName(name):
  name = os.path.splitext(name)[0]
  s1 = re.sub('([a-z])([A-Z])', r'\1_\2', name)
  s2 = re.sub('([A-Z]+)([A-Z][a-z])', r'\1_\2', s1)
  return s2.replace('.', '_').lower()

def _FindMatchingTemplates(template_name, template_path_list):
  matches = []
  unix_name = _UnixName(template_name)
  for template in template_path_list:
    if unix_name == _UnixName(template.split(os.sep)[-1]):
      matches.append(template)
  return matches

def _SanitizeAPIName(name, api_path):
  if not api_path.endswith(os.sep):
    api_path += os.sep
  filename = os.path.splitext(name)[0][len(api_path):].replace(os.sep, '_')
  if 'experimental' in filename:
    filename = 'experimental_' + filename.replace('experimental_', '')
  return filename

def _CreateIntegrationTestArgs(affected_files):
  if (any(fnmatch.fnmatch(name, '%s*.py' % SERVER2_PATH)
         for name in affected_files) or
      any(fnmatch.fnmatch(name, '%s*' % PRIVATE_TEMPLATES_PATH)
          for name in affected_files)):
    return ['-a']
  args = []
  for name in affected_files:
    if (fnmatch.fnmatch(name, '%s*' % PUBLIC_TEMPLATES_PATH) or
        fnmatch.fnmatch(name, '%s*' % INTROS_PATH) or
        fnmatch.fnmatch(name, '%s*' % ARTICLES_PATH)):
      args.extend(_FindMatchingTemplates(name.split(os.sep)[-1],
                                         _ListFilesInPublic()))
    if fnmatch.fnmatch(name, '%s*' % API_PATH):
      args.extend(_FindMatchingTemplates(_SanitizeAPIName(name, API_PATH),
                                         _ListFilesInPublic()))
  return args

def _CheckChange(input_api, output_api):
  results = []
  try:
    integration_test = []
    # From depot_tools/presubmit_canned_checks.py:529
    if input_api.platform == 'win32':
      integration_test = [input_api.python_executable]
    integration_test.append(
        os.path.join('docs', 'server2', 'integration_test.py'))
    integration_test.extend(_CreateIntegrationTestArgs(input_api.LocalPaths()))
    input_api.subprocess.check_call(integration_test,
                                    cwd=input_api.PresubmitLocalPath())
  except input_api.subprocess.CalledProcessError:
    results.append(output_api.PresubmitError('IntegrationTest failed!'))
  return results

def CheckChangeOnUpload(input_api, output_api):
  return _CheckChange(input_api, output_api)

def CheckChangeOnCommit(input_api, output_api):
  return _CheckChange(input_api, output_api)
