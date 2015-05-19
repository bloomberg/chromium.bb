# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Configuration options for various cbuildbot builders."""

from __future__ import print_function

import copy

from chromite.cbuildbot import constants

GS_PATH_DEFAULT = 'default' # Means gs://chromeos-image-archive/ + bot_id

# Contains the valid build config suffixes in the order that they are dumped.
CONFIG_TYPE_PRECQ = 'pre-cq'
CONFIG_TYPE_PALADIN = 'paladin'
CONFIG_TYPE_RELEASE = 'release'
CONFIG_TYPE_FULL = 'full'
CONFIG_TYPE_FIRMWARE = 'firmware'
CONFIG_TYPE_FACTORY = 'factory'
CONFIG_TYPE_RELEASE_AFDO = 'release-afdo'

CONFIG_TYPE_DUMP_ORDER = (
    CONFIG_TYPE_PALADIN,
    constants.PRE_CQ_GROUP_CONFIG,
    CONFIG_TYPE_PRECQ,
    constants.PRE_CQ_LAUNCHER_CONFIG,
    'incremental',
    'telemetry',
    CONFIG_TYPE_FULL,
    'full-group',
    CONFIG_TYPE_RELEASE,
    'release-group',
    'release-afdo',
    'release-afdo-generate',
    'release-afdo-use',
    'sdk',
    'chromium-pfq',
    'chromium-pfq-informational',
    'chrome-perf',
    'chrome-pfq',
    'chrome-pfq-informational',
    'pre-flight-branch',
    CONFIG_TYPE_FACTORY,
    CONFIG_TYPE_FIRMWARE,
    'toolchain-major',
    'toolchain-minor',
    'asan',
    'asan-informational',
    'refresh-packages',
    'test-ap',
    'test-ap-group',
    constants.BRANCH_UTIL_CONFIG,
    constants.PAYLOADS_TYPE,
)

def IsPFQType(b_type):
  """Returns True if this build type is a PFQ."""
  return b_type in (constants.PFQ_TYPE, constants.PALADIN_TYPE,
                    constants.CHROME_PFQ_TYPE)


def IsCQType(b_type):
  """Returns True if this build type is a Commit Queue."""
  return b_type == constants.PALADIN_TYPE


def IsCanaryType(b_type):
  """Returns True if this build type is a Canary."""
  return b_type == constants.CANARY_TYPE


class BuildConfig(dict):
  """Dictionary of explicit configuration settings for a cbuildbot config

  Each dictionary entry is in turn a dictionary of config_param->value.

  See _settings for details on known configurations, and their documentation.
  """

  _delete_key_sentinel = object()

  @classmethod
  def delete_key(cls):
    """Used to remove the given key from inherited config.

    Usage:
      new_config = base_config.derive(foo=delete_key())
    """
    return cls._delete_key_sentinel

  @classmethod
  def delete_keys(cls, keys):
    """Used to remove a set of keys from inherited config.

    Usage:
      new_config = base_config.derive(delete_keys(set_of_keys))
    """
    return {k: cls._delete_key_sentinel for k in keys}

  def __getattr__(self, name):
    """Support attribute-like access to each dict entry."""
    if name in self:
      return self[name]

    # Super class (dict) has no __getattr__ method, so use __getattribute__.
    return super(BuildConfig, self).__getattribute__(name)

  def GetBotId(self, remote_trybot=False):
    """Get the 'bot id' of a particular bot.

    The bot id is used to specify the subdirectory where artifacts are stored
    in Google Storage. To avoid conflicts between remote trybots and regular
    bots, we add a 'trybot-' prefix to any remote trybot runs.

    Args:
      remote_trybot: Whether this run is a remote trybot run.
    """
    return 'trybot-%s' % self.name if remote_trybot else self.name

  def deepcopy(self):
    """Create a deep copy of this object.

    This is a specialized version of copy.deepcopy() for BuildConfig objects. It
    speeds up deep copies by 10x because we know in advance what is stored
    inside a BuildConfig object and don't have to do as much introspection. This
    function is called a lot during setup of the config objects so optimizing it
    makes a big difference. (It saves seconds off the load time of the
    cbuildbot_config module!)
    """
    new_config = BuildConfig(self)
    for k, v in self.iteritems():
      # type(v) is faster than isinstance.
      if type(v) is list:
        new_config[k] = v[:]

    if new_config.get('child_configs'):
      new_config['child_configs'] = [
          x.deepcopy() for x in new_config['child_configs']]

    if new_config.get('hw_tests'):
      new_config['hw_tests'] = [copy.copy(x) for x in new_config['hw_tests']]

    return new_config

  def derive(self, *args, **kwargs):
    """Create a new config derived from this one.

    Note: If an override is callable, it will be called and passed the prior
    value for the given key (or None) to compute the new value.

    Args:
      args: Mapping instances to mixin.
      kwargs: Settings to inject; see _settings for valid values.

    Returns:
      A new _config instance.
    """
    inherits = list(args)
    inherits.append(kwargs)
    new_config = self.deepcopy()

    for update_config in inherits:
      for k, v in update_config.iteritems():
        if callable(v):
          new_config[k] = v(new_config.get(k))
        else:
          new_config[k] = v

      keys_to_delete = [k for k in new_config if
                        new_config[k] is self._delete_key_sentinel]

      for k in keys_to_delete:
        new_config.pop(k, None)

    return new_config


class HWTestConfig(object):
  """Config object for hardware tests suites.

  Members:
    suite: Name of the test suite to run.
    timeout: Number of seconds to wait before timing out waiting for
             results.
    pool: Pool to use for hw testing.
    blocking: Suites that set this true run sequentially; each must pass
              before the next begins.  Tests that set this false run in
              parallel after all blocking tests have passed.
    async: Fire-and-forget suite.
    warn_only: Failure on HW tests warns only (does not generate error).
    critical: Usually we consider structural failures here as OK.
    priority:  Priority at which tests in the suite will be scheduled in
               the hw lab.
    file_bugs: Should we file bugs if a test fails in a suite run.
    num: Maximum number of DUTs to use when scheduling tests in the hw lab.
    minimum_duts: minimum number of DUTs required for testing in the hw lab.
    retry: Whether we should retry tests that fail in a suite run.
    max_retries: Integer, maximum job retries allowed at suite level.
                 None for no max.
    suite_min_duts: Preferred minimum duts. Lab will prioritize on getting such
                    number of duts even if the suite is competing with
                    other suites that have higher priority.

  Some combinations of member settings are invalid:
    * A suite config may not specify both blocking and async.
    * A suite config may not specify both retry and async.
    * A suite config may not specify both warn_only and critical.
  """
  # This timeout is larger than it needs to be because of autotest overhead.
  # TODO(davidjames): Reduce this timeout once http://crbug.com/366141 is fixed.
  DEFAULT_HW_TEST_TIMEOUT = 60 * 220
  BRANCHED_HW_TEST_TIMEOUT = 10 * 60 * 60

  def __init__(self, suite, num=constants.HWTEST_DEFAULT_NUM,
               pool=constants.HWTEST_MACH_POOL, timeout=DEFAULT_HW_TEST_TIMEOUT,
               async=False, warn_only=False, critical=False, blocking=False,
               file_bugs=False, priority=constants.HWTEST_BUILD_PRIORITY,
               retry=True, max_retries=10, minimum_duts=0, suite_min_duts=0):
    """Constructor -- see members above."""
    assert not async or (not blocking and not retry)
    assert not warn_only or not critical
    self.suite = suite
    self.num = num
    self.pool = pool
    self.timeout = timeout
    self.blocking = blocking
    self.async = async
    self.warn_only = warn_only
    self.critical = critical
    self.file_bugs = file_bugs
    self.priority = priority
    self.retry = retry
    self.max_retries = max_retries
    self.minimum_duts = minimum_duts
    self.suite_min_duts = suite_min_duts

  def SetBranchedValues(self):
    """Changes the HW Test timeout/priority values to branched values."""
    self.timeout = max(HWTestConfig.BRANCHED_HW_TEST_TIMEOUT, self.timeout)

    # Set minimum_duts default to 0, which means that lab will not check the
    # number of available duts to meet the minimum requirement before creating
    # a suite job for branched build.
    self.minimum_duts = 0

    # Only reduce priority if it's lower.
    new_priority = constants.HWTEST_DEFAULT_PRIORITY
    if (constants.HWTEST_PRIORITIES_MAP[self.priority] >
        constants.HWTEST_PRIORITIES_MAP[new_priority]):
      self.priority = new_priority

  @property
  def timeout_mins(self):
    return int(self.timeout / 60)

  def __eq__(self, other):
    return self.__dict__ == other.__dict__


class Config(dict):
  """This holds a set of named BuildConfig values."""

  def __init__(self, defaults=None):
    """Init."""
    super(Config, self).__init__()
    self._defaults = {} if defaults is None else defaults

  def GetDefault(self):
    """Create the cannonical default build configuration."""
    # Enumeration of valid settings; any/all config settings must be in this.
    # All settings must be documented.
    return BuildConfig(**self._defaults)

  #
  # Methods used when creating a Config programatically.
  #

  def AddConfig(self, config, name, *args, **kwargs):
    """Derive and add the config to cbuildbot's usable config targets

    Args:
      config: BuildConfig to derive the new config from.
      name: The name to label this configuration; this is what cbuildbot
            would see.
      args: See the docstring of derive.
      kwargs: See the docstring of derive.

    Returns:
      See the docstring of derive.
    """
    inherits, overrides = args, kwargs
    overrides['name'] = name

    # Add ourselves into the global dictionary, adding in the defaults.
    new_config = config.derive(*inherits, **overrides)
    self[name] = self.GetDefault().derive(config, new_config)

    # Return a BuildConfig object without the defaults, so that other objects
    # can derive from us without inheriting the defaults.
    return new_config

  def AddRawConfig(self, name, *args, **kwargs):
    """Add a config containing only explicitly listed values (no defaults)."""
    return self.AddConfig(BuildConfig(), name, *args, **kwargs)

  def AddGroup(self, name, *args, **kwargs):
    """Create a new group of build configurations.

    Args:
      name: The name to label this configuration; this is what cbuildbot
            would see.
      args: Configurations to build in this group. The first config in
            the group is considered the primary configuration and is used
            for syncing and creating the chroot.
      kwargs: Override values to use for the parent config.

    Returns:
      A new BuildConfig instance.
    """
    child_configs = [self.GetDefault().derive(x, grouped=True) for x in args]
    return self.AddConfig(args[0], name, child_configs=child_configs, **kwargs)
