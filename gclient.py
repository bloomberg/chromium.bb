#!/usr/bin/python
#
# Copyright 2008 Google Inc.  All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""A wrapper script to manage a set of client modules in different SCM.

This script is intended to be used to help basic management of client
program sources residing in one or more Subversion modules, along with
other modules it depends on, also in Subversion, but possibly on
multiple respositories, making a wrapper system apparently necessary.

Files
  .gclient      : Current client configuration, written by 'config' command.
                  Format is a Python script defining 'solutions', a list whose
                  entries each are maps binding the strings "name" and "url"
                  to strings specifying the name and location of the client
                  module, as well as "custom_deps" to a map similar to the DEPS
                  file below.
  .gclient_entries : A cache constructed by 'update' command.  Format is a
                  Python script defining 'entries', a list of the names
                  of all modules in the client
  <module>/DEPS : Python script defining var 'deps' as a map from each requisite
                  submodule name to a URL where it can be found (via one SCM)

Hooks
  .gclient and DEPS files may optionally contain a list named "hooks" to
  allow custom actions to be performed based on files that have changed in the
  working copy as a result of a "sync"/"update" or "revert" operation.  This
  could be prevented by using --nohooks (hooks run by default). Hooks can also
  be forced to run with the "runhooks" operation.  If "sync" is run with
  --force, all known hooks will run regardless of the state of the working
  copy.

  Each item in a "hooks" list is a dict, containing these two keys:
    "pattern"  The associated value is a string containing a regular
               expression.  When a file whose pathname matches the expression
               is checked out, updated, or reverted, the hook's "action" will
               run.
    "action"   A list describing a command to run along with its arguments, if
               any.  An action command will run at most one time per gclient
               invocation, regardless of how many files matched the pattern.
               The action is executed in the same directory as the .gclient
               file.  If the first item in the list is the string "python",
               the current Python interpreter (sys.executable) will be used
               to run the command. If the list contains string "$matching_files"
               it will be removed from the list and the list will be extended
               by the list of matching files.

  Example:
    hooks = [
      { "pattern": "\\.(gif|jpe?g|pr0n|png)$",
        "action":  ["python", "image_indexer.py", "--all"]},
    ]
"""

__author__ = "darinf@gmail.com (Darin Fisher)"
__version__ = "0.3.3"

import errno
import logging
import optparse
import os
import pprint
import re
import sys
import urlparse
import urllib

import gclient_scm
import gclient_utils

# default help text
DEFAULT_USAGE_TEXT = (
"""usage: %prog <subcommand> [options] [--] [svn options/args...]
a wrapper for managing a set of client modules in svn.
Version """ + __version__ + """

subcommands:
   cleanup
   config
   diff
   export
   pack
   revert
   status
   sync
   update
   runhooks
   revinfo

Options and extra arguments can be passed to invoked svn commands by
appending them to the command line.  Note that if the first such
appended option starts with a dash (-) then the options must be
preceded by -- to distinguish them from gclient options.

For additional help on a subcommand or examples of usage, try
   %prog help <subcommand>
   %prog help files
""")

GENERIC_UPDATE_USAGE_TEXT = (
    """Perform a checkout/update of the modules specified by the gclient
configuration; see 'help config'.  Unless --revision is specified,
then the latest revision of the root solutions is checked out, with
dependent submodule versions updated according to DEPS files.
If --revision is specified, then the given revision is used in place
of the latest, either for a single solution or for all solutions.
Unless the --force option is provided, solutions and modules whose
local revision matches the one to update (i.e., they have not changed
in the repository) are *not* modified. Unless --nohooks is provided,
the hooks are run.
This a synonym for 'gclient %(alias)s'

usage: gclient %(cmd)s [options] [--] [svn update options/args]

Valid options:
  --force             : force update even for unchanged modules
  --nohooks           : don't run the hooks after the update is complete
  --revision REV      : update/checkout all solutions with specified revision
  --revision SOLUTION@REV : update given solution to specified revision
  --deps PLATFORM(S)  : sync deps for the given platform(s), or 'all'
  --verbose           : output additional diagnostics
  --head              : update to latest revision, instead of last good revision

Examples:
  gclient %(cmd)s
      update files from SVN according to current configuration,
      *for modules which have changed since last update or sync*
  gclient %(cmd)s --force
      update files from SVN according to current configuration, for
      all modules (useful for recovering files deleted from local copy)
""")

COMMAND_USAGE_TEXT = {
    "cleanup":
    """Clean up all working copies, using 'svn cleanup' for each module.
Additional options and args may be passed to 'svn cleanup'.

usage: cleanup [options] [--] [svn cleanup args/options]

Valid options:
  --verbose           : output additional diagnostics
""",
    "config": """Create a .gclient file in the current directory; this
specifies the configuration for further commands.  After update/sync,
top-level DEPS files in each module are read to determine dependent
modules to operate on as well.  If optional [url] parameter is
provided, then configuration is read from a specified Subversion server
URL.  Otherwise, a --spec option must be provided.

