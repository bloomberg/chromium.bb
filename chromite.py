#!/usr/bin/python
# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chromite."""

# Python imports
import ConfigParser
import cPickle
import optparse
import os
import sys
import tempfile


# Local library imports
from lib import text_menu
from lib.cros_build_lib import Die
from lib.cros_build_lib import Info
from lib.cros_build_lib import IsInsideChroot
from lib.cros_build_lib import RunCommand
from lib.cros_build_lib import Warning as Warn


# Find the Chromite root and Chromium OS root...  Note: in the chroot we may
# choose to install Chromite somewhere (/usr/lib/chromite?), so we use the
# environment variable to get the right place if it exists.
_CHROMITE_PATH = os.path.dirname(os.path.realpath(__file__))
_SRCROOT_PATH = os.environ.get('CROS_WORKON_SRCROOT',
                               os.path.realpath(os.path.join(_CHROMITE_PATH,
                                                             '..')))


# Commands can take one of these two types of specs.  Note that if a command
# takes a build spec, we will find the associated chroot spec.  This should be
# a human-readable string.  It is printed and also is the name of the spec
# directory.
_BUILD_SPEC_TYPE = 'build'
_CHROOT_SPEC_TYPE = 'chroot'


# This is a special target that indicates that you want to do something just
# to the host.  This means different things to different commands.
# TODO(dianders): Good idea or bad idea?
_HOST_TARGET = 'HOST'


# Define command handlers and command strings.  We define them in this way
# so that someone searching for what calls _CmdXyz can find it easy with a grep.
#
# ORDER MATTERS here when we show the menu.
#
# TODO(dianders): Make a command superclass, then make these subclasses.
_COMMAND_HANDLERS = [
    '_CmdBuild',
    '_CmdClean',
    '_CmdEbuild',
    '_CmdEmerge',
    '_CmdEquery',
    '_CmdPortageq',
    '_CmdShell',
    '_CmdWorkon',
]
_COMMAND_STRS = [fn_str[len('_Cmd'):].lower() for fn_str in _COMMAND_HANDLERS]


def _GetBoardDir(build_config):
  """Returns the board directory (inside the chroot) given the name.

  Args:
    build_config: A SafeConfigParser representing the config that we're
        building.

  Returns:
    The absolute path to the board.
  """
  target_name = build_config.get('BUILD', 'target')

  # Extra checks on these, since we sometimes might do a rm -f on the board
  # directory and these could cause some really bad behavior.
  assert target_name, "Didn't expect blank target name."
  assert len(target_name.split()) == 1, 'Target name should have no whitespace.'

  return os.path.join('/', 'build', target_name)


def _GetChrootAbsDir(chroot_config):
  """Returns the absolute chroot directory the chroot config.

  Args:
    chroot_config: A SafeConfigParser representing the config for the chroot.

  Returns:
    The chroot directory, always absolute.
  """
  chroot_dir = chroot_config.get('CHROOT', 'path')
  chroot_dir = os.path.join(_SRCROOT_PATH, chroot_dir)

  return chroot_dir


def _DoesChrootExist(chroot_config):
  """Return True if the chroot folder exists.

  Args:
    chroot_config: A SafeConfigParser representing the config for the chroot.

  Returns:
    True if the chroot folder exists.
  """
  chroot_dir = _GetChrootAbsDir(chroot_config)
  return os.path.isdir(chroot_dir)


def _FindCommand(cmd_name):
  """Find the command that matches the given command name.

  This tries to be smart.  See the cmd_name parameter for details.

  Args:
    cmd_name: Can be any of the following:
        1. The full name of a command.  This is checked first so that if one
           command name is a substring of another, you can still specify
           the shorter spec name and know you won't get a menu (the exact
           match prevents the menu).
        2. A _prefix_ that will be used to pare-down a menu of commands
           Can be the empty string to show a menu of all commands

  Returns:
    The command name.
  """
  # Always make cmd_name lower.  Commands are case-insensitive.
  cmd_name = cmd_name.lower()

  # If we're an exact match, we're done!
  if cmd_name in _COMMAND_STRS:
    return cmd_name

  # Find ones that match and put them in a menu...
  possible_cmds = []
  possible_choices = []
  for cmd_num, this_cmd in enumerate(_COMMAND_STRS):
    if this_cmd.startswith(cmd_name):
      handler = eval(_COMMAND_HANDLERS[cmd_num])
      assert hasattr(handler, '__doc__'), \
             ('All handlers must have docstrings: %s' % cmd_name)
      desc = handler.__doc__.splitlines()[0]

      possible_cmds.append(this_cmd)
      possible_choices.append('%s - %s' % (this_cmd, desc))

  if not possible_choices:
    Die('No commands matched: "%s".  '
        'Try running with no arguments for a menu.' %
        cmd_name)

  if cmd_name and len(possible_choices) == 1:
    # Avoid showing the user a menu if the user's search string matched exactly
    # one item.
    choice = 0
    Info("Running command '%s'." % possible_cmds[choice])
  else:
    choice = text_menu.TextMenu(possible_choices, 'Which chromite command',
                                menu_width=0)

  if choice is None:
    Die('OK, cancelling...')
  else:
    return possible_cmds[choice]


