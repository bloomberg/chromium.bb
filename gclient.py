#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A wrapper script to manage a set of client modules in different SCM.

This script is intended to be used to help basic management of client
program sources residing in one or more Subversion modules and Git
repositories, along with other modules it depends on, also in Subversion or Git,
but possibly on multiple respositories, making a wrapper system apparently
necessary.

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

__version__ = "0.4"

import errno
import logging
import optparse
import os
import pprint
import re
import sys
import urlparse
import urllib

import breakpad

import gclient_scm
import gclient_utils
from third_party.repo.progress import Progress


def attr(attr, data):
  """Sets an attribute on a function."""
  def hook(fn):
    setattr(fn, attr, data)
    return fn
  return hook


## GClient implementation.


class GClient(object):
  """Object that represent a gclient checkout."""

  supported_commands = [
    'cleanup', 'diff', 'export', 'pack', 'revert', 'status', 'update',
    'runhooks'
  ]

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

  DEPS_FILE = 'DEPS'

  DEFAULT_CLIENT_FILE_TEXT = ("""\
solutions = [
  { "name"        : "%(solution_name)s",
    "url"         : "%(solution_url)s",
    "custom_deps" : {
    },
    "safesync_url": "%(safesync_url)s"
  },
]
""")

  DEFAULT_SNAPSHOT_SOLUTION_TEXT = ("""\
  { "name"        : "%(solution_name)s",
    "url"         : "%(solution_url)s",
    "custom_deps" : {
      %(solution_deps)s,
    },
    "safesync_url": "%(safesync_url)s"
  },
""")

  DEFAULT_SNAPSHOT_FILE_TEXT = ("""\
# Snapshot generated with gclient revinfo --snapshot
solutions = [
%(solution_list)s
]
""")

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
      split_path = os.path.split(path)
      if not split_path[1]:
        return None
      path = split_path[0]
    client = GClient(path, options)
    client._LoadConfig()
    return client

  def SetDefaultConfig(self, solution_name, solution_url, safesync_url):
    self.SetConfig(self.DEFAULT_CLIENT_FILE_TEXT % {
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
    # Sometimes pprint.pformat will use {', sometimes it'll use { ' ... It
    # makes testing a bit too fun.
    result = pprint.pformat(entries, 2)
    if result.startswith('{\''):
      result = '{ \'' + result[2:]
    text = "entries = \\\n" + result + '\n'
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

    def __init__(self, module_name, sub_target_name=None):
      """module_name is the dep module we want to include from.  It can also be
      the name of a subdirectory to include from.

      sub_target_name is an optional parameter if the module name in the other
      DEPS file is different. E.g., you might want to map src/net to net."""
      self.module_name = module_name
      self.sub_target_name = sub_target_name

    def __str__(self):
      return 'From(%s, %s)' % (repr(self.module_name),
                               repr(self.sub_target_name))

    def GetUrl(self, target_name, sub_deps_base_url, root_dir, sub_deps):
      """Resolve the URL for this From entry."""
      sub_deps_target_name = target_name
      if self.sub_target_name:
        sub_deps_target_name = self.sub_target_name
      url = sub_deps[sub_deps_target_name]
      if url.startswith('/'):
        # If it's a relative URL, we need to resolve the URL relative to the
        # sub deps base URL.
        if not isinstance(sub_deps_base_url, basestring):
          sub_deps_base_url = sub_deps_base_url.GetPath()
        scm = gclient_scm.CreateSCM(sub_deps_base_url, root_dir,
                                    None)
        url = scm.FullUrlForRelativeUrl(url)
      return url

  class FileImpl:
    """Used to implement the File('') syntax which lets you sync a single file
    from an SVN repo."""

    def __init__(self, file_location):
      self.file_location = file_location

    def __str__(self):
      return 'File("%s")' % self.file_location

    def GetPath(self):
      return os.path.split(self.file_location)[0]

    def GetFilename(self):
      rev_tokens = self.file_location.split('@')
      return os.path.split(rev_tokens[0])[1]

    def GetRevision(self):
      rev_tokens = self.file_location.split('@')
      if len(rev_tokens) > 1:
        return rev_tokens[1]
      return None

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
                         custom_vars, parse_hooks):
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
    global_scope = {
      "File": self.FileImpl,
      "From": self.FromImpl,
      "Var": var.Lookup,
      "deps_os": {},
    }
    exec(solution_deps_content, global_scope, local_scope)
    deps = local_scope.get("deps", {})

    # load os specific dependencies if defined.  these dependencies may
    # override or extend the values defined by the 'deps' member.
    if "deps_os" in local_scope:
      if self._options.deps_os is not None:
        deps_to_include = self._options.deps_os.split(",")
        if "all" in deps_to_include:
          deps_to_include = list(set(self.deps_os_choices.itervalues()))
      else:
        deps_to_include = [self.deps_os_choices.get(sys.platform, "unix")]

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

    if 'hooks' in local_scope and parse_hooks:
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
                              custom_vars,
                              True)

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
          if isinstance(url, self.FromImpl):
            if url.module_name in solution_urls:
              # Already parsed.
              continue
            if d in deps and type(deps[d]) != str:
              if url.module_name == deps[d].module_name:
                continue
          elif isinstance(url, str):
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
    logging.info(hook_dict)
    logging.info(matching_file_list)
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

  def _EnforceRevisions(self, solutions):
    """Checks for revision overrides."""
    revision_overrides = {}
    if self._options.head:
      return revision_overrides
    for s in solutions:
      if not s.get('safesync_url', None):
        continue
      handle = urllib.urlopen(s['safesync_url'])
      rev = handle.read().strip()
      handle.close()
      if len(rev):
        self._options.revisions.append('%s@%s' % (s['name'], rev))
    if not self._options.revisions:
      return revision_overrides
    # --revision will take over safesync_url.
    solutions_names = [s['name'] for s in solutions]
    index = 0
    for revision in self._options.revisions:
      if not '@' in revision:
        # Support for --revision 123
        revision = '%s@%s' % (solutions_names[index], revision)
      sol, rev = revision.split("@", 1)
      if not sol in solutions_names:
        #raise gclient_utils.Error('%s is not a valid solution.' % sol)
        print >> sys.stderr, ('Please fix your script, having invalid '
                              '--revision flags will soon considered an error.')
      else:
        revision_overrides[sol] = rev
      index += 1
    return revision_overrides

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

    solutions = self.GetVar("solutions")
    if not solutions:
      raise gclient_utils.Error("No solution specified")
    revision_overrides = self._EnforceRevisions(solutions)

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
      deps_file = solution.get("deps_file", self.DEPS_FILE)
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
    if command == 'update' and not self._options.verbose:
      pm = Progress('Syncing projects', len(deps_to_process))
    for d in deps_to_process:
      if command == 'update' and not self._options.verbose:
        pm.update()
      if type(deps[d]) == str:
        url = deps[d]
        entries[d] = url
        if run_scm:
          self._options.revision = revision_overrides.get(d)
          scm = gclient_scm.CreateSCM(url, self._root_dir, d)
          scm.RunCommand(command, self._options, args, file_list)
          self._options.revision = None
      elif isinstance(deps[d], self.FileImpl):
        file_dep = deps[d]
        self._options.revision = file_dep.GetRevision()
        if run_scm:
          scm = gclient_scm.CreateSCM(file_dep.GetPath(), self._root_dir, d)
          scm.RunCommand("updatesingle", self._options,
                         args + [file_dep.GetFilename()], file_list)

    if command == 'update' and not self._options.verbose:
      pm.end()

    # Second pass for inherited deps (via the From keyword)
    for d in deps_to_process:
      if isinstance(deps[d], self.FromImpl):
        filename = os.path.join(self._root_dir,
                                deps[d].module_name,
                                self.DEPS_FILE)
        content =  gclient_utils.FileRead(filename)
        sub_deps = self._ParseSolutionDeps(deps[d].module_name, content, {},
                                           False)
        # Getting the URL from the sub_deps file can involve having to resolve
        # a File() or having to resolve a relative URL.  To resolve relative
        # URLs, we need to pass in the orignal sub deps URL.
        sub_deps_base_url = deps[deps[d].module_name]
        url = deps[d].GetUrl(d, sub_deps_base_url, self._root_dir, sub_deps)
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
          if isinstance(prev_entries, list):
            # old .gclient_entries format was list, now dict
            modified_files = gclient_scm.scm.SVN.CaptureStatus(e_dir)
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
    return 0

  def PrintRevInfo(self):
    """Output revision info mapping for the client and its dependencies.

    This allows the capture of an overall "revision" for the source tree that
    can be used to reproduce the same tree in the future. It is only useful for
    "unpinned dependencies", i.e. DEPS/deps references without a svn revision
    number or a git hash. A git branch name isn't "pinned" since the actual
    commit can change.

    The --snapshot option allows creating a .gclient file to reproduce the tree.
    """
    solutions = self.GetVar("solutions")
    if not solutions:
      raise gclient_utils.Error("No solution specified")

    # Inner helper to generate base url and rev tuple
    def GetURLAndRev(name, original_url):
      url, _ = gclient_utils.SplitUrlRevision(original_url)
      scm = gclient_scm.CreateSCM(original_url, self._root_dir, name)
      return (url, scm.revinfo(self._options, [], None))

    # text of the snapshot gclient file
    new_gclient = ""
    # Dictionary of { path : SCM url } to ensure no duplicate solutions
    solution_names = {}
    entries = {}
    entries_deps_content = {}
    # Run on the base solutions first.
    for solution in solutions:
      # Dictionary of { path : SCM url } to describe the gclient checkout
      name = solution["name"]
      if name in solution_names:
        raise gclient_utils.Error("solution %s specified more than once" % name)
      (url, rev) = GetURLAndRev(name, solution["url"])
      entries[name] = "%s@%s" % (url, rev)
      solution_names[name] = "%s@%s" % (url, rev)
      deps_file = solution.get("deps_file", self.DEPS_FILE)
      if '/' in deps_file or '\\' in deps_file:
        raise gclient_utils.Error('deps_file name must not be a path, just a '
                                  'filename.')
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
        (url, rev) = GetURLAndRev(d, deps[d])
        entries[d] = "%s@%s" % (url, rev)

    # Second pass for inherited deps (via the From keyword)
    for d in deps_to_process:
      if isinstance(deps[d], self.FromImpl):
        deps_parent_url = entries[deps[d].module_name]
        if deps_parent_url.find("@") < 0:
          raise gclient_utils.Error("From %s missing revisioned url" %
                                        deps[d].module_name)
        content =  gclient_utils.FileRead(os.path.join(
                                            self._root_dir,
                                            deps[d].module_name,
                                            self.DEPS_FILE))
        sub_deps = self._ParseSolutionDeps(deps[d].module_name, content, {},
                                           False)
        (url, rev) = GetURLAndRev(d, sub_deps[d])
        entries[d] = "%s@%s" % (url, rev)

    # Build the snapshot configuration string
    if self._options.snapshot:
      url = entries.pop(name)
      custom_deps = ",\n      ".join(["\"%s\": \"%s\"" % (x, entries[x])
                                      for x in sorted(entries.keys())])

      new_gclient += self.DEFAULT_SNAPSHOT_SOLUTION_TEXT % {
                     'solution_name': name,
                     'solution_url': url,
                     'safesync_url' : "",
                     'solution_deps': custom_deps,
                     }
    else:
      print(";\n".join(["%s: %s" % (x, entries[x])
                       for x in sorted(entries.keys())]))

    # Print the snapshot configuration file
    if self._options.snapshot:
      config = self.DEFAULT_SNAPSHOT_FILE_TEXT % {'solution_list': new_gclient}
      snapclient = GClient(self._root_dir, self._options)
      snapclient.SetConfig(config)
      print(snapclient._config_content)