usage: config [option | url] [safesync url]

Valid options:
  --spec=GCLIENT_SPEC   : contents of .gclient are read from string parameter.
                          *Note that due to Cygwin/Python brokenness, it
                          probably can't contain any newlines.*

Examples:
  gclient config https://gclient.googlecode.com/svn/trunk/gclient
      configure a new client to check out gclient.py tool sources
  gclient config --spec='solutions=[{"name":"gclient","""
    '"url":"https://gclient.googlecode.com/svn/trunk/gclient",'
    '"custom_deps":{}}]',
    "diff": """Display the differences between two revisions of modules.
(Does 'svn diff' for each checked out module and dependences.)
Additional args and options to 'svn diff' can be passed after
gclient options.

usage: diff [options] [--] [svn args/options]

Valid options:
  --verbose            : output additional diagnostics

Examples:
  gclient diff
      simple 'svn diff' for configured client and dependences
  gclient diff -- -x -b
      use 'svn diff -x -b' to suppress whitespace-only differences
  gclient diff -- -r HEAD -x -b
      diff versus the latest version of each module
""",
    "export":
    """Wrapper for svn export for all managed directories
""",
    "pack":

    """Generate a patch which can be applied at the root of the tree.
Internally, runs 'svn diff' on each checked out module and
dependencies, and performs minimal postprocessing of the output. The
resulting patch is printed to stdout and can be applied to a freshly
checked out tree via 'patch -p0 < patchfile'. Additional args and
options to 'svn diff' can be passed after gclient options.

usage: pack [options] [--] [svn args/options]

Valid options:
  --verbose            : output additional diagnostics

Examples:
  gclient pack > patch.txt
      generate simple patch for configured client and dependences
  gclient pack -- -x -b > patch.txt
      generate patch using 'svn diff -x -b' to suppress
      whitespace-only differences
  gclient pack -- -r HEAD -x -b > patch.txt
      generate patch, diffing each file versus the latest version of
      each module
""",
    "revert":
    """Revert every file in every managed directory in the client view.

usage: revert
""",
    "status":
    """Show the status of client and dependent modules, using 'svn diff'
for each module.  Additional options and args may be passed to 'svn diff'.

usage: status [options] [--] [svn diff args/options]

Valid options:
  --verbose           : output additional diagnostics
  --nohooks           : don't run the hooks after the update is complete
""",
    "sync": GENERIC_UPDATE_USAGE_TEXT % {"cmd": "sync", "alias": "update"},
    "update": GENERIC_UPDATE_USAGE_TEXT % {"cmd": "update", "alias": "sync"},
    "help": """Describe the usage of this program or its subcommands.

usage: help [options] [subcommand]

Valid options:
  --verbose           : output additional diagnostics
""",
    "runhooks":
    """Runs hooks for files that have been modified in the local working copy,
according to 'svn status'. Implies --force.

usage: runhooks [options]

Valid options:
  --verbose           : output additional diagnostics
""",
    "revinfo":
    """Outputs source path, server URL and revision information for every
dependency in all solutions (no local checkout required).

usage: revinfo [options]
""",
}

DEFAULT_CLIENT_FILE_TEXT = ("""\
# An element of this array (a "solution") describes a repository directory
# that will be checked out into your working copy.  Each solution may
# optionally define additional dependencies (via its DEPS file) to be
# checked out alongside the solution's directory.  A solution may also
# specify custom dependencies (via the "custom_deps" property) that
# override or augment the dependencies specified by the DEPS file.
# If a "safesync_url" is specified, it is assumed to reference the location of
# a text file which contains nothing but the last known good SCM revision to
# sync against. It is fetched if specified and used unless --head is passed

solutions = [
  { "name"        : "%(solution_name)s",
    "url"         : "%(solution_url)s",
    "custom_deps" : {
      # To use the trunk of a component instead of what's in DEPS:
      #"component": "https://svnserver/component/trunk/",
      # To exclude a component from your working copy:
      #"data/really_large_component": None,
    },
    "safesync_url": "%(safesync_url)s"
  },
]
""")


## GClient implementation.


