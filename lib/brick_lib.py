# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common brick related utilities."""

from __future__ import print_function

import os

from chromite.lib import osutils
from chromite.lib import workspace_lib

_DEFAULT_LAYOUT_CONF = {'profile_eapi_when_unspecified': '5-progress',
                        'profile-formats': 'portage-2 profile-default-eapi',
                        'thin-manifests': 'true',
                        'use-manifests': 'true'}

_CONFIG_FILE = 'config.json'

_IGNORED_OVERLAYS = ('portage-stable', 'chromiumos', 'eclass-overlay')


class BrickCreationFailed(Exception):
  """The brick creation failed."""


class BrickNotFound(Exception):
  """The brick does not exist."""


class BrickFeatureNotSupported(Exception):
  """Attempted feature not supported for this brick."""


class Brick(object):
  """Encapsulates the interaction with a brick."""

  def __init__(self, brick_loc, initial_config=None, allow_legacy=True):
    """Instantiates a brick object.

    Args:
      brick_loc: brick locator. This can be a relative path to CWD, an absolute
        path, a public board name prefix with 'board:' or a relative path to the
        root of the workspace, prefixed with '//').
      initial_config: The initial configuration as a python dictionary.
        If not None, creates a brick with this configuration.
      allow_legacy: Allow board overlays, simulating a basic read-only config.
        Ignored if |initial_config| is not None.

    Raises:
      ValueError: If |brick_loc| is invalid.
      LocatorNotResolved: |brick_loc| is valid but could not be resolved.
      BrickNotFound: If |brick_loc| does not point to a brick and no initial
        config was provided.
      BrickCreationFailed: when the brick could not be created successfully.
    """
    if workspace_lib.IsLocator(brick_loc):
      self.brick_dir = workspace_lib.LocatorToPath(brick_loc)
      self.brick_locator = brick_loc
    else:
      self.brick_dir = brick_loc
      self.brick_locator = workspace_lib.PathToLocator(brick_loc)

    self.config = None
    self.legacy = False
    config_json = os.path.join(self.brick_dir, _CONFIG_FILE)

    if not os.path.exists(config_json):
      if initial_config:
        if os.path.exists(self.brick_dir):
          raise BrickCreationFailed('directory %s already exists.'
                                    % self.brick_dir)
        success = False
        try:
          self.UpdateConfig(initial_config)
          osutils.SafeMakedirs(self.OverlayDir())
          osutils.SafeMakedirs(self.SourceDir())
          success = True
        except BrickNotFound as e:
          # If BrickNotFound was raised, the dependencies contain a missing
          # brick.
          raise BrickCreationFailed('dependency not found %s' % e)
        finally:
          if not success:
            # If the brick creation failed for any reason, cleanup the partially
            # created brick.
            osutils.RmDir(self.brick_dir, ignore_missing=True)

      elif allow_legacy:
        self.legacy = True
        try:
          masters = self._ReadLayoutConf().get('masters')
          masters_list = masters.split() if masters else []

          # Keep general Chromium OS overlays out of this list as they are
          # handled separately by the build system.
          deps = ['board:' + d for d in masters_list
                  if d not in _IGNORED_OVERLAYS]
          self.config = {'name': self._ReadLayoutConf()['repo-name'],
                         'dependencies': deps}
        except (IOError, KeyError):
          pass

      if self.config is None:
        raise BrickNotFound('Brick not found at %s' % self.brick_dir)
    elif initial_config is None:
      self.config = workspace_lib.ReadConfigFile(config_json)
    else:
      raise BrickCreationFailed('brick %s already exists.' % self.brick_dir)

    self.friendly_name = None
    if not self.legacy:
      self.friendly_name = workspace_lib.LocatorToFriendlyName(
          self.brick_locator)

  def _LayoutConfPath(self):
    """Returns the path to the layout.conf file."""
    return os.path.join(self.OverlayDir(), 'metadata', 'layout.conf')

  def _WriteLayoutConf(self, content):
    """Writes layout.conf.

    Sets unset fields to a sensible default and write |content| in layout.conf
    in the right format.

    Args:
      content: dictionary containing the set fields in layout.conf.
    """
    for k, v in _DEFAULT_LAYOUT_CONF.iteritems():
      content.setdefault(k, v)

    content_str = ''.join(['%s = %s\n' % (k, v)
                           for k, v in content.iteritems()])
    osutils.WriteFile(self._LayoutConfPath(), content_str, makedirs=True)

  def _ReadLayoutConf(self):
    """Returns the content of layout.conf as a Python dictionary."""
    def ParseConfLine(line):
      k, _, v = line.partition('=')
      return k.strip(), v.strip() or None

    content_str = osutils.ReadFile(self._LayoutConfPath())
    return dict(ParseConfLine(line) for line in content_str.splitlines())

  def UpdateConfig(self, config, regenerate=True):
    """Updates the brick's configuration.

    Writes |config| to the configuration file.
    If |regenerate| is true, regenerate the portage configuration files in
    this brick to match the new configuration.

    Args:
      config: brick configuration as a python dict.
      regenerate: if True, regenerate autogenerated brick files.
    """
    if self.legacy:
      raise BrickFeatureNotSupported(
          'Cannot update configuration of legacy brick %s' % self.brick_dir)

    self.config = config
    # All objects must be unambiguously referenced. Normalize all the
    # dependencies according to the workspace.
    self.config['dependencies'] = [d if workspace_lib.IsLocator(d)
                                   else workspace_lib.PathToLocator(d)
                                   for d in self.config.get('dependencies', [])]

    workspace_lib.WriteConfigFile(os.path.join(self.brick_dir, _CONFIG_FILE),
                                  config)

    if regenerate:
      self.GeneratePortageConfig()

  def GeneratePortageConfig(self):
    """Generates all autogenerated brick files."""
    # We don't generate anything in legacy brick so everything is up-to-date.
    if self.legacy:
      return

    deps = [b.config['name'] for b in self.Dependencies()]

    self._WriteLayoutConf(
        {'masters': ' '.join(
            ['eclass-overlay', 'portage-stable', 'chromiumos'] + deps),
         'repo-name': self.config['name']})

  def Dependencies(self):
    """Returns the dependent bricks."""
    return [Brick(d) for d in self.config.get('dependencies', [])]

  def Inherits(self, brick_name):
    """Checks whether this brick contains |brick_name|.

    Args:
      brick_name: The name of the brick to check containment.

    Returns:
      Whether |brick_name| is contained in this brick.
    """
    return brick_name in [b.config['name'] for b in self.BrickStack()]

  def MainPackages(self):
    """Returns the brick's main package(s).

    This finds the 'main_package' property.  It nevertheless returns a (single
    element) list as it is easier to work with.

    Returns:
      A list of main packages; empty if no main package configured.
    """
    main_package = self.config.get('main_package')
    return [main_package] if main_package else []

  def OverlayDir(self):
    """Returns the brick's overlay directory."""
    if self.legacy:
      return self.brick_dir

    return os.path.join(self.brick_dir, 'packages')

  def SourceDir(self):
    """Returns the project's source directory."""
    return os.path.join(self.brick_dir, 'src')

  def FriendlyName(self):
    """Return the friendly name for this brick.

    This name is used as the board name for legacy commands (--board).
    """
    if self.friendly_name is None:
      raise BrickFeatureNotSupported()
    return self.friendly_name

  def BrickStack(self):
    """Returns the brick stack for this brick.

    Returns:
      A list of bricks, respecting the partial ordering of bricks as defined by
      dependencies, ordered from the lowest priority to the highest priority.
    """
    seen = set()
    def _stack(brick):
      seen.add(brick.brick_dir)
      l = []
      for dep in brick.Dependencies():
        if dep.brick_dir not in seen:
          l.extend(_stack(dep))
      l.append(brick)
      return l

    return _stack(self)


def FindBrickInPath(path=None):
  """Returns the root directory of the brick containing a path.

  Return the first parent directory of |path| that is the root of a brick.
  This method is used for brick auto-detection and does not consider legacy.

  Args:
    path: path to a directory. If |path| is None, |path| will be set to CWD.

  Returns:
    The path to the first parent that is a brick directory if one exist.
    Otherwise return None.
  """
  for p in osutils.IteratePathParents(path or os.getcwd()):
    try:
      return Brick(p, allow_legacy=False)
    except BrickNotFound:
      pass

  return None