#### gclient commands.


def CMDcleanup(parser, args):
  """Cleans up all working copies.

Mostly svn-specific. Simply runs 'svn cleanup' for each module.
"""
  parser.add_option("--deps", dest="deps_os", metavar="OS_LIST",
                    help="override deps for the specified (comma-separated) "
                         "platform(s); 'all' will process all deps_os "
                         "references")
  (options, args) = parser.parse_args(args)
  client = GClient.LoadCurrentConfig(options)
  if not client:
    raise gclient_utils.Error("client not configured; see 'gclient config'")
  if options.verbose:
    # Print out the .gclient file.  This is longer than if we just printed the
    # client dict, but more legible, and it might contain helpful comments.
    print(client.ConfigContent())
  return client.RunOnDeps('cleanup', args)


@attr('usage', '[url] [safesync url]')
def CMDconfig(parser, args):
  """Create a .gclient file in the current directory.

This specifies the configuration for further commands. After update/sync,
top-level DEPS files in each module are read to determine dependent
modules to operate on as well. If optional [url] parameter is
provided, then configuration is read from a specified Subversion server
URL.
"""
  parser.add_option("--spec",
                    help="create a gclient file containing the provided "
                         "string. Due to Cygwin/Python brokenness, it "
                         "probably can't contain any newlines.")
  parser.add_option("--name",
                    help="overrides the default name for the solution")
  (options, args) = parser.parse_args(args)
  if ((options.spec and args) or len(args) > 2 or
      (not options.spec and not args)):
    parser.error('Inconsistent arguments. Use either --spec or one or 2 args')

  if os.path.exists(options.config_filename):
    raise gclient_utils.Error("%s file already exists in the current directory"
                                  % options.config_filename)
  client = GClient('.', options)
  if options.spec:
    client.SetConfig(options.spec)
  else:
    base_url = args[0].rstrip('/')
    if not options.name:
      name = base_url.split("/")[-1]
    else:
      # specify an alternate relpath for the given URL.
      name = options.name
    safesync_url = ""
    if len(args) > 1:
      safesync_url = args[1]
    client.SetDefaultConfig(name, base_url, safesync_url)
  client.SaveConfig()
  return 0