class GClient(object):
  """Object that represent a gclient checkout."""

  supported_commands = [
    'cleanup', 'diff', 'export', 'pack', 'revert', 'status', 'update',
    'runhooks'
  ]

  def __init__(self, root_dir, options):
    self._root_dir = root_dir
    self._options = options
    self._config_content = None
    self._config_dict = {}
    self._deps_hooks = []

  def SetConfig(self, content):
    self._config_dict = {}
    self._config_content = content
    try:
      exec(content, self._config_dict)
    except SyntaxError, e:
      try:
        __pychecker__ = 'no-objattrs'
        # Try to construct a human readable error message
        error_message = [
            'There is a syntax error in your configuration file.',
            'Line #%s, character %s:' % (e.lineno, e.offset),
            '"%s"' % re.sub(r'[\r\n]*$', '', e.text) ]
      except:
        # Something went wrong, re-raise the original exception
        raise e
      else:
        # Raise a new exception with the human readable message:
        raise gclient_utils.Error('\n'.join(error_message))

  def SaveConfig(self):
    gclient_utils.FileWrite(os.path.join(self._root_dir,
                                         self._options.config_filename),
                            self._config_content)

  def _LoadConfig(self):
    client_source = gclient_utils.FileRead(
        os.path.join(self._root_dir, self._options.config_filename))
    self.SetConfig(client_source)

  def ConfigContent(self):
    return self._config_content

  def GetVar(self, key, default=None):
    return self._config_dict.get(key, default)

  @staticmethod
  def LoadCurrentConfig(options, from_dir=None):
    """Searches for and loads a .gclient file relative to the current working
    dir.

    Returns:
      A dict representing the contents of the .gclient file or an empty dict if
      the .gclient file doesn't exist.
    """
    if not from_dir:
      from_dir = os.curdir
    path = os.path.realpath(from_dir)
    while not os.path.exists(os.path.join(path, options.config_filename)):
      next = os.path.split(path)
      if not next[1]:
        return None
      path = next[0]
    client = GClient(path, options)
    client._LoadConfig()
    return client

  def SetDefaultConfig(self, solution_name, solution_url, safesync_url):
    self.SetConfig(DEFAULT_CLIENT_FILE_TEXT % {
      'solution_name': solution_name,
      'solution_url': solution_url,
      'safesync_url' : safesync_url,
    })

  def _SaveEntries(self, entries):
    """Creates a .gclient_entries file to record the list of unique checkouts.

    The .gclient_entries file lives in the same directory as .gclient.

    Args:
      entries: A sequence of solution names.
    """
    text = "entries = \\\n" + pprint.pformat(entries, 2) + '\n'
    file_path = os.path.join(self._root_dir, self._options.entries_filename)
    gclient_utils.FileWrite(file_path, text)

  def _ReadEntries(self):
    """Read the .gclient_entries file for the given client.

    Args:
      client: The client for which the entries file should be read.

    Returns:
      A sequence of solution names, which will be empty if there is the
      entries file hasn't been created yet.
    """
    scope = {}
    filename = os.path.join(self._root_dir, self._options.entries_filename)
    if not os.path.exists(filename):
      return []
    exec(gclient_utils.FileRead(filename), scope)
    return scope["entries"]

  class FromImpl:
    """Used to implement the From syntax."""

    def __init__(self, module_name):
      self.module_name = module_name

    def __str__(self):
      return 'From("%s")' % self.module_name

  class _VarImpl:
    def __init__(self, custom_vars, local_scope):
      self._custom_vars = custom_vars
      self._local_scope = local_scope

    def Lookup(self, var_name):
      """Implements the Var syntax."""
      if var_name in self._custom_vars:
        return self._custom_vars[var_name]
      elif var_name in self._local_scope.get("vars", {}):
        return self._local_scope["vars"][var_name]
      raise gclient_utils.Error("Var is not defined: %s" % var_name)

  def _ParseSolutionDeps(self, solution_name, solution_deps_content,
                         custom_vars):
    """Parses the DEPS file for the specified solution.

    Args:
      solution_name: The name of the solution to query.
      solution_deps_content: Content of the DEPS file for the solution
      custom_vars: A dict of vars to override any vars defined in the DEPS file.

    Returns:
      A dict mapping module names (as relative paths) to URLs or an empty
      dict if the solution does not have a DEPS file.
    """
    # Skip empty
    if not solution_deps_content:
      return {}
    # Eval the content
    local_scope = {}
    var = self._VarImpl(custom_vars, local_scope)
    global_scope = {"From": self.FromImpl, "Var": var.Lookup, "deps_os": {}}
    exec(solution_deps_content, global_scope, local_scope)
    deps = local_scope.get("deps", {})

    # load os specific dependencies if defined.  these dependencies may
    # override or extend the values defined by the 'deps' member.
    if "deps_os" in local_scope:
      deps_os_choices = {
          "win32": "win",
          "win": "win",
          "cygwin": "win",
          "darwin": "mac",
          "mac": "mac",
          "unix": "unix",
          "linux": "unix",
          "linux2": "unix",
         }

      if self._options.deps_os is not None:
        deps_to_include = self._options.deps_os.split(",")
        if "all" in deps_to_include:
          deps_to_include = deps_os_choices.values()
      else:
        deps_to_include = [deps_os_choices.get(self._options.platform, "unix")]

      deps_to_include = set(deps_to_include)
      for deps_os_key in deps_to_include:
        os_deps = local_scope["deps_os"].get(deps_os_key, {})
        if len(deps_to_include) > 1:
          # Ignore any overrides when including deps for more than one
          # platform, so we collect the broadest set of dependencies available.
          # We may end up with the wrong revision of something for our
          # platform, but this is the best we can do.
          deps.update([x for x in os_deps.items() if not x[0] in deps])
        else:
          deps.update(os_deps)

    if 'hooks' in local_scope:
      self._deps_hooks.extend(local_scope['hooks'])

    # If use_relative_paths is set in the DEPS file, regenerate
    # the dictionary using paths relative to the directory containing
    # the DEPS file.
    if local_scope.get('use_relative_paths'):
      rel_deps = {}
      for d, url in deps.items():
        # normpath is required to allow DEPS to use .. in their
        # dependency local path.
        rel_deps[os.path.normpath(os.path.join(solution_name, d))] = url
      return rel_deps
    else:
      return deps

  def _ParseAllDeps(self, solution_urls, solution_deps_content):
    """Parse the complete list of dependencies for the client.

    Args:
      solution_urls: A dict mapping module names (as relative paths) to URLs
        corresponding to the solutions specified by the client.  This parameter
        is passed as an optimization.
      solution_deps_content: A dict mapping module names to the content
        of their DEPS files

    Returns:
      A dict mapping module names (as relative paths) to URLs corresponding
      to the entire set of dependencies to checkout for the given client.

    Raises:
      Error: If a dependency conflicts with another dependency or of a solution.
    """
    deps = {}
    for solution in self.GetVar("solutions"):
      custom_vars = solution.get("custom_vars", {})
      solution_deps = self._ParseSolutionDeps(
                              solution["name"],
                              solution_deps_content[solution["name"]],
                              custom_vars)

      # If a line is in custom_deps, but not in the solution, we want to append
      # this line to the solution.
      if "custom_deps" in solution:
        for d in solution["custom_deps"]:
          if d not in solution_deps:
            solution_deps[d] = solution["custom_deps"][d]

      for d in solution_deps:
        if "custom_deps" in solution and d in solution["custom_deps"]:
          # Dependency is overriden.
          url = solution["custom_deps"][d]
          if url is None:
            continue
        else:
          url = solution_deps[d]
          # if we have a From reference dependent on another solution, then
          # just skip the From reference. When we pull deps for the solution,
          # we will take care of this dependency.
          #
          # If multiple solutions all have the same From reference, then we
          # should only add one to our list of dependencies.
          if type(url) != str:
            if url.module_name in solution_urls:
              # Already parsed.
              continue
            if d in deps and type(deps[d]) != str:
              if url.module_name == deps[d].module_name:
                continue
          else:
            parsed_url = urlparse.urlparse(url)
            scheme = parsed_url[0]
            if not scheme:
              # A relative url. Fetch the real base.
              path = parsed_url[2]
              if path[0] != "/":
                raise gclient_utils.Error(
                    "relative DEPS entry \"%s\" must begin with a slash" % d)
              # Create a scm just to query the full url.
              scm = gclient_scm.CreateSCM(solution["url"], self._root_dir,
                                           None)
              url = scm.FullUrlForRelativeUrl(url)
        if d in deps and deps[d] != url:
          raise gclient_utils.Error(
              "Solutions have conflicting versions of dependency \"%s\"" % d)
        if d in solution_urls and solution_urls[d] != url:
          raise gclient_utils.Error(
              "Dependency \"%s\" conflicts with specified solution" % d)
        # Grab the dependency.
        deps[d] = url
    return deps

  def _RunHookAction(self, hook_dict, matching_file_list):
    """Runs the action from a single hook.
    """
    command = hook_dict['action'][:]
    if command[0] == 'python':
      # If the hook specified "python" as the first item, the action is a
      # Python script.  Run it by starting a new copy of the same
      # interpreter.
      command[0] = sys.executable

    if '$matching_files' in command:
      splice_index = command.index('$matching_files')
      command[splice_index:splice_index + 1] = matching_file_list

    # Use a discrete exit status code of 2 to indicate that a hook action
    # failed.  Users of this script may wish to treat hook action failures
    # differently from VC failures.
    gclient_utils.SubprocessCall(command, self._root_dir, fail_status=2)

  def _RunHooks(self, command, file_list, is_using_git):
    """Evaluates all hooks, running actions as needed.
    """
    # Hooks only run for these command types.
    if not command in ('update', 'revert', 'runhooks'):
      return

    # Hooks only run when --nohooks is not specified
    if self._options.nohooks:
      return

    # Get any hooks from the .gclient file.
    hooks = self.GetVar("hooks", [])
    # Add any hooks found in DEPS files.
    hooks.extend(self._deps_hooks)

    # If "--force" was specified, run all hooks regardless of what files have
    # changed.  If the user is using git, then we don't know what files have
    # changed so we always run all hooks.
    if self._options.force or is_using_git:
      for hook_dict in hooks:
        self._RunHookAction(hook_dict, [])
      return

    # Run hooks on the basis of whether the files from the gclient operation
    # match each hook's pattern.
    for hook_dict in hooks:
      pattern = re.compile(hook_dict['pattern'])
      matching_file_list = [f for f in file_list if pattern.search(f)]
      if matching_file_list:
        self._RunHookAction(hook_dict, matching_file_list)

  def RunOnDeps(self, command, args):
    """Runs a command on each dependency in a client and its dependencies.

    The module's dependencies are specified in its top-level DEPS files.

    Args:
      command: The command to use (e.g., 'status' or 'diff')
      args: list of str - extra arguments to add to the command line.

    Raises:
      Error: If the client has conflicting entries.
    """
    if not command in self.supported_commands:
      raise gclient_utils.Error("'%s' is an unsupported command" % command)

    # Check for revision overrides.
    revision_overrides = {}
    for revision in self._options.revisions:
      if revision.find("@") == -1:
        raise gclient_utils.Error(
            "Specify the full dependency when specifying a revision number.")
      revision_elem = revision.split("@")
      # Disallow conflicting revs
      if revision_overrides.has_key(revision_elem[0]) and \
         revision_overrides[revision_elem[0]] != revision_elem[1]:
        raise gclient_utils.Error(
            "Conflicting revision numbers specified.")
      revision_overrides[revision_elem[0]] = revision_elem[1]

    solutions = self.GetVar("solutions")
    if not solutions:
      raise gclient_utils.Error("No solution specified")

    # When running runhooks --force, there's no need to consult the SCM.
    # All known hooks are expected to run unconditionally regardless of working
    # copy state, so skip the SCM status check.
    run_scm = not (command == 'runhooks' and self._options.force)

    entries = {}
    entries_deps_content = {}
    file_list = []
    # Run on the base solutions first.
    for solution in solutions:
      name = solution["name"]
      deps_file = solution.get("deps_file", self._options.deps_file)
      if '/' in deps_file or '\\' in deps_file:
        raise gclient_utils.Error('deps_file name must not be a path, just a '
                                  'filename.')
      if name in entries:
        raise gclient_utils.Error("solution %s specified more than once" % name)
      url = solution["url"]
      entries[name] = url
      if run_scm and url:
        self._options.revision = revision_overrides.get(name)
        scm = gclient_scm.CreateSCM(url, self._root_dir, name)
        scm.RunCommand(command, self._options, args, file_list)
        file_list = [os.path.join(name, f.strip()) for f in file_list]
        self._options.revision = None
      try:
        deps_content = gclient_utils.FileRead(
            os.path.join(self._root_dir, name, deps_file))
      except IOError, e:
        if e.errno != errno.ENOENT:
          raise
        deps_content = ""
      entries_deps_content[name] = deps_content

    # Process the dependencies next (sort alphanumerically to ensure that
    # containing directories get populated first and for readability)
    deps = self._ParseAllDeps(entries, entries_deps_content)
    deps_to_process = deps.keys()
    deps_to_process.sort()

    # First pass for direct dependencies.
    for d in deps_to_process:
      if type(deps[d]) == str:
        url = deps[d]
        entries[d] = url
        if run_scm:
          self._options.revision = revision_overrides.get(d)
          scm = gclient_scm.CreateSCM(url, self._root_dir, d)
          scm.RunCommand(command, self._options, args, file_list)
          self._options.revision = None

    # Second pass for inherited deps (via the From keyword)
    for d in deps_to_process:
      if type(deps[d]) != str:
        filename = os.path.join(self._root_dir,
                                deps[d].module_name,
                                self._options.deps_file)
        content =  gclient_utils.FileRead(filename)
        sub_deps = self._ParseSolutionDeps(deps[d].module_name, content, {})
        url = sub_deps[d]
        entries[d] = url
        if run_scm:
          self._options.revision = revision_overrides.get(d)
          scm = gclient_scm.CreateSCM(url, self._root_dir, d)
          scm.RunCommand(command, self._options, args, file_list)
          self._options.revision = None

    # Convert all absolute paths to relative.
    for i in range(len(file_list)):
      # TODO(phajdan.jr): We should know exactly when the paths are absolute.
      # It depends on the command being executed (like runhooks vs sync).
      if not os.path.isabs(file_list[i]):
        continue

      prefix = os.path.commonprefix([self._root_dir.lower(),
                                     file_list[i].lower()])
      file_list[i] = file_list[i][len(prefix):]

      # Strip any leading path separators.
      while file_list[i].startswith('\\') or file_list[i].startswith('/'):
        file_list[i] = file_list[i][1:]

    is_using_git = gclient_utils.IsUsingGit(self._root_dir, entries.keys())
    self._RunHooks(command, file_list, is_using_git)

    if command == 'update':
      # Notify the user if there is an orphaned entry in their working copy.
      # Only delete the directory if there are no changes in it, and
      # delete_unversioned_trees is set to true.
      prev_entries = self._ReadEntries()
      for entry in prev_entries:
        # Fix path separator on Windows.
        entry_fixed = entry.replace('/', os.path.sep)
        e_dir = os.path.join(self._root_dir, entry_fixed)
        # Use entry and not entry_fixed there.
        if entry not in entries and os.path.exists(e_dir):
          modified_files = False
          if isinstance(prev_entries,list):
            # old .gclient_entries format was list, now dict
            modified_files = gclient_scm.CaptureSVNStatus(e_dir)
          else:
            file_list = []
            scm = gclient_scm.CreateSCM(prev_entries[entry], self._root_dir,
                                        entry_fixed)
            scm.status(self._options, [], file_list)
            modified_files = file_list != []
          if not self._options.delete_unversioned_trees or modified_files:
            # There are modified files in this entry. Keep warning until
            # removed.
            print(("\nWARNING: \"%s\" is no longer part of this client.  "
                   "It is recommended that you manually remove it.\n") %
                      entry_fixed)
          else:
            # Delete the entry
            print("\n________ deleting \'%s\' " +
                  "in \'%s\'") % (entry_fixed, self._root_dir)
            gclient_utils.RemoveDirectory(e_dir)
      # record the current list of entries for next time
      self._SaveEntries(entries)

  def PrintRevInfo(self):
    """Output revision info mapping for the client and its dependencies. This
    allows the capture of a overall "revision" for the source tree that can
    be used to reproduce the same tree in the future. The actual output
    contains enough information (source paths, svn server urls and revisions)
    that it can be used either to generate external svn commands (without
    gclient) or as input to gclient's --rev option (with some massaging of
    the data).

    NOTE: Unlike RunOnDeps this does not require a local checkout and is run
    on the Pulse master. It MUST NOT execute hooks.

    Raises:
      Error: If the client has conflicting entries.
    """
    # Check for revision overrides.
    revision_overrides = {}
    for revision in self._options.revisions:
      if revision.find("@") < 0:
        raise gclient_utils.Error(
            "Specify the full dependency when specifying a revision number.")
      revision_elem = revision.split("@")
      # Disallow conflicting revs
      if revision_overrides.has_key(revision_elem[0]) and \
         revision_overrides[revision_elem[0]] != revision_elem[1]:
        raise gclient_utils.Error(
            "Conflicting revision numbers specified.")
      revision_overrides[revision_elem[0]] = revision_elem[1]

    solutions = self.GetVar("solutions")
    if not solutions:
      raise gclient_utils.Error("No solution specified")

    entries = {}
    entries_deps_content = {}

    # Inner helper to generate base url and rev tuple (including honoring
    # |revision_overrides|)
    def GetURLAndRev(name, original_url):
      if original_url.find("@") < 0:
        if revision_overrides.has_key(name):
          return (original_url, revision_overrides[name])
        else:
          scm = gclient_scm.CreateSCM(solution["url"], self._root_dir, name)
          return (original_url, scm.revinfo(self._options, [], None))
      else:
        url_components = original_url.split("@")
        if revision_overrides.has_key(name):
          return (url_components[0], revision_overrides[name])
        else:
          return (url_components[0], url_components[1])

    # Run on the base solutions first.
    for solution in solutions:
      name = solution["name"]
      if name in entries:
        raise gclient_utils.Error("solution %s specified more than once" % name)
      (url, rev) = GetURLAndRev(name, solution["url"])
      entries[name] = "%s@%s" % (url, rev)
      # TODO(aharper): SVN/SCMWrapper cleanup (non-local commandset)
      entries_deps_content[name] = gclient_scm.CaptureSVN(
                                     ["cat",
                                      "%s/%s@%s" % (url,
                                                    self._options.deps_file,
                                                    rev)],
                                     os.getcwd())

    # Process the dependencies next (sort alphanumerically to ensure that
    # containing directories get populated first and for readability)
    deps = self._ParseAllDeps(entries, entries_deps_content)
    deps_to_process = deps.keys()
    deps_to_process.sort()

    # First pass for direct dependencies.
    for d in deps_to_process:
      if type(deps[d]) == str:
        (url, rev) = GetURLAndRev(d, deps[d])
        entries[d] = "%s@%s" % (url, rev)

    # Second pass for inherited deps (via the From keyword)
    for d in deps_to_process:
      if type(deps[d]) != str:
        deps_parent_url = entries[deps[d].module_name]
        if deps_parent_url.find("@") < 0:
          raise gclient_utils.Error("From %s missing revisioned url" %
                                        deps[d].module_name)
        content =  gclient_utils.FileRead(os.path.join(self._root_dir,
                                                       deps[d].module_name,
                                                       self._options.deps_file))
        sub_deps = self._ParseSolutionDeps(deps[d].module_name, content, {})
        (url, rev) = GetURLAndRev(d, sub_deps[d])
        entries[d] = "%s@%s" % (url, rev)
    print(";\n\n".join(["%s: %s" % (x, entries[x])
                        for x in sorted(entries.keys())]))


