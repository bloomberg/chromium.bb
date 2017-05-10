# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Get and parse options from CQ config files for changes."""

from __future__ import print_function

import ConfigParser
import os

from chromite.lib import config_lib
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import git


site_config = config_lib.GetConfig()


class MalformedCQConfigException(Exception):
  """Exception class presenting a malformed CQ config file."""

  def __init__(self, change, config_file, error):
    self.change = change
    self.config_file = config_file
    self.error = error
    super(MalformedCQConfigException, self).__init__(
        '%s has malformed config file %s: %s' % (
            self.change, self.config_file, self.error))


class CQConfigParser(object):
  """Class to parse options for a change from its CQ config files."""

  def __init__(self, build_root, change):
    """Initialize a CQConfigParser instance for a change.

    Args:
      build_root: The path to the build root.
      change: An instance of cros_patch.GerritPatch.
    """
    self.build_root = build_root
    self.change = change
    self._common_config_file = self.GetCommonConfigFileForChange(
        build_root, change)

  def GetOption(self, section, option, forgiven=True):
    """Get |option| from |section| for self.change.

    Args:
      section: Section header name (string).
      option: Option name (string).
      forgiven: Option boolean indicating whether a malformed config can be
        forgiven. Default to True.

    Returns:
      The value of the option (string) or None.

    Raises:
      MalformedCQConfigException if the config is malformed and forgiven is
      False.
    """
    result = None
    if self._common_config_file is not None:
      try:
        result = self._GetOptionFromConfigFile(
            self._common_config_file, section, option)
      except ConfigParser.Error as e:
        error = MalformedCQConfigException(
            self.change, self._common_config_file, e)
        logging.error('Malformed CQ config: %s', error)
        if not forgiven:
          raise error
    return result

  def GetPreCQConfigs(self):
    """Get a list of Pre-CQ configs from config for self.change.

    Retuns:
      A list of Pre-CQ configs (strings).
    """
    result = self.GetOption(constants.CQ_CONFIG_SECTION_GENERAL,
                            constants.CQ_CONFIG_PRE_CQ_CONFIGS)
    return result.split() if result else []

  def GetStagesToIgnore(self):
    """Get a list of stages that the CQ should ignore for self.change.

    The section and option in the config file COMMIT-QUEUE.ini would be like:

    [GENERAL]
      ignored-stages: HWTest VMTest

    The CQ will submit changes to the given project even if the listed stages
    failed. These strings are stage name prefixes, meaning that "HWTest" would
    match any HWTest stage (e.g. "HWTest [bvt]" or "HWTest [foo]")

    Returns:
      A list of stages (strings) to ignore for self.change.
    """
    result = self.GetOption(constants.CQ_CONFIG_SECTION_GENERAL,
                            constants.CQ_CONFIG_IGNORED_STAGES)
    return result.split() if result else []

  def GetSubsystems(self):
    """Get a list of subsystems from config for self.change.

    Retuns:
      A list of subsystems (strings).
    """
    result = self.GetOption(constants.CQ_CONFIG_SECTION_GENERAL,
                            constants.CQ_CONFIG_SUBSYSTEM)
    return result.split() if result else []

  @classmethod
  def _GetOptionFromConfigFile(cls, config_path, section, option):
    """Get |option| from |section| in |config_path|.

    Args:
      config_path: The path to the CQ config file.
      section: Section header name (string).
      option: Option name (string).

    Returns:
      The value (string) of the option.
    """
    parser = ConfigParser.SafeConfigParser()
    parser.read(config_path)
    if parser.has_option(section, option):
      return parser.get(section, option)

  @classmethod
  def _GetCommonAffectedSubdir(cls, change, git_repo):
    """Gets the longest common path of changes in |change|.

    Args:
      change: An instance of cros_patch.GerritPatch.
      git_repo: Path to checkout of git repository.

    Returns:
      An absolute path in |git_repo|.
    """
    affected_paths = [os.path.join(git_repo, path)
                      for path in change.GetDiffStatus(git_repo)]
    return cros_build_lib.GetCommonPathPrefix(affected_paths)

  @classmethod
  def GetCommonConfigFileForChange(cls, build_root, change):
    """Get the config file from the lowest common parent dir of the |change|.

    This will look for a config file from the common parent directory of all the
    changed files from |change|. If no config file is found in that directory,
    it will continue up the directory tree until it finds one. If no config file
    is found within the project checkout path, a config file path in the root of
    the checkout will be returned, in which case the file is not guaranteed to
    exist. See
    http://chromium.org/chromium-os/build/bypassing-tests-on-a-per-project-basis

    Args:
      build_root: The path to the build root.
      change: An instance of cros_patch.GerritPatch.

    Returns:
      Path to the config file to be read for |change|. If no config file is
      found within the project checkout, return a config file path in the root
      of the checkout.
    """
    manifest = git.ManifestCheckout.Cached(build_root)
    checkout = change.GetCheckout(manifest)
    if checkout:
      checkout_path = checkout.GetPath(absolute=True)
      current_dir = cls._GetCommonAffectedSubdir(change, checkout_path)
      while True:
        config_file = os.path.join(current_dir, constants.CQ_CONFIG_FILENAME)
        if os.path.isfile(config_file) or checkout_path.startswith(current_dir):
          return config_file
        assert current_dir not in ('/', '')
        current_dir = os.path.dirname(current_dir)