def CMDexport(parser, args):
  """Wrapper for svn export for all managed directories."""
  parser.add_option("--deps", dest="deps_os", metavar="OS_LIST",
                    help="override deps for the specified (comma-separated) "
                         "platform(s); 'all' will process all deps_os "
                         "references")
  (options, args) = parser.parse_args(args)
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


@attr('epilog', """Example:
  gclient pack > patch.txt
    generate simple patch for configured client and dependences
""")
def CMDpack(parser, args):
  """Generate a patch which can be applied at the root of the tree.

Internally, runs 'svn diff'/'git diff' on each checked out module and
dependencies, and performs minimal postprocessing of the output. The
resulting patch is printed to stdout and can be applied to a freshly
checked out tree via 'patch -p0 < patchfile'.
"""
  parser.add_option("--deps", dest="deps_os", metavar="OS_LIST",
                    help="override deps for the specified (comma-separated) "
                         "platform(s); 'all' will process all deps_os "
                         "references")
  (options, args) = parser.parse_args(args)
  client = GClient.LoadCurrentConfig(options)
  if not client:
    raise gclient_utils.Error("client not configured; see 'gclient config'")
  if options.verbose:
    # Print out the .gclient file.  This is longer than if we just printed the
    # client dict, but more legible, and it might contain helpful comments.
    print(client.ConfigContent())
  return client.RunOnDeps('pack', args)


