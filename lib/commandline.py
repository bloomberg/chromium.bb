# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Purpose of this module is to hold common script/commandline functionality.

This ranges from optparse, to a basic script wrapper setup (much like
what is used for chromite.bin.*).
"""

import argparse
import collections
import datetime
import functools
import logging
import os
import optparse
import signal
import sys
import tempfile
import urlparse

# TODO(build): sort the buildbot.constants/lib.constants issue;
# lib shouldn't have to import from buildbot like this.
from chromite.buildbot import constants
from chromite.lib import cros_build_lib
from chromite.lib import git
from chromite.lib import gs
from chromite.lib import osutils


CHECKOUT_TYPE_UNKNOWN = 'unknown'
CHECKOUT_TYPE_GCLIENT = 'gclient'
CHECKOUT_TYPE_REPO = 'repo'
CHECKOUT_TYPE_SUBMODULE = 'submodule'


CheckoutInfo = collections.namedtuple(
    'CheckoutInfo', ['type', 'root', 'chrome_src_dir'])


def DetermineCheckout(cwd):
  """Gather information on the checkout we are in.

  Returns:
    A CheckoutInfo object with these attributes:
      type: The type of checkout.  Valid values are CHECKOUT_TYPE_*.
      root: The root of the checkout.
      chrome_src_dir: If the checkout is a Chrome checkout, the path to the
        Chrome src/ directory.
  """
  checkout_type = CHECKOUT_TYPE_UNKNOWN
  root, path = None, None
  for path in osutils.IteratePathParents(cwd):
    repo_dir = os.path.join(path, '.repo')
    if os.path.isdir(repo_dir):
      checkout_type = CHECKOUT_TYPE_REPO
      break
    gclient_file = os.path.join(path, '.gclient')
    if os.path.exists(gclient_file):
      checkout_type = CHECKOUT_TYPE_GCLIENT
      break
    submodule_git = os.path.join(path, '.git')
    if (os.path.isdir(submodule_git) and
        git.IsSubmoduleCheckoutRoot(cwd, 'origin', constants.CHROMIUM_GOB_URL)):
      checkout_type = CHECKOUT_TYPE_SUBMODULE
      break

  if checkout_type != CHECKOUT_TYPE_UNKNOWN:
    root = path

  # Determine the chrome src directory.
  chrome_src_dir = None
  if checkout_type == CHECKOUT_TYPE_GCLIENT:
    chrome_src_dir = os.path.join(root, 'src')
  elif checkout_type == CHECKOUT_TYPE_SUBMODULE:
    chrome_src_dir = root

  return CheckoutInfo(checkout_type, root, chrome_src_dir)


def GetCacheDir():
  """Calculate the current cache dir.

  Users can configure the cache dir using the --cache-dir argument and it is
  shared between cbuildbot and all child processes. If no cache dir is
  specified, FindCacheDir finds an alternative location to store the cache.

  Returns:
    The path to the cache dir.
  """
  return os.environ.get(
      constants.SHARED_CACHE_ENVVAR,
      BaseParser.FindCacheDir(None, None))


def AbsolutePath(_option, _opt, value):
  """Expand paths and make them absolute."""
  return osutils.ExpandPath(value)


def NormalizeGSPath(value):
  """Normalize GS paths."""
  url = gs.CanonicalizeURL(value, strict=True)
  return '%s%s' % (gs.BASE_GS_URL, os.path.normpath(url[len(gs.BASE_GS_URL):]))


def NormalizeLocalOrGSPath(value):
  """Normalize a local or GS path."""
  ptype = 'gs_path' if value.startswith(gs.BASE_GS_URL) else 'path'
  return VALID_TYPES[ptype](value)


def ParseDate(value):
  """Parse date argument into a datetime.date object.

  Args:
    value: String representing a single date in "YYYY-MM-DD" format.

  Returns:
    A datetime.date object.
  """
  try:
    return datetime.datetime.strptime(value, '%Y-%m-%d').date()
  except ValueError:
    # Give a helpful error message about the format expected.  Putting this
    # message in the exception is useless because argparse ignores the
    # exception message and just says the value is invalid.
    cros_build_lib.Error('Date is expected to be in format YYYY-MM-DD.')
    raise


def NormalizeUri(value):
  """Normalize a local path or URI."""
  o = urlparse.urlparse(value)
  if o.scheme == 'file':
    # Trim off the file:// prefix.
    return VALID_TYPES['path'](value[7:])
  elif o.scheme not in ('', 'gs'):
    o = list(o)
    o[2] = os.path.normpath(o[2])
    return urlparse.urlunparse(o)
  else:
    return NormalizeLocalOrGSPath(value)


def OptparseWrapCheck(desc, check_f, _option, opt, value):
  """Optparse adapter for type checking functionality."""
  try:
    return check_f(value)
  except ValueError:
    raise optparse.OptionValueError(
        'Invalid %s given: --%s=%s' % (desc, opt, value))


VALID_TYPES = {
    'date': ParseDate,
    'path': osutils.ExpandPath,
    'gs_path': NormalizeGSPath,
    'local_or_gs_path': NormalizeLocalOrGSPath,
    'path_or_uri': NormalizeUri,
}


class Option(optparse.Option):
  """Subclass to implement path evaluation & other useful types."""

  _EXTRA_TYPES = ("path", "gs_path")
  TYPES = optparse.Option.TYPES + _EXTRA_TYPES
  TYPE_CHECKER = optparse.Option.TYPE_CHECKER.copy()
  for t in _EXTRA_TYPES:
    TYPE_CHECKER[t] = functools.partial(OptparseWrapCheck, t, VALID_TYPES[t])


class FilteringOption(Option):
  """Subclass that supports Option filtering for FilteringOptionParser"""

  def take_action(self, action, dest, opt, value, values, parser):
    if action in FilteringOption.ACTIONS:
      Option.take_action(self, action, dest, opt, value, values, parser)

    if value is None:
      value = []
    elif not self.nargs or self.nargs <= 1:
      value = [value]

    parser.AddParsedArg(self, opt, [str(v) for v in value])


class BaseParser(object):
  """Base parser class that includes the logic to add logging controls."""

  DEFAULT_LOG_LEVELS = ('fatal', 'critical', 'error', 'warning', 'info',
                        'debug')

  DEFAULT_LOG_LEVEL = "info"
  ALLOW_LOGGING = True

  REPO_CACHE_DIR = '.cache'
  CHROME_CACHE_DIR = '.cros_cache'

  def __init__(self, **kwargs):
    """Initialize this parser instance.

    kwargs:
      logging: Defaults to ALLOW_LOGGING from the class; if given,
        add --log-level.
      default_log_level: If logging is enabled, override the default logging
        level.  Defaults to the class's DEFAULT_LOG_LEVEL value.
      log_levels: If logging is enabled, this overrides the enumeration of
        allowed logging levels.  If not given, defaults to the classes
        DEFAULT_LOG_LEVELS value.
      manual_debug: If logging is enabled and this is True, suppress addition
        of a --debug alias.  This option defaults to True unless 'debug' has
        been exempted from the allowed logging level targets.
      caching: If given, must be either a callable that discerns the cache
        location if it wasn't specified (the prototype must be akin to
        lambda parser, values:calculated_cache_dir_path; it may return None to
        indicate that it handles setting the value on its own later in the
        parsing including setting the env), or True; if True, the
        machinery defaults to invoking the class's FindCacheDir method
        (which can be overridden).  FindCacheDir $CROS_CACHEDIR, falling
        back to $REPO/.cache, finally falling back to $TMP.
        Note that the cache_dir is not created, just discerned where it
        should live.
        If False, or caching is not given, then no --cache-dir option will be
        added.
    """
    self.debug_enabled = False
    self.caching_group = None
    self.debug_group = None
    self.default_log_level = None
    self.log_levels = None
    self.logging_enabled = kwargs.get('logging', self.ALLOW_LOGGING)
    self.default_log_level = kwargs.get('default_log_level',
                                        self.DEFAULT_LOG_LEVEL)
    self.log_levels = tuple(x.lower() for x in
                            kwargs.get('log_levels', self.DEFAULT_LOG_LEVELS))
    self.debug_enabled = (not kwargs.get('manual_debug', False)
                          and 'debug' in self.log_levels)
    self.caching = kwargs.get('caching', False)

  @staticmethod
  def PopUsedArgs(kwarg_dict):
    """Removes keys used by the base parser from the kwarg namespace."""
    parser_keys = ['logging', 'default_log_level', 'log_levels', 'manual_debug',
                   'caching']
    for key in parser_keys:
      kwarg_dict.pop(key, None)

  def SetupOptions(self):
    """Sets up special chromite options for an OptionParser."""
    if self.logging_enabled:
      self.debug_group = self.add_option_group("Debug options")
      self.add_option_to_group(
          self.debug_group, "--log-level", choices=self.log_levels,
          default=self.default_log_level,
          help="Set logging level to report at.")
      if self.debug_enabled:
        self.add_option_to_group(
          self.debug_group, "--debug", action="store_const", const="debug",
          dest="log_level", help="Alias for `--log-level=debug`. "
          "Useful for debugging bugs/failures.")
      self.add_option_to_group(
        self.debug_group, '--nocolor', action='store_false', dest='color',
        default=None,
        help='Do not use colorized output (or `export NOCOLOR=true`)')

    if self.caching:
      self.caching_group = self.add_option_group("Caching Options")
      self.add_option_to_group(
          self.caching_group, "--cache-dir", default=None, type='path',
          help="Override the calculated chromeos cache directory; "
          "typically defaults to '$REPO/.cache' .")

  def SetupLogging(self, opts):
    value = opts.log_level.upper()
    logging.getLogger().setLevel(getattr(logging, value))
    return value

  def DoPostParseSetup(self, opts, args):
    """Method called to handle post opts/args setup.

    This can be anything from logging setup to positional arg count validation.

    Args:
      opts: optparse.Values or argparse.Namespace instance
      args: position arguments unconsumed from parsing.

    Returns:
      (opts, args), w/ whatever modification done.
    """
    if self.logging_enabled:
      value = self.SetupLogging(opts)
      if self.debug_enabled:
        opts.debug = (value == "DEBUG")

    if self.caching:
      path = os.environ.get(constants.SHARED_CACHE_ENVVAR)
      if path is not None and opts.cache_dir is None:
        opts.cache_dir = os.path.abspath(path)

      opts.cache_dir_specified = opts.cache_dir is not None
      if not opts.cache_dir_specified:
        func = self.FindCacheDir if not callable(self.caching) else self.caching
        opts.cache_dir = func(self, opts)
      if opts.cache_dir is not None:
        self.ConfigureCacheDir(opts.cache_dir)

    return opts, args

  @staticmethod
  def ConfigureCacheDir(cache_dir):
    if cache_dir is None:
      os.environ.pop(constants.SHARED_CACHE_ENVVAR, None)
      logging.debug("Removed cache_dir setting")
    else:
      os.environ[constants.SHARED_CACHE_ENVVAR] = cache_dir
      logging.debug("Configured cache_dir to %r", cache_dir)

  @classmethod
  def FindCacheDir(cls, _parser, _opts):
    logging.debug('Cache dir lookup.')
    checkout = DetermineCheckout(os.getcwd())
    path = None
    if checkout.type == CHECKOUT_TYPE_REPO:
      path = os.path.join(checkout.root, cls.REPO_CACHE_DIR)
    elif checkout.type in (CHECKOUT_TYPE_GCLIENT, CHECKOUT_TYPE_SUBMODULE):
      path = os.path.join(checkout.root, cls.CHROME_CACHE_DIR)
    elif checkout.type == CHECKOUT_TYPE_UNKNOWN:
      path = os.path.join(tempfile.gettempdir(), 'chromeos-cache')
    else:
      raise AssertionError('Unexpected type %s' % checkout.type)

    return path

  def add_option_group(self, *args, **kwargs):
    """Returns a new option group see optparse.OptionParser.add_option_group."""
    raise NotImplementedError('Subclass must override this method')

  @staticmethod
  def add_option_to_group(group, *args, **kwargs):
    """Adds the given option defined by args and kwargs to group."""
    group.add_option(*args, **kwargs)


class ArgumentNamespace(argparse.Namespace):
  """Class to mimic argparse.Namespace with value freezing support."""
  __metaclass__ = cros_build_lib.FrozenAttributesClass
  _FROZEN_ERR_MSG = 'Option values are frozen, cannot alter %s.'


# Note that because optparse.Values is not a new-style class this class
# must use the mixin FrozenAttributesMixin rather than the metaclass
# FrozenAttributesClass.
class OptionValues(cros_build_lib.FrozenAttributesMixin, optparse.Values):
  """Class to mimic optparse.Values with value freezing support."""
  _FROZEN_ERR_MSG = 'Option values are frozen, cannot alter %s.'

  def __init__(self, defaults, *args, **kwargs):
    cros_build_lib.FrozenAttributesMixin.__init__(self)
    optparse.Values.__init__(self, defaults, *args, **kwargs)

    # Used by FilteringParser.
    self.parsed_args = None


class OptionParser(optparse.OptionParser, BaseParser):
  """Custom parser adding our custom option class in.

  Aside from adding a couple of types (path for absolute paths,
  gs_path for google storage urls, and log_level for logging level control),
  this additionally exposes logging control by default; if undesired,
  either derive from this class setting ALLOW_LOGGING to False, or
  pass in logging=False to the constructor.
  """

  DEFAULT_OPTION_CLASS = Option

  def __init__(self, usage=None, **kwargs):
    BaseParser.__init__(self, **kwargs)
    self.PopUsedArgs(kwargs)
    kwargs.setdefault("option_class", self.DEFAULT_OPTION_CLASS)
    optparse.OptionParser.__init__(self, usage=usage, **kwargs)
    self.SetupOptions()

  def parse_args(self, args=None, values=None):
    # If no Values object is specified then use our custom OptionValues.
    if values is None:
      values = OptionValues(defaults=self.defaults)

    opts, remaining = optparse.OptionParser.parse_args(
        self, args=args, values=values)
    return self.DoPostParseSetup(opts, remaining)


PassedOption = collections.namedtuple(
        'PassedOption', ['opt_inst', 'opt_str', 'value_str'])


class FilteringParser(OptionParser):
  """Custom option parser for filtering options."""

  DEFAULT_OPTION_CLASS = FilteringOption

  def parse_args(self, args=None, values=None):
    # If no Values object is specified then use our custom OptionValues.
    if values is None:
      values = OptionValues(defaults=self.defaults)

    values.parsed_args = []

    return OptionParser.parse_args(self, args=args, values=values)

  def AddParsedArg(self, opt_inst, opt_str, value_str):
    """Add a parsed argument with attributes.

    Args:
      opt_inst: An instance of a raw optparse.Option object that represents the
                option.
      opt_str: The option string.
      value_str: A list of string-ified values dentified by OptParse.
    """
    self.values.parsed_args.append(PassedOption(opt_inst, opt_str, value_str))

  @staticmethod
  def FilterArgs(parsed_args, filter_fn):
    """Filter the argument by passing it through a function.

    Args:
      parsed_args: The list of parsed argument namedtuples to filter.  Tuples
        are of the form (opt_inst, opt_str, value_str).
      filter_fn: A function with signature f(PassedOption), and returns True if
        the argument is to be passed through.  False if not.

    Returns:
      A tuple containing two lists - one of accepted arguments and one of
      removed arguments.
    """
    removed = []
    accepted = []
    for arg in parsed_args:
      target = accepted if filter_fn(arg) else removed
      target.append(arg.opt_str)
      target.extend(arg.value_str)

    return accepted, removed


# pylint: disable=R0901
class ArgumentParser(BaseParser, argparse.ArgumentParser):
  """Custom argument parser for use by chromite.

  This class additionally exposes logging control by default; if undesired,
  either derive from this class setting ALLOW_LOGGING to False, or
  pass in logging=False to the constructor.
  """
  # pylint: disable=W0231
  def __init__(self, usage=None, **kwargs):
    kwargs.setdefault('formatter_class', argparse.RawDescriptionHelpFormatter)
    BaseParser.__init__(self, **kwargs)
    self.PopUsedArgs(kwargs)
    argparse.ArgumentParser.__init__(self, usage=usage, **kwargs)
    self._SetupTypes()
    self.SetupOptions()

  def _SetupTypes(self):
    """Register types with ArgumentParser."""
    for t, check_f in VALID_TYPES.iteritems():
      self.register('type', t, check_f)

  def add_option_group(self, *args, **kwargs):
    """Return an argument group rather than an option group."""
    return self.add_argument_group(*args, **kwargs)

  @staticmethod
  def add_option_to_group(group, *args, **kwargs):
    """Adds an argument rather than an option to the given group."""
    return group.add_argument(*args, **kwargs)

  def parse_args(self, args=None, namespace=None):
    """Translates OptionParser call to equivalent ArgumentParser call."""
    # If no Namespace object is specified then use our custom ArgumentNamespace.
    if namespace is None:
      namespace = ArgumentNamespace()

    # Unlike OptionParser, ArgParser works only with a single namespace and no
    # args. Re-use BaseParser DoPostParseSetup but only take the namespace.
    namespace = argparse.ArgumentParser.parse_args(
        self, args=args, namespace=namespace)
    return self.DoPostParseSetup(namespace, None)[0]


class _ShutDownException(SystemExit):
  """Exception raised when user hits CTRL+C."""

  def __init__(self, sig_num, message):
    self.signal = sig_num
    # Setup a usage message primarily for any code that may intercept it
    # while this exception is crashing back up the stack to us.
    SystemExit.__init__(self, message)
    self.args = (sig_num, message)


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
    find_target_func: a function, which, when given the absolute
      pathway the script was invoked via (for example,
      /home/ferringb/cros/trunk/chromite/bin/cros_sdk; note that any
      trailing .py from the path name will be removed),
      will return the main function to invoke (that functor will take
      a single arg- a list of arguments, and shall return either None
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
  # Note a script target may setup default logging in its module namespace
  # which will take precedence over this.
  logging.basicConfig(
      level=log_level,
      format=log_format,
      datefmt=constants.LOGGER_DATE_FMT)

  signal.signal(signal.SIGTERM, _DefaultHandler)

  ret = 1
  try:
    ret = target(argv[1:])
  except _ShutDownException as e:
    sys.stdout.flush()
    print >> sys.stderr, ("%s: Signaled to shutdown: caught %i signal." %
                          (name, e.signal,))
    sys.stderr.flush()
  except SystemExit as e:
    # Right now, let this crash through- longer term, we'll update the scripts
    # in question to not use sys.exit, and make this into a flagged error.
    raise
  except Exception as e:
    sys.stdout.flush()
    print >> sys.stderr, ("%s: Unhandled exception:" % (name,))
    sys.stderr.flush()
    raise
  finally:
    logging.shutdown()

  if ret is None:
    ret = 0
  sys.exit(ret)
