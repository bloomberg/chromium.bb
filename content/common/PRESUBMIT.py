# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for content/common."""

import os, re

AX_JS_FILE = 'content/browser/resources/accessibility/accessibility.js'
AX_MODE_HEADER = 'content/common/accessibility_mode.h'

# Given a full path to c++ header, return an array of the first static
# constexpr defined. (Note there can be more than one defined in a C++
# header)
def GetConstexprFromFile(fullpath):
  values = []
  for line in open(fullpath).readlines():
    # Strip out comments
    line = re.sub('//.*', '', line)

    # Look for lines of the form "static constexpr <type> NAME "
    m = re.search('static constexpr [\w]+ ([\w]+)', line)
    if m:
      values.append(m.group(1))

  return values

# Given a full path to js file, return the AccessibilityMode consts
# defined
def GetAccessibilityModesFromFile(fullpath):
  values = []
  inside = False
  for line in open(fullpath).readlines():
    # Strip out comments
    line = re.sub('//.*', '', line)

    # Look for the block of code that defines AccessibilityMode
    m = re.search('const AccessibilityMode = {', line)
    if m:
      inside = True
      continue

    # Look for a "}" character signifying the end of an enum
    if line.find('};') >= 0:
      return values

    if not inside:
      continue

    m = re.search('([\w]+):', line)
    if m:
      values.append(m.group(1))
      continue

    # getters
    m = re.search('get ([\w]+)\(\)', line)
    if m:
      values.append(m.group(1))
  return values

# Make sure that the modes defined in the C++ header match those defined in
# the js file. Note that this doesn't guarantee that the values are the same,
# but does make sure if we add or remove we can signal to the developer that
# they should be aware that this dependency exists.
def CheckModesMatch(input_api, output_api):
  errs = []
  repo_root = input_api.change.RepositoryRoot()

  ax_modes_in_header = GetConstexprFromFile(
    os.path.join(repo_root,AX_MODE_HEADER))
  ax_modes_in_js = GetAccessibilityModesFromFile(
    os.path.join(repo_root, AX_JS_FILE))

  for value in ax_modes_in_header:
    if value not in ax_modes_in_js:
      errs.append(output_api.PresubmitError(
          'Found %s in %s, but did not find %s in %s' % (
              value, AX_MODE_HEADER, value, AX_JS_FILE)))
  return errs

def CheckChangeOnUpload(input_api, output_api):
  if AX_MODE_HEADER not in input_api.LocalPaths():
    return []
  return CheckModesMatch(input_api, output_api)

def CheckChangeOnCommit(input_api, output_api):
  if AX_MODE_HEADER not in input_api.LocalPaths():
    return []
  return CheckModesMatch(input_api, output_api)
