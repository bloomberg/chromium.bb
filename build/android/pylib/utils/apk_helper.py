# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing utilities for apk packages."""

import os.path
import re

from pylib import cmd_helper
from pylib import constants


_AAPT_PATH = os.path.join(constants.ANDROID_SDK_TOOLS, 'aapt')
_MANIFEST_ATTRIBUTE_RE = re.compile(
    r'\s*A: ([^\(\)= ]*)\([^\(\)= ]*\)="(.*)" \(Raw: .*\)$')
_MANIFEST_ELEMENT_RE = re.compile(r'\s*(?:E|N): (\S*) .*$')


def GetPackageName(apk_path):
  """Returns the package name of the apk."""
  aapt_cmd = [_AAPT_PATH, 'dump', 'badging', apk_path]
  aapt_output = cmd_helper.GetCmdOutput(aapt_cmd).split('\n')
  package_name_re = re.compile(r'package: .*name=\'(\S*)\'')
  for line in aapt_output:
    m = package_name_re.match(line)
    if m:
      return m.group(1)
  raise Exception('Failed to determine package name of %s' % apk_path)


def _ParseManifestFromApk(apk_path):
  aapt_cmd = [_AAPT_PATH, 'dump', 'xmltree', apk_path, 'AndroidManifest.xml']
  aapt_output = cmd_helper.GetCmdOutput(aapt_cmd).split('\n')

  parsed_manifest = {}
  node_stack = [parsed_manifest]
  indent = '  '

  for line in aapt_output[1:]:
    if len(line) == 0:
      continue

    indent_depth = 0
    while line[(len(indent) * indent_depth):].startswith(indent):
      indent_depth += 1

    node_stack = node_stack[:indent_depth]
    node = node_stack[-1]

    m = _MANIFEST_ELEMENT_RE.match(line[len(indent) * indent_depth:])
    if m:
      if not m.group(1) in node:
        node[m.group(1)] = {}
      node_stack += [node[m.group(1)]]
      continue

    m = _MANIFEST_ATTRIBUTE_RE.match(line[len(indent) * indent_depth:])
    if m:
      if not m.group(1) in node:
        node[m.group(1)] = []
      node[m.group(1)].append(m.group(2))
      continue

  return parsed_manifest


def GetInstrumentationName(
    apk_path, default='android.test.InstrumentationTestRunner'):
  """Returns the name of the Instrumentation in the apk."""

  try:
    manifest_info = _ParseManifestFromApk(apk_path)
    return manifest_info['manifest']['instrumentation']['android:name'][0]
  except KeyError:
    return default