def CMDstatus(parser, args):
  """Show modification status for every dependencies."""
  parser.add_option("--deps", dest="deps_os", metavar="OS_LIST",
                    help="override deps for the specified (comma-separated) "
                         "platform(s); 'all' will process all deps_os "
                         "references")
  (options, args) = parser.parse_args(args)
  client = GClient.LoadCurrentConfig(options)
  if not client:
    raise gclient_utils.Error("client not configured; see 'gclient config'")
  if options.verbose:
    # Print out the .gclient file.  This is longer than if we just printed the
    # client dict, but more legible, and it might contain helpful comments.
    print(client.ConfigContent())
  return client.RunOnDeps('status', args)


@attr('epilog', """Examples:
  gclient sync
      update files from SCM according to current configuration,
      *for modules which have changed since last update or sync*
  gclient sync --force
      update files from SCM according to current configuration, for
      all modules (useful for recovering files deleted from local copy)
  gclient sync --revision src@31000
      update src directory to r31000
""")
def CMDsync(parser, args):
  """Checkout/update all modules."""
  parser.add_option("--force", action="store_true",
                    help="force update even for unchanged modules")
  parser.add_option("--nohooks", action="store_true",
                    help="don't run hooks after the update is complete")
  parser.add_option("-r", "--revision", action="append",
                    dest="revisions", metavar="REV", default=[],
                    help="Enforces revision/hash for the solutions with the "
                         "format src@rev. The src@ part is optional and can be "
                         "skipped. -r can be used multiple times when .gclient "
                         "has multiple solutions configured and will work even "
                         "if the src@ part is skipped.")
  parser.add_option("--head", action="store_true",
                    help="skips any safesync_urls specified in "
                         "configured solutions and sync to head instead")
  parser.add_option("--delete_unversioned_trees", action="store_true",
                    help="delete any unexpected unversioned trees "
                         "that are in the checkout")
  parser.add_option("--reset", action="store_true",
                    help="resets any local changes before updating (git only)")
  parser.add_option("--deps", dest="deps_os", metavar="OS_LIST",
                    help="override deps for the specified (comma-separated) "
                         "platform(s); 'all' will process all deps_os "
                         "references")
  parser.add_option("--manually_grab_svn_rev", action="store_true",
                    help="Skip svn up whenever possible by requesting "
                         "actual HEAD revision from the repository")
  (options, args) = parser.parse_args(args)
  client = GClient.LoadCurrentConfig(options)

  if not client:
    raise gclient_utils.Error("client not configured; see 'gclient config'")

  if options.revisions and options.head:
    # TODO(maruel): Make it a parser.error if it doesn't break any builder.
    print("Warning: you cannot use both --head and --revision")

  if options.verbose:
    # Print out the .gclient file.  This is longer than if we just printed the
    # client dict, but more legible, and it might contain helpful comments.
    print(client.ConfigContent())
  return client.RunOnDeps('update', args)