def _FindSpec(spec_name, spec_type=_BUILD_SPEC_TYPE, can_show_ui=True):
  """Find the spec with the given name.

  This tries to be smart about helping the user to find the right spec.  See
  the spec_name parameter for details.

  Args:
    spec_name: Can be any of the following:
        1. A full path to a spec file (including the .spec suffix).  This is
           checked first (i.e. if os.path.isfile(spec_name), we assume we've
           got this case).
        2. The full name of a spec file somewhere in the spec search path
           (not including the .spec suffix).  This is checked second.  Putting
           this check second means that if one spec name is a substring of
           another, you can still specify the shorter spec name and know you
           won't get a menu (the exact match prevents the menu).
        3. A substring that will be used to pare-down a menu of spec files
           found in the spec search path.  Can be the empty string to show a
           menu of all spec files in the spec path.  NOTE: Only possible if
           can_show_ui is True.
    spec_type: The type of spec this is: 'chroot' or 'build'.
    can_show_ui: If True, enables the spec name to be a substring since we can
        then show a menu if the substring matches more than one thing.

  Returns:
    A path to the spec file.
  """
  spec_suffix = '.spec'

  # Handle 'HOST' for spec name w/ no searching, so it counts as an exact match.
  if spec_type == _BUILD_SPEC_TYPE and spec_name == _HOST_TARGET.lower():
    return _HOST_TARGET

  # If we have an exact path name, that's it.  No searching.
  if os.path.isfile(spec_name):
    return spec_name

  # Figure out what our search path should be.
  # ...make these lists in anticipation of the need to support specs that live
  # in private overlays.
  search_path = [
      os.path.join(_CHROMITE_PATH, 'specs', spec_type),
  ]

  # Look for an exact match of a spec name.  An exact match will go through with
  # no menu.
  if spec_name:
    for dir_path in search_path:
      spec_path = os.path.join(dir_path, spec_name + spec_suffix)
      if os.path.isfile(spec_path):
        return spec_path

  # Die right away if we can't show UI and didn't have an exact match.
  if not can_show_ui:
    Die("Couldn't find %s spec: %s" % (spec_type, spec_name))

  # No full path and no exact match.  Move onto a menu.
  # First step is to construct the options.  We'll store in a dict keyed by
  # spec name.
  options = {}
  for dir_path in search_path:
    for file_name in os.listdir(dir_path):
      # Find any files that end with ".spec" in a case-insensitive manner.
      if not file_name.lower().endswith(spec_suffix):
        continue

      this_spec_name, _ = os.path.splitext(file_name)

      # Skip if this spec file doesn't contain our substring.  We are _always_
      # case insensitive here to be helpful to the user.
      if spec_name.lower() not in this_spec_name.lower():
        continue

      # Disallow the spec to appear twice in the search path.  This is the
      # safest thing for now.  We could revisit it later if we ever found a
      # good reason (and if we ever have more than one directory in the
      # search path).
      if this_spec_name in options:
        Die('Spec %s was found in two places in the search path' %
            this_spec_name)

      # Combine to get a full path.
      full_path = os.path.join(dir_path, file_name)

      # Ignore directories or anything else that isn't a file.
      if not os.path.isfile(full_path):
        continue

      # OK, it's good.  Store the path.
      options[this_spec_name] = full_path

  # Add 'HOST'.  All caps so it sorts first.
  if (spec_type == _BUILD_SPEC_TYPE and
      spec_name.lower() in _HOST_TARGET.lower()):
    options[_HOST_TARGET] = _HOST_TARGET

  # If no match, die.
  if not options:
    Die("Couldn't find any matching %s specs for: %s" % (spec_type, spec_name))

  # Avoid showing the user a menu if the user's search string matched exactly
  # one item.
  if spec_name and len(options) == 1:
    _, spec_path = options.popitem()
    return spec_path

  # If more than one, show a menu...
  option_keys = sorted(options.iterkeys())
  choice = text_menu.TextMenu(option_keys, 'Choose a %s spec' % spec_type)

  if choice is None:
    Die('OK, cancelling...')
  else:
    return options[option_keys[choice]]


def _ReadConfig(spec_path):
  """Read the a build config or chroot config from a spec file.

  This includes adding thue proper _default stuff in.

  Args:
    spec_path: The path to the build or chroot spec.

  Returns:
    config: A SafeConfigParser representing the config.
  """
  spec_name, _ = os.path.splitext(os.path.basename(spec_path))
  spec_dir = os.path.dirname(spec_path)

  config = ConfigParser.SafeConfigParser({'name': spec_name})
  config.read(os.path.join(spec_dir, '_defaults'))
  config.read(spec_path)

  return config


def _GetBuildConfigFromArgs(argv):
  """Helper for commands that take a build config in the arg list.

  This function can Die() in some instances.

  Args:
    argv: A list of command line arguments.  If non-empty, [0] should be the
        build spec.  These will not be modified.

  Returns:
    argv: The argv with the build spec stripped off.  This might be argv[1:] or
        just argv.  Not guaranteed to be new memory.
    build_config: The SafeConfigParser for the build config; might be None if
        this is a host config.  TODO(dianders): Should there be a build spec for
        the host?
  """
  # The spec name is optional.  If no arguments, we'll show a menu...
  # Note that if there are arguments, but the first argument is a flag, we'll
  # assume that we got called before OptionParser.  In that case, they might
  # have specified options but still want the board menu.
  if argv and not argv[0].startswith('-'):
    spec_name = argv[0]
    argv = argv[1:]
  else:
    spec_name = ''

  spec_path = _FindSpec(spec_name)

  if spec_path == _HOST_TARGET:
    return argv, None

  build_config = _ReadConfig(spec_path)

  # TODO(dianders): Add a config checker class that makes sure that the
  # target is not a blank string.  Might also be good to make sure that the
  # target has no whitespace (so we don't screw up a subcommand invoked
  # through a shell).

  return argv, build_config


def _SplitEnvFromArgs(argv):
  """Split environment settings from arguments.

  This function will just loop over the arguments, looking for ones with an '='
  character in them.  As long as it finds them, it takes them out of the arg
  list adds them to a dictionary.  As soon as it finds the first argument
  without an '=', it stops looping.

  NOTE: Some doctests below test for equality with ==, since dicts with more
        than one item may be arbitrarily ordered.

  >>> result = _SplitEnvFromArgs(['abc=1', 'def=two', 'three'])
  >>> result == ({'abc': '1', 'def': 'two'}, ['three'])
  True

  >>> _SplitEnvFromArgs(['one', 'two', 'three'])
  ({}, ['one', 'two', 'three'])

  >>> _SplitEnvFromArgs(['abc=1', 'three', 'def=two'])
  ({'abc': '1'}, ['three', 'def=two'])

  >>> result = _SplitEnvFromArgs(['abc=1', 'ghi=4 4', 'def=two'])
  >>> result == ({'abc': '1', 'ghi': '4 4', 'def': 'two'}, [])
  True

  >>> _SplitEnvFromArgs(['abc=1', 'abc=2', 'three'])
  ({'abc': '2'}, ['three'])

  >>> _SplitEnvFromArgs([])
  ({}, [])

  Args:
    argv: The arguments to parse; this list is not modified.  Should
        not include "argv[0]"
  Returns:
    env: A dict containing key=value paris.
    argv: A new list containing anything left after.
  """
  # Copy the list so we don't screw with caller...
  argv = list(argv)

  env = {}
  while argv:
    if '=' in argv[0]:
      key, val = argv.pop(0).split('=', 2)
      env[key] = val
    else:
      break

  return env, argv


def _DoMakeChroot(chroot_config, clean_first):
  """Build the chroot, if needed.

  Args:
    chroot_config: A SafeConfigParser representing the config for the chroot.
    clean_first: Delete any old chroot first.
  """
  # Skip this whole command if things already exist.
  # TODO(dianders): Theoretically, calling make_chroot a second time is OK
  # and "fixes up" the chroot.  ...but build_packages will do the fixups
  # anyway (I think), so this isn't that important.
  chroot_dir = _GetChrootAbsDir(chroot_config)
  if (not clean_first) and _DoesChrootExist(chroot_config):
    Info('%s already exists, skipping make_chroot.' % chroot_dir)
    return

  Info('MAKING THE CHROOT')

  # Put together command.
  cmd_list = [
      './make_chroot',
      '--chroot="%s"' % chroot_dir,
      chroot_config.get('CHROOT', 'make_chroot_flags'),
  ]
  if clean_first:
    cmd_list.insert(1, '--replace')

  # We're going convert to a string and force the shell to do all of the
  # splitting of arguments, since we're throwing all of the flags from the
  # config file in there.
  cmd = ' '.join(cmd_list)

  # We'll put CWD as src/scripts when running the command.  Since everyone
  # running by hand has their cwd there, it is probably the safest.
  cwd = os.path.join(_SRCROOT_PATH, 'src', 'scripts')

  # Run it.  Exceptions will cause the program to exit.
  RunCommand(cmd, shell=True, cwd=cwd, ignore_sigint=True)


def _DoEnterChroot(chroot_config, fn, *args, **kwargs):
  """Re-run the given function inside the chroot.

  When the function is run, it will be run in a SEPARATE INSTANCE of chromite,
  which will be run in the chroot.  This is a little weird.  Specifically:
  - When the callee executes, it will be a separate python instance.
    - Globals will be reset back to defaults.
    - A different version of python (with different modules) may end up running
      the script in the chroot.
  - All arguments are pickled up into a temp file, whose path is passed on the
    command line.
    - That means that args must be pickleable.
    - It also means that modifications to the parameters by the callee are not
      visible to the caller.
    - Even the function is "pickled".  The way the pickle works, I belive, is it
      just passes the name of the function.  If this name somehow resolves
      differently in the chroot, you may get weirdness.
  - Since we're in the chroot, obviously files may have different paths.  It's
    up to you to convert parameters if you need to.
  - The stdin, stdout, and stderr aren't touched.

  Args:
    chroot_config: A SafeConfigParser representing the config for the chroot.
    fn: The function to call.
    args: All other arguments will be passed to the function as is.
    kwargs: All other arguments will be passed to the function as is.
  """
  # Make sure that the chroot exists...
  chroot_dir = _GetChrootAbsDir(chroot_config)
  if not _DoesChrootExist(chroot_config):
    Die('Chroot dir does not exist; try the "build host" command.\n  %s.' %
        chroot_dir)

  Info('ENTERING THE CHROOT')

  # Save state to a temp file (inside the chroot!) using pickle.
  tmp_dir = os.path.join(chroot_dir, 'tmp')
  state_file = tempfile.NamedTemporaryFile(prefix='chromite', dir=tmp_dir)
  try:
    cPickle.dump((fn, args, kwargs), state_file, cPickle.HIGHEST_PROTOCOL)
    state_file.flush()

    # Translate temp file name into a chroot path...
    chroot_state_path = os.path.join('/tmp', os.path.basename(state_file.name))

    # Put together command.  We're going to force the shell to do all of the
    # splitting of arguments, since we're throwing all of the flags from the
    # config file in there.
    # TODO(dianders): Once chromite is in the path inside the chroot, we should
    #                 change it from '../../chromite/chromite.py' to just
    #                 'chromite'.
    cmd = (
        './enter_chroot.sh --chroot="%s" %s --'
        ' python ../../chromite/chromite.py --resume-state %s') % (
            chroot_dir,
            chroot_config.get('CHROOT', 'enter_chroot_flags'),
            chroot_state_path)

    # We'll put CWD as src/scripts when running the command.  Since everyone
    # running by hand has their cwd there, it is probably the safest.
    cwd = os.path.join(_SRCROOT_PATH, 'src', 'scripts')

    # Run it.  We allow "error" so we don't print a confusing error message
    # filled with out resume-state garbage on control-C.
    cmd_result = RunCommand(cmd, shell=True, cwd=cwd, print_cmd=False,
                            exit_code=True, error_ok=True, ignore_sigint=True)

    if cmd_result.returncode:
      Die('Chroot exited with error code %d' % cmd_result.returncode)
  finally:
    # Make sure things get closed (and deleted), even upon exception.
    state_file.close()


def _DoSetupBoard(build_config, clean_first):
  """Setup the board, if needed.

  This just runs the setup_board command with the proper args, if needed.

  Args:
    build_config: A SafeConfigParser representing the build config.
    clean_first: Delete any old board config first.
  """
  # Skip this whole command if things already exist.
  board_dir = _GetBoardDir(build_config)
  if (not clean_first) and os.path.isdir(board_dir):
    Info('%s already exists, skipping setup_board.' % board_dir)
    return

  Info('SETTING UP THE BOARD')

  # Put together command.
  cmd_list = [
      './setup_board',
      '--board="%s"' % build_config.get('BUILD', 'target'),
      build_config.get('BUILD', 'setup_board_flags'),
  ]
  if clean_first:
    cmd_list.insert(1, '--force')

  # We're going convert to a string and force the shell to do all of the
  # splitting of arguments, since we're throwing all of the flags from the
  # config file in there.
  cmd = ' '.join(cmd_list)

  # We'll put CWD as src/scripts when running the command.  Since everyone
  # running by hand has their cwd there, it is probably the safest.
  cwd = os.path.join(_SRCROOT_PATH, 'src', 'scripts')

  # Run it.  Exceptions will cause the program to exit.
  RunCommand(cmd, shell=True, cwd=cwd, ignore_sigint=True)


def _DoBuildPackages(build_config):
  """Build packages.

  This just runs the build_packages command with the proper args.

  Args:
    build_config: A SafeConfigParser representing the build config.
  """
  Info('BUILDING PACKAGES')

  # Put together command.  We're going to force the shell to do all of the
  # splitting of arguments, since we're throwing all of the flags from the
  # config file in there.
  cmd = './build_packages --board="%s" %s' % (
      build_config.get('BUILD', 'target'),
      build_config.get('BUILD', 'build_packages_flags')
  )

  # We'll put CWD as src/scripts when running the command.  Since everyone
  # running by hand has their cwd there, it is probably the safest.
  cwd = os.path.join(_SRCROOT_PATH, 'src', 'scripts')

  # Run it.  Exceptions will cause the program to exit.
  RunCommand(cmd, shell=True, cwd=cwd, ignore_sigint=True)


def _DoBuildImage(build_config):
  """Build an image.

  This just runs the build_image command with the proper args.

  Args:
    build_config: A SafeConfigParser representing the build config.
  """
  Info('BUILDING THE IMAGE')

  # Put together command.  We're going to force the shell to do all of the
  # splitting of arguments, since we're throwing all of the flags from the
  # config file in there.
  cmd = './build_image --board="%s" %s' % (
      build_config.get('BUILD', 'target'),
      build_config.get('IMAGE', 'build_image_flags')
  )

  # We'll put CWD as src/scripts when running the command.  Since everyone
  # running by hand has their cwd there, it is probably the safest.
  cwd = os.path.join(_SRCROOT_PATH, 'src', 'scripts')

  # Run it.  Exceptions will cause the program to exit.
  RunCommand(cmd, shell=True, cwd=cwd, ignore_sigint=True)


def _DoClean(chroot_config, build_config, want_force_yes):
  """Clean a target.

  Args:
    chroot_config: A SafeConfigParser representing the config for the chroot.
    build_config: A SafeConfigParser representing the build config.
    want_force_yes: If True, we won't ask any questions--we'll just assume
        that the user really wants to kill the directory.  If False, we'll
        show UI asking the user to confirm.
  """
  # We'll need the directory so we can delete stuff; this is a chroot path.
  board_dir = _GetBoardDir(build_config)

  # If not in the chroot, convert board_dir into a non-chroot path...
  if not IsInsideChroot():
    chroot_dir = _GetChrootAbsDir(chroot_config)

    # We'll need to make the board directory relative to the chroot.
    assert board_dir.startswith('/'), 'Expected unix-style, absolute path.'
    board_dir = board_dir.lstrip('/')
    board_dir = os.path.join(chroot_dir, board_dir)

  if not os.path.isdir(board_dir):
    Die("Nothing to clean: the board directory doesn't exist.\n  %s" %
        board_dir)

  if not want_force_yes:
    sys.stderr.write('\n'
                     'Board dir is at: %s\n'
                     'Are you sure you want to delete it (YES/NO)? ' %
                     board_dir)
    answer = raw_input()
    if answer.lower() not in ('y', 'ye', 'yes'):
      Die("You must answer 'yes' if you want to proceed.")

  # Since we're about to do a sudo rm -rf, these are just extra precautions.
  # This shouldn't be the only place testing these (assert fails are ugly and
  # can be turned off), but better safe than sorry.
  # Note that the restriction on '*' is a bit unnecessary, since no shell
  # expansion should happen.  ...but again, I'd rather be safe.
  assert os.path.isabs(board_dir), 'Board dir better be absolute'
  assert board_dir != '/', 'Board dir better not be /'
  assert '*' not in board_dir, 'Board dir better not have any *s'
  assert build_config.get('BUILD', 'target'), 'Target better not be blank'
  assert build_config.get('BUILD', 'target') in board_dir, \
         'Target name better be in board dir'

  argv = ['sudo', '--', 'rm', '-rf', board_dir]
  RunCommand(argv)
  Info('Deleted: %s' % board_dir)


def _DoDistClean(chroot_config, want_force_yes):
  """Remove the whole chroot.

  Args:
    chroot_config: A SafeConfigParser representing the config for the chroot.
    want_force_yes: If True, we won't ask any questions--we'll just assume
        that the user really wants to kill the directory.  If False, we'll
        show UI asking the user to confirm.
  """
  if IsInsideChroot():
    Die('Please exit the chroot before trying to delete it.')

  chroot_dir = _GetChrootAbsDir(chroot_config)
  if not want_force_yes:
    sys.stderr.write('\n'
                     'Chroot is at: %s\n'
                     'Are you sure you want to delete it (YES/NO)? ' %
                     chroot_dir)
    answer = raw_input()
    if answer.lower() not in ('y', 'ye', 'yes'):
      Die("You must answer 'yes' if you want to proceed.")

  # Can pass argv and not shell=True, since no user flags.  :)
  argv = ['./make_chroot', '--chroot=%s' % chroot_dir, '--delete']

  # We'll put CWD as src/scripts when running the command.  Since everyone
  # running by hand has their cwd there, it is probably the safest.
  cwd = os.path.join(_SRCROOT_PATH, 'src', 'scripts')

  # Run it.  Pass any failures upward.
  RunCommand(argv, cwd=cwd)


def _DoWrappedChrootCommand(target_cmd, host_cmd, raw_argv, need_args=False,
                            chroot_config=None, argv=None, build_config=None):
  """Helper function for any command that is simply wrapped by chromite.

  These are commands where:
  - We parse the command line only enough to figure out what board they
    want.  All othe command line parsing is handled by the wrapped command.
    Because of this, the board name _needs_ to be specified first.
  - Everything else (arg parsing, help, etc) is handled by the wrapped command.
    The usage string will be a little messed up, but hopefully that's OK.

  Args:
    target_cmd: We'll put this at the start of argv when calling a target
        command.  We'll substiture %s with the target.
        Like - ['my_command-%s'] or ['my_command', '--board=%s']
    host_cmd: We'll put this at the start of argv when calling a host command.
        Like - ['my_command']
    raw_argv: Command line arguments, including this command's name, but not
        the chromite command name or chromite options.
    need_args: If True, we'll prompt for arguments if they weren't specified.
        This makes the most sense when someone runs chromite with no arguments
        and then walks through the menus.  It's not ideal, but less sucky than
        just quitting.
    chroot_config: A SafeConfigParser for the chroot config; or None chromite
        was called from within the chroot.
    argv: None when called normally, but contains argv with board stripped off
        if we call ourselves with _DoEnterChroot().
    build_config: None when called normally, but contains the SafeConfigParser
        for the build config if we call ourselves with _DoEnterChroot().  Note
        that even when called through _DoEnterChroot(), could still be None
        if user chose 'HOST' as the target.
  """
  # If we didn't get called through EnterChroot, we need to read the build
  # config.
  if argv is None:
    # We look for the build config without calling OptionParser.  This means
    # that the board _needs_ to be first (if it's specified) and all options
    # will be passed straight to our subcommand.
    argv, build_config = _GetBuildConfigFromArgs(raw_argv[1:])

  # Enter the chroot if needed...
  if not IsInsideChroot():
    _DoEnterChroot(chroot_config, _DoWrappedChrootCommand, target_cmd, host_cmd,
                   raw_argv, need_args=need_args, argv=argv,
                   build_config=build_config)
  else:
    # We'll put CWD as src/scripts when running the command.  Since everyone
    # running by hand has their cwd there, it is probably the safest.
    cwd = os.path.join(_SRCROOT_PATH, 'src', 'scripts')

    # Get command to call.  If build_config is None, it means host.
    if build_config is None:
      argv_prefix = host_cmd
    else:
      # Make argv_prefix w/ target.
      target_name = build_config.get('BUILD', 'target')
      argv_prefix = [arg % target_name for arg in target_cmd]

    # Not a great way to to specify arguments, but works for now...  Wrapped
    # commands are not wonderful interfaces anyway...
    if need_args and not argv:
      while True:
        sys.stderr.write('arg %d (blank to exit): ' % (len(argv)+1))
        arg = raw_input()
        if not arg:
          break
        argv.append(arg)

    # Add the prefix...
    argv = argv_prefix + argv

    # Run, ignoring errors since some commands (ahem, cros_workon) seem to
    # return errors from things like --help.
    RunCommand(argv, cwd=cwd, ignore_sigint=True, error_ok=True)


def _CmdBuild(raw_argv, chroot_config=None, loaded_config=False,
              build_config=None):
  """Build the chroot (if needed), the packages for a target, and the image.

  Args:
    raw_argv: Command line arguments, including this command's name, but not
        the chromite command name or chromite options.
    chroot_config: A SafeConfigParser for the chroot config; or None chromite
        was called from within the chroot.
    loaded_config: If True, we've already loaded the config.
    build_config: None when called normally, but contains the SafeConfigParser
        for the build config if we call ourselves with _DoEnterChroot().  Note
        that even when called through _DoEnterChroot(), could still be None
        if user chose 'HOST' as the target.
  """
  # Parse options for command...
  usage_str = ('usage: %%prog [chromite_options] %s [options] [target]' %
               raw_argv[0])
  parser = optparse.OptionParser(usage=usage_str)
  parser.add_option('--clean', default=False, action='store_true',
                    help='Clean before building.')
  (options, argv) = parser.parse_args(raw_argv[1:])

  # Load the build config if needed...
  if not loaded_config:
    argv, build_config = _GetBuildConfigFromArgs(argv)
    if argv:
      Die('Unknown arguments: %s' % ' '.join(argv))

  if not IsInsideChroot():
    # Note: we only want to clean the chroot if they do --clean and have the
    # host target.  If they do --clean and have a board target, it means
    # that they just want to clean the board...
    want_clean_chroot = options.clean and build_config is None

    _DoMakeChroot(chroot_config, want_clean_chroot)

    if build_config is not None:
      _DoEnterChroot(chroot_config, _CmdBuild, raw_argv,
                     build_config=build_config, loaded_config=True)

    Info('Done building.')
  else:
    if build_config is None:
      Die("You can't build the host chroot from within the chroot.")

    _DoSetupBoard(build_config, options.clean)
    _DoBuildPackages(build_config)
    _DoBuildImage(build_config)


def _CmdWorkon(raw_argv, *args, **kwargs):
  """Run cros_workon.

  This is just a wrapped command.

  Args:
    raw_argv: Command line arguments, including this command's name, but not
        the chromite command name or chromite options.
    args: The rest of the positional arguments.  See _DoWrappedChrootCommand.
    kwargs: The keyword arguments.  See _DoWrappedChrootCommand.
  """
  # Slight optimization, just since I do this all the time...
  if len(raw_argv) >= 2:
    if raw_argv[1] in ('start', 'stop', 'list', 'list-all', 'iterate'):
      Warn('OOPS, looks like you forgot a board name. Pick one.')
      raw_argv = raw_argv[:1] + [''] + raw_argv[1:]

  # Note that host version uses "./", since it's in src/scripts and not in the
  # path...
  _DoWrappedChrootCommand(['cros_workon-%s'], ['./cros_workon', '--host'],
                          raw_argv, need_args=True, *args, **kwargs)


def _CmdEbuild(raw_argv, *args, **kwargs):
  """Run ebuild.

  This is just a wrapped command.

  Args:
    raw_argv: Command line arguments, including this command's name, but not
        the chromite command name or chromite options.
    args: The rest of the positional arguments.  See _DoWrappedChrootCommand.
    kwargs: The keyword arguments.  See _DoWrappedChrootCommand.
  """
  _DoWrappedChrootCommand(['ebuild-%s'], ['ebuild'],
                          raw_argv, need_args=True, *args, **kwargs)


def _CmdEmerge(raw_argv, *args, **kwargs):
  """Run emerge.

  This is just a wrapped command.

  Args:
    raw_argv: Command line arguments, including this command's name, but not
        the chromite command name or chromite options.
    args: The rest of the positional arguments.  See _DoWrappedChrootCommand.
    kwargs: The keyword arguments.  See _DoWrappedChrootCommand.
  """
  _DoWrappedChrootCommand(['emerge-%s'], ['emerge'],
                          raw_argv, need_args=True, *args, **kwargs)


def _CmdEquery(raw_argv, *args, **kwargs):
  """Run equery.

  This is just a wrapped command.

  Args:
    raw_argv: Command line arguments, including this command's name, but not
        the chromite command name or chromite options.
    args: The rest of the positional arguments.  See _DoWrappedChrootCommand.
    kwargs: The keyword arguments.  See _DoWrappedChrootCommand.
  """
  _DoWrappedChrootCommand(['equery-%s'], ['equery'],
                          raw_argv, need_args=True, *args, **kwargs)


def _CmdPortageq(raw_argv, *args, **kwargs):
  """Run portageq.

  This is just a wrapped command.

  Args:
    raw_argv: Command line arguments, including this command's name, but not
        the chromite command name or chromite options.
    args: The rest of the positional arguments.  See _DoWrappedChrootCommand.
    kwargs: The keyword arguments.  See _DoWrappedChrootCommand.
  """
  _DoWrappedChrootCommand(['portageq-%s'], ['portageq'],
                          raw_argv, need_args=True, *args, **kwargs)


def _CmdShell(raw_argv, chroot_config=None):
  """Start a shell in the chroot.

  This can either just start a simple interactive shell, it can be used to
  run an arbirtary command inside the chroot and then exit.

  Args:
    raw_argv: Command line arguments, including this command's name, but not
        the chromite command name or chromite options.
    chroot_config: A SafeConfigParser for the chroot config; or None chromite
        was called from within the chroot.
  """
  # Parse options for command...
  # ...note that OptionParser will eat the '--' if it's there, which is what
  # we want..
  usage_str = ('usage: %%prog [chromite_options] %s [options] [VAR=value] '
               '[-- command [arg1] [arg2] ...]') % raw_argv[0]
  parser = optparse.OptionParser(usage=usage_str)
  (_, argv) = parser.parse_args(raw_argv[1:])

  # Enter the chroot if needed...
  if not IsInsideChroot():
    _DoEnterChroot(chroot_config, _CmdShell, raw_argv)
  else:
    # We'll put CWD as src/scripts when running the command.  Since everyone
    # running by hand has their cwd there, it is probably the safest.
    cwd = os.path.join(_SRCROOT_PATH, 'src', 'scripts')

    # By default, no special environment...
    env = None

    if not argv:
      # If no arguments, we'll just start bash.
      argv = ['bash']
    else:
      # Parse the command line, looking at the beginning for VAR=value type
      # statements.  I couldn't figure out a way to get bash to do this for me.
      user_env, argv = _SplitEnvFromArgs(argv)
      if not argv:
        Die('No command specified')

      # If there was some environment, use it to override the standard
      # environment.
      if user_env:
        env = dict(os.environ)
        env.update(user_env)

    # Don't show anything special for errors; we'll let the shell report them.
    RunCommand(argv, cwd=cwd, env=env, error_ok=True, ignore_sigint=True)


def _CmdClean(raw_argv, chroot_config=None):
  """Clean out built packages for a target; if target is host, deletes chroot.

  Args:
    raw_argv: Command line arguments, including this command's name, but not
        the chromite command name or chromite options.
    chroot_config: A SafeConfigParser for the chroot config; or None chromite
        was called from within the chroot.
  """
  # Parse options for command...
  usage_str = ('usage: %%prog [chromite_options] %s [options] [target]' %
               raw_argv[0])
  parser = optparse.OptionParser(usage=usage_str)
  parser.add_option('-y', '--yes', default=False, action='store_true',
                    help='Answer "YES" to "are you sure?" questions.')
  (options, argv) = parser.parse_args(raw_argv[1:])

  # Make sure the chroot exists first, before possibly prompting for board...
  # ...not really required, but nice for the user...
  if not IsInsideChroot():
    if not _DoesChrootExist(chroot_config):
      Die("Nothing to clean: the chroot doesn't exist.\n  %s" %
          _GetChrootAbsDir(chroot_config))

  # Load the build config...
  argv, build_config = _GetBuildConfigFromArgs(argv)
  if argv:
    Die('Unknown arguments: %s' % ' '.join(argv))

  # If they do clean host, we'll delete the whole chroot
  if build_config is None:
    _DoDistClean(chroot_config, options.yes)
  else:
    _DoClean(chroot_config, build_config, options.yes)


def main():
  # TODO(dianders): Make help a little better.  Specifically:
  # 1. Add a command called 'help'
  # 2. Make the help string below include command list and descriptions (like
  #    the menu, but without being interactive).
  # 3. Make "help command" and "--help command" equivalent to "command --help".
  help_str = (
      """Usage: %(prog)s [chromite_options] [cmd [args]]\n"""
      """\n"""
      """The chromite script is a wrapper to make it easy to do various\n"""
      """build tasks.  For a list of commands, run without any arguments.\n"""
      """\n"""
      """Options:\n"""
      """  -h, --help            show this help message and exit\n"""
  ) % {'prog': os.path.basename(sys.argv[0])}
  if not IsInsideChroot():
    help_str += (
        """  --chroot=CHROOT_NAME  Chroot spec to use. Can be an absolute\n"""
        """                        path to a spec file or a substring of a\n"""
        """                        chroot spec name (without .spec suffix)\n"""
    )

  # We don't use OptionParser here, since options for different subcommands are
  # so different.  We just look for the chromite options here...
  if sys.argv[1:2] == ['--help']:
    print help_str
    sys.exit(0)
  elif sys.argv[1:2] == ['--resume-state']:
    # Internal mechanism (not documented to users) to resume in the chroot.
    # ...actual resume state file is passed in sys.argv[2] for simplicity...
    assert len(sys.argv) == 3, 'Resume State not passed properly.'
    fn, args, kwargs = cPickle.load(open(sys.argv[2], 'rb'))
    fn(*args, **kwargs)
  else:
    # Start by skipping argv[0]
    argv = sys.argv[1:]

    # Look for special "--chroot" argument to allow for alternate chroots
    if not IsInsideChroot():
      # Default chroot name...
      chroot_name = 'chroot'

      # Get chroot spec name if specified; trim argv down if needed...
      if argv:
        if argv[0].startswith('--chroot='):
          _, chroot_name = argv[0].split('=', 2)
          argv = argv[1:]
        elif argv[0] == '--chroot':
          if len(argv) < 2:
            Die('Chroot not specified.')

          chroot_name = argv[1]
          argv = argv[2:]

      chroot_spec_path = _FindSpec(chroot_name, spec_type=_CHROOT_SPEC_TYPE)

      Info('Using chroot "%s"' % os.path.relpath(chroot_spec_path))

      chroot_config = _ReadConfig(chroot_spec_path)
    else:
      # Already in the chroot; no need to get config...
      chroot_config = None

    # Get command and arguments
    if argv:
      chromite_cmd = argv[0].lower()
      argv = argv[1:]
    else:
      chromite_cmd = ''

    # Validate the chromite_cmd, popping a menu if needed.
    chromite_cmd = _FindCommand(chromite_cmd)

    # Finally, call the function w/ standard argv.
    cmd_fn = eval(_COMMAND_HANDLERS[_COMMAND_STRS.index(chromite_cmd)])
    cmd_fn([chromite_cmd] + argv, chroot_config=chroot_config)


if __name__ == '__main__':
  main()