## gclient commands.


def DoCleanup(options, args):
  """Handle the cleanup subcommand.

  Raises:
    Error: if client isn't configured properly.
  """
  client = GClient.LoadCurrentConfig(options)
  if not client:
    raise gclient_utils.Error("client not configured; see 'gclient config'")
  if options.verbose:
    # Print out the .gclient file.  This is longer than if we just printed the
    # client dict, but more legible, and it might contain helpful comments.
    print(client.ConfigContent())
  return client.RunOnDeps('cleanup', args)


def DoConfig(options, args):
  """Handle the config subcommand.

  Args:
    options: If options.spec set, a string providing contents of config file.
    args: The command line args.  If spec is not set,
          then args[0] is a string URL to get for config file.

  Raises:
    Error: on usage error
  """
  if len(args) < 1 and not options.spec:
    raise gclient_utils.Error("required argument missing; see 'gclient help "
                              "config'")
  if os.path.exists(options.config_filename):
    raise gclient_utils.Error("%s file already exists in the current directory"
                                  % options.config_filename)
  client = GClient('.', options)
  if options.spec:
    client.SetConfig(options.spec)
  else:
    # TODO(darin): it would be nice to be able to specify an alternate relpath
    # for the given URL.
    base_url = args[0].rstrip('/')
    name = base_url.split("/")[-1]
    safesync_url = ""
    if len(args) > 1:
      safesync_url = args[1]
    client.SetDefaultConfig(name, base_url, safesync_url)
  client.SaveConfig()