def CMDupdate(parser, args):
  """Alias for the sync command. Deprecated."""
  return CMDsync(parser, args)

def CMDdiff(parser, args):
  """Displays local diff for every dependencies."""
  parser.add_option("--deps", dest="deps_os", metavar="OS_LIST",
                    help="override deps for the specified (comma-separated) "
                         "platform(s); 'all' will process all deps_os "
                         "references")
  (options, args) = parser.parse_args(args)
  client = GClient.LoadCurrentConfig(options)
  if not client:
    raise gclient_utils.Error("client not configured; see 'gclient config'")
  if options.verbose:
    # Print out the .gclient file.  This is longer than if we just printed the
    # client dict, but more legible, and it might contain helpful comments.
    print(client.ConfigContent())
  return client.RunOnDeps('diff', args)


def CMDrevert(parser, args):
  """Revert all modifications in every dependencies."""
  parser.add_option("--deps", dest="deps_os", metavar="OS_LIST",
                    help="override deps for the specified (comma-separated) "
                         "platform(s); 'all' will process all deps_os "
                         "references")
  parser.add_option("--nohooks", action="store_true",
                    help="don't run hooks after the revert is complete")
  (options, args) = parser.parse_args(args)
  # --force is implied.
  options.force = True
  client = GClient.LoadCurrentConfig(options)
  if not client:
    raise gclient_utils.Error("client not configured; see 'gclient config'")
  return client.RunOnDeps('revert', args)


