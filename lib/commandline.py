# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Purpose of this module is to hold common script/commandline functionality.

This ranges from optparse, to a basic script wrapper setup (much like
what is used for chromite.bin.* ).
"""

import logging
import os
import optparse
import signal
import sys
import tempfile
# TODO(build): sort the buildbot.constants/lib.constants issue;
# lib shouldn't have to import from buildbot like this.
from chromite.buildbot import constants
from chromite.lib import cros_build_lib
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
  SUPPORTS_CACHING = False


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
      caching: If given, must be either a callable that discerns the cache
        location if it wasn't specified (the prototype must be akin to
        lambda parser, values:calculated_cache_dir_path; it may return None to
        indicate that it handles setting the value on it's own later in the
        parsing including setting the env), or True; if True, the
        machinery defaults to invoking the class's FindCacheDir method
        (which can be overridden).  FindCacheDir $CROS_CACHEDIR, falling
        back to $REPO/.cache, finally falling back to $TMP.
        Note that the cache_dir is not created, just discerned where it
        should live.
        If False, or caching is not given and the class attr SUPPORTS_CACHING
        is False, then no --cache-dir option will be added.
    """
    self.debug_enabled = False
    self.caching_group = self.debug_group = None
    self.logging_enabled = kwargs.pop("logging", self.ALLOW_LOGGING)
    if self.logging_enabled:
      log_levels = tuple(x.lower() for x in kwargs.pop(
          "log_levels", self.DEFAULT_LOG_LEVELS))
      default_level = kwargs.pop("default_log_level", self.DEFAULT_LOG_LEVEL)
      self.debug_enabled = (not kwargs.pop("manual_debug", False)
                            and "debug" in log_levels)

    self.caching = kwargs.pop("caching", self.SUPPORTS_CACHING)
    # Allow usage to be passed positionally, since that's semi common.
    kwargs.setdefault("option_class", self.DEFAULT_OPTION_CLASS)
    optparse.OptionParser.__init__(self, usage=usage, **kwargs)

    if self.logging_enabled:
      self.debug_group = group = self.add_option_group("Debug options")
      group.add_option("--log-level", choices=log_levels, default=default_level,
                      help="Set logging level to report at.")
      if self.debug_enabled:
        group.add_option("--debug", action="store_const", const="debug",
                        dest="log_level",
                        help="Alias for `--log-level=debug`.  Useful for "
                        "debugging bugs/failures.")
    if self.caching:
      self.caching_group = self.add_option_group("Caching Options")
      self.caching_group.add_option(
          "--cache-dir", default=None, type='path',
          help="Override the calculated chromeos cache directory; "
          "typically defaults to '$REPO/.cache' .")

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

    if self.caching:
      func = self.FindCacheDir if not callable(self.caching) else self.caching
      opts.cache_dir = func(self, opts)
      if opts.cache_dir is not None:
        opts.cache_dir = os.path.abspath(func(self, opts))
        self.ConfigureCacheDir(opts.cache_dir)

    return opts, args

  def ConfigureCacheDir(self, cache_dir):
    os.environ[constants.SHARED_CACHE_ENVVAR] = cache_dir
    logging.debug("Configured cache_dir to %r", cache_dir)

  @staticmethod
  def FindCacheDir(_parser, _opts):
    path = os.environ.get(constants.SHARED_CACHE_ENVVAR)
    if path is None:
      path = cros_build_lib.FindRepoCheckoutRoot()
      path = os.path.join(path, '.cache') if path else path
    if path is None:
      path = os.path.join(tempfile.gettempdir(), 'chromeos-cache')
    return path

  def parse_args(self, args=None, values=None):
    opts, remaining = optparse.OptionParser.parse_args(
        self, args=args, values=values)
    return self.DoPostParseSetup(opts, remaining)


class _ShutDownException(SystemExit):

  def __init__(self, sig_num, message):
    self.signal = sig_num
    # Setup a usage mesage primarily for any code that may intercept it
    # while this exception is crashing back up the stack to us.
    SystemExit.__init__(self, message)


def _DefaultHandler(signum, _frame):
  # Don't double process sigterms; just trigger shutdown from the first
  # exception.
  signal.signal(signum, signal.SIG_IGN)
  raise _ShutDownException(
      signum, "Received signal %i; shutting down" % (signum,))


def ScriptWrapperMain(find_target_func, argv=None,
                      log_level=logging.DEBUG,
                      log_format=constants.LOGGER_FMT):
  """Function usable for chromite.script.* style wrapping.

  Note that this function invokes sys.exit on the way out by default.

  Args:
    find_target_func: a function, which when given the absolute
      pathway this the script was invoked via (for example,
      /home/ferringb/cros/trunk/chromite/bin/cros_sdk; note that any
      trailing .py from the path name will be removed),
      wille return the main function to invoke (that functor will take
      a single arg- a list of arguments, and shall return either None,
      or an integer, to indicate the exit code).
    argv: sys.argv, or an equivalent tuple for testing.  If nothing is
      given, sys.argv is defaulted to.
    log_level: Default logging level to start at.
    log_format: Default logging format to use.
  """
  if argv is None:
    argv = sys.argv[:]
  target = os.path.abspath(argv[0])
  name = os.path.basename(target)
  if target.endswith('.py'):
    target = os.path.splitext(target)[0]
  target = find_target_func(target)
  if target is None:
    print >> sys.stderr, ("Internal error detected- no main "
                          "functor found in module %r." % (name,))
    sys.exit(100)

  # Set up basic logging information for all modules that use logging.
  # Note a script target may setup default logging in it's module namespace
  # which will take precedence over this.
  logging.basicConfig(
      level=log_level,
      format=log_format,
      datefmt=constants.LOGGER_DATE_FMT)


  signal.signal(signal.SIGTERM, _DefaultHandler)

  ret = 1
  try:
    ret = target(argv[1:])
  except _ShutDownException, e:
    sys.stdout.flush()
    print >> sys.stderr, ("%s: Signaled to shutdown: caught %i signal." %
                          (name, e.signal,))
    sys.stderr.flush()
  except SystemExit, e:
    # Right now, let this crash through- longer term, we'll update the scripts
    # in question to not use sys.exit, and make this into a flagged error.
    raise
  except Exception, e:
    sys.stdout.flush()
    print >> sys.stderr, ("%s: Unhandled exception:" % (name,))
    sys.stderr.flush()
    raise

  if ret is None:
    ret = 0
  sys.exit(ret)