def DoExport(options, args):
  """Handle the export subcommand.

  Raises:
    Error: on usage error
  """
  if len(args) != 1:
    raise gclient_utils.Error("Need directory name")
  client = GClient.LoadCurrentConfig(options)

  if not client:
    raise gclient_utils.Error("client not configured; see 'gclient config'")

  if options.verbose:
    # Print out the .gclient file.  This is longer than if we just printed the
    # client dict, but more legible, and it might contain helpful comments.
    print(client.ConfigContent())
  return client.RunOnDeps('export', args)

def DoHelp(options, args):
  """Handle the help subcommand giving help for another subcommand.

  Raises:
    Error: if the command is unknown.
  """
  __pychecker__ = 'unusednames=options'
  if len(args) == 1 and args[0] in COMMAND_USAGE_TEXT:
    print(COMMAND_USAGE_TEXT[args[0]])
  else:
    raise gclient_utils.Error("unknown subcommand '%s'; see 'gclient help'" %
                                  args[0])


def DoPack(options, args):
  """Handle the pack subcommand.

  Raises:
    Error: if client isn't configured properly.
  """
  client = GClient.LoadCurrentConfig(options)
  if not client:
    raise gclient_utils.Error("client not configured; see 'gclient config'")
  if options.verbose:
    # Print out the .gclient file.  This is longer than if we just printed the
    # client dict, but more legible, and it might contain helpful comments.
    print(client.ConfigContent())
  return client.RunOnDeps('pack', args)