def CMDrunhooks(parser, args):
  """Runs hooks for files that have been modified in the local working copy."""
  parser.add_option("--deps", dest="deps_os", metavar="OS_LIST",
                    help="override deps for the specified (comma-separated) "
                         "platform(s); 'all' will process all deps_os "
                         "references")
  parser.add_option("--force", action="store_true", default=True,
                    help="Deprecated. No effect.")
  (options, args) = parser.parse_args(args)
  client = GClient.LoadCurrentConfig(options)
  if not client:
    raise gclient_utils.Error("client not configured; see 'gclient config'")
  if options.verbose:
    # Print out the .gclient file.  This is longer than if we just printed the
    # client dict, but more legible, and it might contain helpful comments.
    print(client.ConfigContent())
  options.force = True
  options.nohooks = False
  return client.RunOnDeps('runhooks', args)


def CMDrevinfo(parser, args):
  """Outputs details for every dependencies."""
  parser.add_option("--deps", dest="deps_os", metavar="OS_LIST",
                    help="override deps for the specified (comma-separated) "
                         "platform(s); 'all' will process all deps_os "
                         "references")
  parser.add_option("--snapshot", action="store_true",
                    help="create a snapshot file of the current "
                         "version of all repositories")
  (options, args) = parser.parse_args(args)
  client = GClient.LoadCurrentConfig(options)
  if not client:
    raise gclient_utils.Error("client not configured; see 'gclient config'")
  client.PrintRevInfo()
  return 0


def Command(name):
  return getattr(sys.modules[__name__], 'CMD' + name, None)


def CMDhelp(parser, args):
  """Prints list of commands or help for a specific command."""
  (options, args) = parser.parse_args(args)
  if len(args) == 1:
    return Main(args + ['--help'])
  parser.print_help()
  return 0


def GenUsage(parser, command):
  """Modify an OptParse object with the function's documentation."""
  obj = Command(command)
  if command == 'help':
    command = '<command>'
  # OptParser.description prefer nicely non-formatted strings.
  parser.description = re.sub('[\r\n ]{2,}', ' ', obj.__doc__)
  usage = getattr(obj, 'usage', '')
  parser.set_usage('%%prog %s [options] %s' % (command, usage))
  parser.epilog = getattr(obj, 'epilog', None)


def Main(argv):
  """Doesn't parse the arguments here, just find the right subcommand to
  execute."""
  # Do it late so all commands are listed.
  CMDhelp.usage = ('\n\nCommands are:\n' + '\n'.join([
      '  %-10s %s' % (fn[3:], Command(fn[3:]).__doc__.split('\n')[0].strip())
      for fn in dir(sys.modules[__name__]) if fn.startswith('CMD')]))
  parser = optparse.OptionParser(version='%prog ' + __version__)
  parser.add_option("-v", "--verbose", action="count", default=0,
                    help="Produces additional output for diagnostics. Can be "
                         "used up to three times for more logging info.")
  parser.add_option("--gclientfile", metavar="FILENAME", dest="config_filename",
                    default=os.environ.get("GCLIENT_FILE", ".gclient"),
                    help="Specify an alternate .gclient file")
  # Integrate standard options processing.
  old_parser = parser.parse_args
  def Parse(args):
    (options, args) = old_parser(args)
    if options.verbose == 2:
      logging.basicConfig(level=logging.INFO)
    elif options.verbose > 2:
      logging.basicConfig(level=logging.DEBUG)
    options.entries_filename = options.config_filename + "_entries"
    if not hasattr(options, 'revisions'):
      # GClient.RunOnDeps expects it even if not applicable.
      options.revisions = []
    if not hasattr(options, 'head'):
      options.head = None
    return (options, args)
  parser.parse_args = Parse
  # We don't want wordwrapping in epilog (usually examples)
  parser.format_epilog = lambda _: parser.epilog or ''
  if argv:
    command = Command(argv[0])
    if command:
      # "fix" the usage and the description now that we know the subcommand.
      GenUsage(parser, argv[0])
      return command(parser, argv[1:])
  # Not a known command. Default to help.
  GenUsage(parser, 'help')
  return CMDhelp(parser, argv)


if "__main__" == __name__:
  try:
    sys.exit(Main(sys.argv[1:]))
  except gclient_utils.Error, e:
    print >> sys.stderr, "Error: %s" % str(e)
    sys.exit(1)

# vim: ts=2:sw=2:tw=80:et:
