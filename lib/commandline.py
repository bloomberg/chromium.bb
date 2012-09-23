# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Purpose of this module is to hold common optparse functionality.

Currently not much, but should expand going forward.
"""

import logging
import optparse
from chromite.lib import osutils


def AbsolutePath(_option, opt, value):
  """Expand paths and make them absolute."""
  expanded = osutils.ExpandPath(value)
  if expanded == "/":
    raise optparse.OptionValueError("Invalid path %s specified for %s"
                                    % (expanded, opt))

  return expanded


def ValidateGSPath(_option, opt, value):
  """Expand paths and make them absolute."""
  value = value.strip().rstrip("/")
  if not value.startswith("gs://"):
    raise optparse.OptionValueError("Invalid gs path %s specified for %s"
                                    % (value, opt))

  return value


def ValidateLogLevel(_option, opt, value):
  name = value.upper()
  if not hasattr(logging, name):
    raise optparse.OptionValueError("Invalid logging level given: --%s=%s"
        % (opt, value))
  return name


class Option(optparse.Option):
  """
  Subclass Option class to implement path evaluation, and other useful types.
  """
  TYPES = optparse.Option.TYPES + ("path", "gs_path", "log_level")
  TYPE_CHECKER = optparse.Option.TYPE_CHECKER.copy()
  TYPE_CHECKER["path"] = AbsolutePath
  TYPE_CHECKER["gs_path"] = ValidateGSPath
  TYPE_CHECKER["log_level"] = ValidateLogLevel


class OptionParser(optparse.OptionParser):

  """Custom parser adding our custom option class in.

  Aside from adding a couple of types (path for absolute paths,
  gs_path for google storage urls, and log_level for logging level control),
  this additionally exposes logging control by default; if undesired,
  either derive from this class setting ALLOW_LOGGING to False, or
  pass in logging=False to the constructor.
  """

  DEFAULT_OPTION_CLASS = Option
  DEFAULT_LOG_LEVELS = ("critical", "debug", "error", "fatal", "info",
                        "warning")
  DEFAULT_LOG_LEVEL = "info"
  ALLOW_LOGGING = True


  def __init__(self, usage=None, **kwargs):
    """Initialize this parser instance.

    kwargs:
      logging: Defaults to ALLOW_LOGGING from the class; if given,
        add --log-level.
      default_log_level: If logging is enabled, override the default logging
        level.  Defaults to the class's DEFAULT_LOG_LEVEL value.
      log_evels: If logging is enabled, this overrides the enumeration of
        allowed logging levels.  If not given, defaults to the classes
        DEFAULT_LOG_LEVELS value.
      manual_debug: If logging is enabled and this is True, suppress addition
        of a --debug alias.  This option defaults to True unless 'debug' has
        been exempted from the allowed logging level targets.
    """
    self.debug_enabled = False
    self.logging_enabled = kwargs.pop("logging", self.ALLOW_LOGGING)
    if self.logging_enabled:
      log_levels = tuple(x.lower() for x in kwargs.pop(
          "log_levels", self.DEFAULT_LOG_LEVELS))
      default_level = kwargs.pop("default_log_level", self.DEFAULT_LOG_LEVEL)
      self.debug_enabled = (not kwargs.pop("manual_debug", False)
                            and "debug" in log_levels)
    # Allow usage to be passed positionally, since that's semi common.
    kwargs.setdefault("option_class", self.DEFAULT_OPTION_CLASS)
    optparse.OptionParser.__init__(self, usage=usage, **kwargs)

    if self.logging_enabled:
      group = self.add_option_group("Debug options")
      group.add_option("--log-level", choices=log_levels, default=default_level,
                      help="Set logging level to report at.")
      if self.debug_enabled:
        group.add_option("--debug", action="store_const", const="debug",
                        dest="log_level",
                        help="Alias for `--log-level=debug`.  Useful for "
                        "debugging bugs/failures.")

  def DoPostParseSetup(self, opts, args):
    """Method called to handle post opts/args setup.

    This can be anything from logging setup to positional arg count validation.
    Args:
      opts: optparse.Values instance
      args: position arguments unconsumed from parsing.
    Returns:
      (opts, args), w/ whatever modification done.
    """
    if self.logging_enabled:
      value = opts.log_level.upper()
      logging.getLogger().setLevel(getattr(logging, value))
      if self.debug_enabled:
        opts.debug = value == "DEBUG"
    return opts, args

  def parse_args(self, args=None, values=None):
    opts, remaining = optparse.OptionParser.parse_args(
        self, args=args, values=values)
    return self.DoPostParseSetup(opts, remaining)