def DoStatus(options, args):
  """Handle the status subcommand.

  Raises:
    Error: if client isn't configured properly.
  """
  client = GClient.LoadCurrentConfig(options)
  if not client:
    raise gclient_utils.Error("client not configured; see 'gclient config'")
  if options.verbose:
    # Print out the .gclient file.  This is longer than if we just printed the
    # client dict, but more legible, and it might contain helpful comments.
    print(client.ConfigContent())
  return client.RunOnDeps('status', args)


def DoUpdate(options, args):
  """Handle the update and sync subcommands.

  Raises:
    Error: if client isn't configured properly.
  """
  client = GClient.LoadCurrentConfig(options)

  if not client:
    raise gclient_utils.Error("client not configured; see 'gclient config'")

  if not options.head:
    solutions = client.GetVar('solutions')
    if solutions:
      for s in solutions:
        if s.get('safesync_url', ''):
          # rip through revisions and make sure we're not over-riding
          # something that was explicitly passed
          has_key = False
          for r in options.revisions:
            if r.split('@')[0] == s['name']:
              has_key = True
              break

          if not has_key:
            handle = urllib.urlopen(s['safesync_url'])
            rev = handle.read().strip()
            handle.close()
            if len(rev):
              options.revisions.append(s['name']+'@'+rev)

  if options.verbose:
    # Print out the .gclient file.  This is longer than if we just printed the
    # client dict, but more legible, and it might contain helpful comments.
    print(client.ConfigContent())
  return client.RunOnDeps('update', args)


def DoDiff(options, args):
  """Handle the diff subcommand.

  Raises:
    Error: if client isn't configured properly.
  """
  client = GClient.LoadCurrentConfig(options)
  if not client:
    raise gclient_utils.Error("client not configured; see 'gclient config'")
  if options.verbose:
    # Print out the .gclient file.  This is longer than if we just printed the
    # client dict, but more legible, and it might contain helpful comments.
    print(client.ConfigContent())
  return client.RunOnDeps('diff', args)


def DoRevert(options, args):
  """Handle the revert subcommand.

  Raises:
    Error: if client isn't configured properly.
  """
  client = GClient.LoadCurrentConfig(options)
  if not client:
    raise gclient_utils.Error("client not configured; see 'gclient config'")
  return client.RunOnDeps('revert', args)


def DoRunHooks(options, args):
  """Handle the runhooks subcommand.

  Raises:
    Error: if client isn't configured properly.
  """
  client = GClient.LoadCurrentConfig(options)
  if not client:
    raise gclient_utils.Error("client not configured; see 'gclient config'")
  if options.verbose:
    # Print out the .gclient file.  This is longer than if we just printed the
    # client dict, but more legible, and it might contain helpful comments.
    print(client.ConfigContent())
  options.force = True
  return client.RunOnDeps('runhooks', args)


def DoRevInfo(options, args):
  """Handle the revinfo subcommand.

  Raises:
    Error: if client isn't configured properly.
  """
  __pychecker__ = 'unusednames=args'
  client = GClient.LoadCurrentConfig(options)
  if not client:
    raise gclient_utils.Error("client not configured; see 'gclient config'")
  client.PrintRevInfo()


gclient_command_map = {
  "cleanup": DoCleanup,
  "config": DoConfig,
  "diff": DoDiff,
  "export": DoExport,
  "help": DoHelp,
  "pack": DoPack,
  "status": DoStatus,
  "sync": DoUpdate,
  "update": DoUpdate,
  "revert": DoRevert,
  "runhooks": DoRunHooks,
  "revinfo" : DoRevInfo,
}


def DispatchCommand(command, options, args, command_map=None):
  """Dispatches the appropriate subcommand based on command line arguments."""
  if command_map is None:
    command_map = gclient_command_map

  if command in command_map:
    return command_map[command](options, args)
  else:
    raise gclient_utils.Error("unknown subcommand '%s'; see 'gclient help'" %
                                  command)


def Main(argv):
  """Parse command line arguments and dispatch command."""

  option_parser = optparse.OptionParser(usage=DEFAULT_USAGE_TEXT,
                                        version=__version__)
  option_parser.disable_interspersed_args()
  option_parser.add_option("", "--force", action="store_true", default=False,
                           help=("(update/sync only) force update even "
                                 "for modules which haven't changed"))
  option_parser.add_option("", "--nohooks", action="store_true", default=False,
                           help=("(update/sync/revert only) prevent the hooks from "
                                 "running"))
  option_parser.add_option("", "--revision", action="append", dest="revisions",
                           metavar="REV", default=[],
                           help=("(update/sync only) sync to a specific "
                                 "revision, can be used multiple times for "
                                 "each solution, e.g. --revision=src@123, "
                                 "--revision=internal@32"))
  option_parser.add_option("", "--deps", default=None, dest="deps_os",
                           metavar="OS_LIST",
                           help=("(update/sync only) sync deps for the "
                                 "specified (comma-separated) platform(s); "
                                 "'all' will sync all platforms"))
  option_parser.add_option("", "--spec", default=None,
                           help=("(config only) create a gclient file "
                                 "containing the provided string"))
  option_parser.add_option("", "--verbose", action="store_true", default=False,
                           help="produce additional output for diagnostics")
  option_parser.add_option("", "--manually_grab_svn_rev", action="store_true",
                           default=False,
                           help="Skip svn up whenever possible by requesting "
                                "actual HEAD revision from the repository")
  option_parser.add_option("", "--head", action="store_true", default=False,
                           help=("skips any safesync_urls specified in "
                                 "configured solutions"))
  option_parser.add_option("", "--delete_unversioned_trees",
                           action="store_true", default=False,
                           help=("on update, delete any unexpected "
                                 "unversioned trees that are in the checkout"))

  if len(argv) < 2:
    # Users don't need to be told to use the 'help' command.
    option_parser.print_help()
    return 1
  # Add manual support for --version as first argument.
  if argv[1] == '--version':
    option_parser.print_version()
    return 0

  # Add manual support for --help as first argument.
  if argv[1] == '--help':
    argv[1] = 'help'

  command = argv[1]
  options, args = option_parser.parse_args(argv[2:])

  if len(argv) < 3 and command == "help":
    option_parser.print_help()
    return 0

  if options.verbose:
    logging.basicConfig(level=logging.DEBUG)

  # Files used for configuration and state saving.
  options.config_filename = os.environ.get("GCLIENT_FILE", ".gclient")
  options.entries_filename = ".gclient_entries"
  options.deps_file = "DEPS"

  options.platform = sys.platform
  return DispatchCommand(command, options, args)


if "__main__" == __name__:
  try:
    result = Main(sys.argv)
  except gclient_utils.Error, e:
    print >> sys.stderr, "Error: %s" % str(e)
    result = 1
  sys.exit(result)

# vim: ts=2:sw=2:tw=80:et:
