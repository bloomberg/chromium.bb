# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''
Utility functions for all things related to manipulating google play services
related files.
'''

import argparse
import filecmp
import logging
import os
import re
import sys
import yaml
import zipfile

sys.path.append(os.path.join(os.path.dirname(__file__), os.pardir))
from devil.utils import cmd_helper


_CONFIG_VERSION_NUMBER_KEY = 'version_number'
_YAML_VERSION_NUMBER_PATTERN = re.compile(
    r'(^\s*%s\s*:\s*)(\d+)(.*$)' % _CONFIG_VERSION_NUMBER_KEY, re.MULTILINE)
_XML_VERSION_NUMBER_PATTERN = re.compile(
    r'<integer name="google_play_services_version">(\d+)<\/integer>')


class DefaultsRawHelpFormatter(argparse.ArgumentDefaultsHelpFormatter,
                               argparse.RawDescriptionHelpFormatter):
  '''
  Combines the features of RawDescriptionHelpFormatter and
  ArgumentDefaultsHelpFormatter, providing defaults for the arguments and raw
  text for the description.
  '''
  pass


def FileEquals(expected_file, actual_file):
  '''
  Returns whether the two files are equal. Returns False if any of the files
  doesn't exist.
  '''

  if not os.path.isfile(actual_file) or not os.path.isfile(expected_file):
    return False
  return filecmp.cmp(expected_file, actual_file)


def IsRepoDirty(repo_root):
  '''Returns True if there are no staged or modified files, False otherwise.'''

  # diff-index returns 1 if there are staged changes or modified files,
  # 0 otherwise
  cmd = ['git', 'diff-index', '--quiet', 'HEAD']
  return cmd_helper.Call(cmd, cwd=repo_root) == 1


def GetVersionNumberFromLibraryResources(version_xml):
  '''
  Extracts a Google Play services version number from its version.xml file.
  '''

  with open(version_xml, 'r') as version_file:
    version_file_content = version_file.read()

  match = _XML_VERSION_NUMBER_PATTERN.search(version_file_content)
  if not match:
    raise AttributeError('A value for google_play_services_version was not '
                         'found in ' + version_xml)
  return int(match.group(1))


def UpdateVersionNumber(config_file_path, new_version_number):
  '''Updates the version number in the update/preprocess configuration file.'''

  with open(config_file_path, 'r+') as stream:
    config_content = stream.read()
    # Implemented as string replacement instead of yaml parsing to preserve
    # whitespace and comments.
    updated = _YAML_VERSION_NUMBER_PATTERN.sub(
        r'\g<1>%s\g<3>' % new_version_number, config_content)
    stream.seek(0)
    stream.write(updated)


def GetVersionNumber(config_file_path):
  '''
  Returns the version number from an update/preprocess configuration file.
  '''

  return int(GetConfig(config_file_path)[_CONFIG_VERSION_NUMBER_KEY])


def GetConfig(path):
  '''
  Returns the configuration from an an update/preprocess configuration file as
  as dictionary.
  '''

  with open(path, 'r') as stream:
    config = yaml.load(stream)
  return config


def MakeLocalCommit(repo_root, files_to_commit, message):
  '''Makes a local git commit.'''

  logging.debug('Staging files (%s) for commit.', files_to_commit)
  if cmd_helper.Call(['git', 'add'] + files_to_commit, cwd=repo_root) != 0:
    raise Exception('The local commit failed.')

  logging.debug('Committing.')
  if cmd_helper.Call(['git', 'commit', '-m', message], cwd=repo_root) != 0:
    raise Exception('The local commit failed.')


def ZipDir(output, base_dir):
  '''Creates a zip file from a directory.'''

  base = os.path.join(base_dir, os.pardir)
  with zipfile.ZipFile(output, 'w') as out_file:
    for root, _, files in os.walk(base_dir):
      for in_file in files:
        out_file.write(os.path.join(root, in_file), base)
