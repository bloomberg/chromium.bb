#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Meta checkout manager supporting both Subversion and GIT.

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
  can be prevented by using --nohooks (hooks run by default). Hooks can also
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

__version__ = "0.5"

import logging
import optparse
import os
import pprint
import re
import subprocess
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


class GClientKeywords(object):
  class FromImpl(object):
    """Used to implement the From() syntax."""

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

  class FileImpl(object):
    """Used to implement the File('') syntax which lets you sync a single file
    from a SVN repo."""

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

  class VarImpl(object):
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


class Dependency(GClientKeywords):
  """Object that represents a dependency checkout."""
  DEPS_FILE = 'DEPS'

  def __init__(self, parent, name, url, safesync_url=None, custom_deps=None,
               custom_vars=None, deps_file=None):
    GClientKeywords.__init__(self)
    self.parent = parent
    self.name = name
    self.url = url
    self.parsed_url = None
    # These 2 are only set in .gclient and not in DEPS files.
    self.safesync_url = safesync_url
    self.custom_vars = custom_vars or {}
    self.custom_deps = custom_deps or {}
    self.deps_hooks = []
    self.dependencies = []
    self.deps_file = deps_file or self.DEPS_FILE
    # A cache of the files affected by the current operation, necessary for
    # hooks.
    self.file_list = []
    self.deps_parsed = False
    self.direct_reference = False

    # Sanity checks
    if not self.name and self.parent:
      raise gclient_utils.Error('Dependency without name')
    if self.name in [d.name for d in self.tree(False)]:
      raise gclient_utils.Error('Dependency %s specified more than once' %
          self.name)
    if not isinstance(self.url,
        (basestring, self.FromImpl, self.FileImpl, None.__class__)):
      raise gclient_utils.Error('dependency url must be either a string, None, '
                                'File() or From() instead of %s' %
                                self.url.__class__.__name__)
    if '/' in self.deps_file or '\\' in self.deps_file:
      raise gclient_utils.Error('deps_file name must not be a path, just a '
                                'filename. %s' % self.deps_file)

  def LateOverride(self, url):
    overriden_url = self.get_custom_deps(self.name, url)
    if overriden_url != url:
      self.parsed_url = overriden_url
      logging.debug('%s, %s was overriden to %s' % (self.name, url,
          self.parsed_url))
    elif isinstance(url, self.FromImpl):
      ref = [dep for dep in self.tree(True) if url.module_name == dep.name]
      if not len(ref) == 1:
        raise Exception('Failed to find one reference to %s. %s' % (
          url.module_name, ref))
      ref = ref[0]
      sub_target = url.sub_target_name or url
      # Make sure the referenced dependency DEPS file is loaded and file the
      # inner referenced dependency.
      ref.ParseDepsFile(False)
      found_dep = None
      for d in ref.dependencies:
        if d.name == sub_target:
          found_dep = d
          break
      if not found_dep:
        raise Exception('Couldn\'t find %s in %s, referenced by %s' % (
            sub_target, ref.name, self.name))
      # Call LateOverride() again.
      self.parsed_url = found_dep.LateOverride(found_dep.url)
      logging.debug('%s, %s to %s' % (self.name, url, self.parsed_url))
    elif isinstance(url, basestring):
      parsed_url = urlparse.urlparse(url)
      if not parsed_url[0]:
        # A relative url. Fetch the real base.
        path = parsed_url[2]
        if not path.startswith('/'):
          raise gclient_utils.Error(
              'relative DEPS entry \'%s\' must begin with a slash' % url)
        # Create a scm just to query the full url.
        parent_url = self.parent.parsed_url
        if isinstance(parent_url, self.FileImpl):
          parent_url = parent_url.file_location
        scm = gclient_scm.CreateSCM(parent_url, self.root_dir(), None)
        self.parsed_url = scm.FullUrlForRelativeUrl(url)
      else:
        self.parsed_url = url
      logging.debug('%s, %s -> %s' % (self.name, url, self.parsed_url))
    elif isinstance(url, self.FileImpl):
      self.parsed_url = url
      logging.debug('%s, %s -> %s (File)' % (self.name, url, self.parsed_url))
    return self.parsed_url

  def ParseDepsFile(self, direct_reference):
    """Parses the DEPS file for this dependency."""
    if direct_reference:
      # Maybe it was referenced earlier by a From() keyword but it's now
      # directly referenced.
      self.direct_reference = direct_reference
    if self.deps_parsed:
      return
    self.deps_parsed = True
    filepath = os.path.join(self.root_dir(), self.name, self.deps_file)
    if not os.path.isfile(filepath):
      return
    deps_content = gclient_utils.FileRead(filepath)

    # Eval the content.
    # One thing is unintuitive, vars= {} must happen before Var() use.
    local_scope = {}
    var = self.VarImpl(self.custom_vars, local_scope)
    global_scope = {
      'File': self.FileImpl,
      'From': self.FromImpl,
      'Var': var.Lookup,
      'deps_os': {},
    }
    try:
      exec(deps_content, global_scope, local_scope)
    except SyntaxError, e:
      gclient_utils.SyntaxErrorToError(filepath, e)
    deps = local_scope.get('deps', {})
    # load os specific dependencies if defined.  these dependencies may
    # override or extend the values defined by the 'deps' member.
    if 'deps_os' in local_scope:
      for deps_os_key in self.enforced_os():
        os_deps = local_scope['deps_os'].get(deps_os_key, {})
        if len(self.enforced_os()) > 1:
          # Ignore any conflict when including deps for more than one
          # platform, so we collect the broadest set of dependencies available.
          # We may end up with the wrong revision of something for our
          # platform, but this is the best we can do.
          deps.update([x for x in os_deps.items() if not x[0] in deps])
        else:
          deps.update(os_deps)

    self.deps_hooks.extend(local_scope.get('hooks', []))

    # If a line is in custom_deps, but not in the solution, we want to append
    # this line to the solution.
    for d in self.custom_deps:
      if d not in deps:
        deps[d] = self.custom_deps[d]

    # If use_relative_paths is set in the DEPS file, regenerate
    # the dictionary using paths relative to the directory containing
    # the DEPS file.
    use_relative_paths = local_scope.get('use_relative_paths', False)
    if use_relative_paths:
      rel_deps = {}
      for d, url in deps.items():
        # normpath is required to allow DEPS to use .. in their
        # dependency local path.
        rel_deps[os.path.normpath(os.path.join(self.name, d))] = url
      deps = rel_deps

    # Convert the deps into real Dependency.
    for name, url in deps.iteritems():
      if name in [s.name for s in self.dependencies]:
        raise
      self.dependencies.append(Dependency(self, name, url))
    logging.info('Loaded: %s' % str(self))

  def RunCommandRecursively(self, options, revision_overrides,
                            command, args, pm):
    """Runs 'command' before parsing the DEPS in case it's a initial checkout
    or a revert."""
    assert self.file_list == []
    # When running runhooks, there's no need to consult the SCM.
    # All known hooks are expected to run unconditionally regardless of working
    # copy state, so skip the SCM status check.
    run_scm = command not in ('runhooks', None)
    self.LateOverride(self.url)
    if run_scm and self.parsed_url:
      if isinstance(self.parsed_url, self.FileImpl):
        # Special support for single-file checkout.
        if not command in (None, 'cleanup', 'diff', 'pack', 'status'):
          options.revision = self.parsed_url.GetRevision()
          scm = gclient_scm.SVNWrapper(self.parsed_url.GetPath(),
                                       self.root_dir(),
                                       self.name)
          scm.RunCommand('updatesingle', options,
                         args + [self.parsed_url.GetFilename()],
                         self.file_list)
      else:
        options.revision = revision_overrides.get(self.name)
        scm = gclient_scm.CreateSCM(self.parsed_url, self.root_dir(), self.name)
        scm.RunCommand(command, options, args, self.file_list)
        self.file_list = [os.path.join(self.name, f.strip())
                          for f in self.file_list]
      options.revision = None
    if pm:
      # The + 1 comes from the fact that .gclient is considered a step in
      # itself, .i.e. this code is called one time for the .gclient. This is not
      # conceptually correct but it simplifies code.
      pm._total = len(self.tree(False)) + 1
      pm.update()
    if self.recursion_limit():
      # Then we can parse the DEPS file.
      self.ParseDepsFile(True)
      if pm:
        pm._total = len(self.tree(False)) + 1
        pm.update(0)
      # Parse the dependencies of this dependency.
      for s in self.dependencies:
        # TODO(maruel): All these can run concurrently! No need for threads,
        # just buffer stdout&stderr on pipes and flush as they complete.
        # Watch out for stdin.
        s.RunCommandRecursively(options, revision_overrides, command, args, pm)

  def RunHooksRecursively(self, options):
    """Evaluates all hooks, running actions as needed. RunCommandRecursively()
    must have been called before to load the DEPS."""
    # If "--force" was specified, run all hooks regardless of what files have
    # changed.
    if self.deps_hooks and self.direct_reference:
      # TODO(maruel): If the user is using git or git-svn, then we don't know
      # what files have changed so we always run all hooks. It'd be nice to fix
      # that.
      if (options.force or
          isinstance(self.parsed_url, self.FileImpl) or
          gclient_scm.GetScmName(self.parsed_url) in ('git', None) or
          os.path.isdir(os.path.join(self.root_dir(), self.name, '.git'))):
        for hook_dict in self.deps_hooks:
          self._RunHookAction(hook_dict, [])
      else:
        # TODO(phajdan.jr): We should know exactly when the paths are absolute.
        # Convert all absolute paths to relative.
        for i in range(len(self.file_list)):
          # It depends on the command being executed (like runhooks vs sync).
          if not os.path.isabs(self.file_list[i]):
            continue

          prefix = os.path.commonprefix([self.root_dir().lower(),
                                        self.file_list[i].lower()])
          self.file_list[i] = self.file_list[i][len(prefix):]

          # Strip any leading path separators.
          while (self.file_list[i].startswith('\\') or
                 self.file_list[i].startswith('/')):
            self.file_list[i] = self.file_list[i][1:]

        # Run hooks on the basis of whether the files from the gclient operation
        # match each hook's pattern.
        for hook_dict in self.deps_hooks:
          pattern = re.compile(hook_dict['pattern'])
          matching_file_list = [f for f in self.file_list if pattern.search(f)]
          if matching_file_list:
            self._RunHookAction(hook_dict, matching_file_list)
    if self.recursion_limit():
      for s in self.dependencies:
        s.RunHooksRecursively(options)

  def _RunHookAction(self, hook_dict, matching_file_list):
    """Runs the action from a single hook."""
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
    return gclient_utils.SubprocessCall(command, self.root_dir(), fail_status=2)

  def root_dir(self):
    return self.parent.root_dir()

  def enforced_os(self):
    return self.parent.enforced_os()

  def recursion_limit(self):
    return self.parent.recursion_limit() - 1

  def tree(self, force_all):
    return self.parent.tree(force_all)

  def get_custom_deps(self, name, url):
    """Returns a custom deps if applicable."""
    if self.parent:
      url = self.parent.get_custom_deps(name, url)
    # None is a valid return value to disable a dependency.
    return self.custom_deps.get(name, url)

  def __str__(self):
    out = []
    for i in ('name', 'url', 'safesync_url', 'custom_deps', 'custom_vars',
              'deps_hooks', 'file_list'):
      # 'deps_file'
      if self.__dict__[i]:
        out.append('%s: %s' % (i, self.__dict__[i]))

    for d in self.dependencies:
      out.extend(['  ' + x for x in str(d).splitlines()])
      out.append('')
    return '\n'.join(out)

  def __repr__(self):
    return '%s: %s' % (self.name, self.url)


class GClient(Dependency):
  """Object that represent a gclient checkout. A tree of Dependency(), one per
  solution or DEPS entry."""

  DEPS_OS_CHOICES = {
    "win32": "win",
    "win": "win",
    "cygwin": "win",
    "darwin": "mac",
    "mac": "mac",
    "unix": "unix",
    "linux": "unix",
    "linux2": "unix",
  }

  DEFAULT_CLIENT_FILE_TEXT = ("""\
solutions = [
  { "name"        : "%(solution_name)s",
    "url"         : "%(solution_url)s",
    "custom_deps" : {
    },
    "safesync_url": "%(safesync_url)s",
  },
]
""")

  DEFAULT_SNAPSHOT_SOLUTION_TEXT = ("""\
  { "name"        : "%(solution_name)s",
    "url"         : "%(solution_url)s",
    "custom_deps" : {
%(solution_deps)s    },
    "safesync_url": "%(safesync_url)s",
  },
""")

  DEFAULT_SNAPSHOT_FILE_TEXT = ("""\
# Snapshot generated with gclient revinfo --snapshot
solutions = [
%(solution_list)s]
""")

  def __init__(self, root_dir, options):
    Dependency.__init__(self, None, None, None)
    self._options = options
    if options.deps_os:
      enforced_os = options.deps_os.split(',')
    else:
      enforced_os = [self.DEPS_OS_CHOICES.get(sys.platform, 'unix')]
    if 'all' in enforced_os:
      enforced_os = self.DEPS_OS_CHOICES.itervalues()
    self._enforced_os = list(set(enforced_os))
    self._root_dir = root_dir
    self.config_content = None
    # Do not change previous behavior. Only solution level and immediate DEPS
    # are processed.
    self._recursion_limit = 2

  def SetConfig(self, content):
    assert self.dependencies == []
    config_dict = {}
    self.config_content = content
    try:
      exec(content, config_dict)
    except SyntaxError, e:
      gclient_utils.SyntaxErrorToError('.gclient', e)
    for s in config_dict.get('solutions', []):
      try:
        self.dependencies.append(Dependency(
            self, s['name'], s['url'],
            s.get('safesync_url', None),
            s.get('custom_deps', {}),
            s.get('custom_vars', {})))
      except KeyError:
        raise gclient_utils.Error('Invalid .gclient file. Solution is '
                                  'incomplete: %s' % s)
    # .gclient can have hooks.
    self.deps_hooks = config_dict.get('hooks', [])

  def SaveConfig(self):
    gclient_utils.FileWrite(os.path.join(self.root_dir(),
                                         self._options.config_filename),
                            self.config_content)

  @staticmethod
  def LoadCurrentConfig(options):
    """Searches for and loads a .gclient file relative to the current working
    dir. Returns a GClient object."""
    path = gclient_utils.FindGclientRoot(os.getcwd(), options.config_filename)
    if not path:
      return None
    client = GClient(path, options)
    client.SetConfig(gclient_utils.FileRead(
        os.path.join(path, options.config_filename)))
    return client

  def SetDefaultConfig(self, solution_name, solution_url, safesync_url):
    self.SetConfig(self.DEFAULT_CLIENT_FILE_TEXT % {
      'solution_name': solution_name,
      'solution_url': solution_url,
      'safesync_url' : safesync_url,
    })

  def _SaveEntries(self):
    """Creates a .gclient_entries file to record the list of unique checkouts.

    The .gclient_entries file lives in the same directory as .gclient.
    """
    # Sometimes pprint.pformat will use {', sometimes it'll use { ' ... It
    # makes testing a bit too fun.
    result = 'entries = {\n'
    for entry in self.tree(False):
      # Skip over File() dependencies as we can't version them.
      if not isinstance(entry.parsed_url, self.FileImpl):
        result += '  %s: %s,\n' % (pprint.pformat(entry.name),
            pprint.pformat(entry.parsed_url))
    result += '}\n'
    file_path = os.path.join(self.root_dir(), self._options.entries_filename)
    logging.info(result)
    gclient_utils.FileWrite(file_path, result)

  def _ReadEntries(self):
    """Read the .gclient_entries file for the given client.

    Returns:
      A sequence of solution names, which will be empty if there is the
      entries file hasn't been created yet.
    """
    scope = {}
    filename = os.path.join(self.root_dir(), self._options.entries_filename)
    if not os.path.exists(filename):
      return {}
    try:
      exec(gclient_utils.FileRead(filename), scope)
    except SyntaxError, e:
      gclient_utils.SyntaxErrorToError(filename, e)
    return scope['entries']

  def _EnforceRevisions(self):
    """Checks for revision overrides."""
    revision_overrides = {}
    if self._options.head:
      return revision_overrides
    for s in self.dependencies:
      if not s.safesync_url:
        continue
      handle = urllib.urlopen(s.safesync_url)
      rev = handle.read().strip()
      handle.close()
      if len(rev):
        self._options.revisions.append('%s@%s' % (s.name, rev))
    if not self._options.revisions:
      return revision_overrides
    # --revision will take over safesync_url.
    solutions_names = [s.name for s in self.dependencies]
    index = 0
    for revision in self._options.revisions:
      if not '@' in revision:
        # Support for --revision 123
        revision = '%s@%s' % (solutions_names[index], revision)
      sol, rev = revision.split('@', 1)
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

    Args:
      command: The command to use (e.g., 'status' or 'diff')
      args: list of str - extra arguments to add to the command line.
    """
    if not self.dependencies:
      raise gclient_utils.Error('No solution specified')
    revision_overrides = self._EnforceRevisions()
    pm = None
    if command == 'update' and not self._options.verbose:
      pm = Progress('Syncing projects', len(self.tree(False)) + 1)
    self.RunCommandRecursively(self._options, revision_overrides,
                               command, args, pm)
    if pm:
      pm.end()

    # Once all the dependencies have been processed, it's now safe to run the
    # hooks.
    if not self._options.nohooks:
      self.RunHooksRecursively(self._options)

    if command == 'update':
      # Notify the user if there is an orphaned entry in their working copy.
      # Only delete the directory if there are no changes in it, and
      # delete_unversioned_trees is set to true.
      entries = [i.name for i in self.tree(False)]
      for entry, prev_url in self._ReadEntries().iteritems():
        # Fix path separator on Windows.
        entry_fixed = entry.replace('/', os.path.sep)
        e_dir = os.path.join(self.root_dir(), entry_fixed)
        # Use entry and not entry_fixed there.
        if entry not in entries and os.path.exists(e_dir):
          file_list = []
          scm = gclient_scm.CreateSCM(prev_url, self.root_dir(), entry_fixed)
          scm.status(self._options, [], file_list)
          modified_files = file_list != []
          if not self._options.delete_unversioned_trees or modified_files:
            # There are modified files in this entry. Keep warning until
            # removed.
            print(('\nWARNING: \'%s\' is no longer part of this client.  '
                   'It is recommended that you manually remove it.\n') %
                      entry_fixed)
          else:
            # Delete the entry
            print('\n________ deleting \'%s\' in \'%s\'' % (
                entry_fixed, self.root_dir()))
            gclient_utils.RemoveDirectory(e_dir)
      # record the current list of entries for next time
      self._SaveEntries()
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
    if not self.dependencies:
      raise gclient_utils.Error('No solution specified')
    # Load all the settings.
    self.RunCommandRecursively(self._options, {}, None, [], None)

    def GetURLAndRev(name, original_url):
      """Returns the revision-qualified SCM url."""
      if original_url is None:
        return None
      if isinstance(original_url, self.FileImpl):
        original_url = original_url.file_location
      url, _ = gclient_utils.SplitUrlRevision(original_url)
      scm = gclient_scm.CreateSCM(original_url, self.root_dir(), name)
      if not os.path.isdir(scm.checkout_path):
        return None
      return '%s@%s' % (url, scm.revinfo(self._options, [], None))

    if self._options.snapshot:
      new_gclient = ''
      # First level at .gclient
      for d in self.dependencies:
        entries = {}
        def GrabDeps(sol):
          """Recursively grab dependencies."""
          for i in sol.dependencies:
            entries[i.name] = GetURLAndRev(i.name, i.parsed_url)
            GrabDeps(i)
        GrabDeps(d)
        custom_deps = []
        for k in sorted(entries.keys()):
          if entries[k]:
            # Quotes aren't escaped...
            custom_deps.append('      \"%s\": \'%s\',\n' % (k, entries[k]))
          else:
            custom_deps.append('      \"%s\": None,\n' % k)
        new_gclient += self.DEFAULT_SNAPSHOT_SOLUTION_TEXT % {
            'solution_name': d.name,
            'solution_url': d.url,
            'safesync_url' : d.safesync_url or '',
            'solution_deps': ''.join(custom_deps),
        }
      # Print the snapshot configuration file
      print(self.DEFAULT_SNAPSHOT_FILE_TEXT % {'solution_list': new_gclient})
    else:
      entries = sorted(self.tree(False), key=lambda i: i.name)
      for entry in entries:
        entry_url = GetURLAndRev(entry.name, entry.parsed_url)
        line = '%s: %s' % (entry.name, entry_url)
        if not entry is entries[-1]:
          line += ';'
        print line

  def ParseDepsFile(self, direct_reference):
    """No DEPS to parse for a .gclient file."""
    self.direct_reference = direct_reference
    self.deps_parsed = True

  def root_dir(self):
    """Root directory of gclient checkout."""
    return self._root_dir

  def enforced_os(self):
    """What deps_os entries that are to be parsed."""
    return self._enforced_os

  def recursion_limit(self):
    """How recursive can each dependencies in DEPS file can load DEPS file."""
    return self._recursion_limit

  def tree(self, force_all):
    """Returns a flat list of all the dependencies."""
    def subtree(dep):
      if not force_all and not dep.direct_reference:
        # Was loaded from a From() keyword in a DEPS file, don't load all its
        # dependencies.
        return []
      result = dep.dependencies[:]
      for d in dep.dependencies:
        result.extend(subtree(d))
      return result
    return subtree(self)


#### gclient commands.


def CMDcleanup(parser, args):
  """Cleans up all working copies.

Mostly svn-specific. Simply runs 'svn cleanup' for each module.
"""
  parser.add_option('--deps', dest='deps_os', metavar='OS_LIST',
                    help='override deps for the specified (comma-separated) '
                         'platform(s); \'all\' will process all deps_os '
                         'references')
  (options, args) = parser.parse_args(args)
  client = GClient.LoadCurrentConfig(options)
  if not client:
    raise gclient_utils.Error('client not configured; see \'gclient config\'')
  if options.verbose:
    # Print out the .gclient file.  This is longer than if we just printed the
    # client dict, but more legible, and it might contain helpful comments.
    print(client.config_content)
  return client.RunOnDeps('cleanup', args)


@attr('usage', '[command] [args ...]')
def CMDrecurse(parser, args):
  """Operates on all the entries.

  Runs a shell command on all entries.
  """
  # Stop parsing at the first non-arg so that these go through to the command
  parser.disable_interspersed_args()
  parser.add_option('-s', '--scm', action='append', default=[],
                    help='choose scm types to operate upon')
  options, args = parser.parse_args(args)
  root, entries = gclient_utils.GetGClientRootAndEntries()
  scm_set = set()
  for scm in options.scm:
    scm_set.update(scm.split(','))

  # Pass in the SCM type as an env variable
  env = os.environ.copy()

  for path, url in entries.iteritems():
    scm = gclient_scm.GetScmName(url)
    if scm_set and scm not in scm_set:
      continue
    dir = os.path.normpath(os.path.join(root, path))
    env['GCLIENT_SCM'] = scm
    env['GCLIENT_URL'] = url
    subprocess.Popen(args, cwd=dir, env=env).communicate()


@attr('usage', '[url] [safesync url]')
def CMDconfig(parser, args):
  """Create a .gclient file in the current directory.

This specifies the configuration for further commands. After update/sync,
top-level DEPS files in each module are read to determine dependent
modules to operate on as well. If optional [url] parameter is
provided, then configuration is read from a specified Subversion server
URL.
"""
  parser.add_option('--spec',
                    help='create a gclient file containing the provided '
                         'string. Due to Cygwin/Python brokenness, it '
                         'probably can\'t contain any newlines.')
  parser.add_option('--name',
                    help='overrides the default name for the solution')
  (options, args) = parser.parse_args(args)
  if ((options.spec and args) or len(args) > 2 or
      (not options.spec and not args)):
    parser.error('Inconsistent arguments. Use either --spec or one or 2 args')

  if os.path.exists(options.config_filename):
    raise gclient_utils.Error('%s file already exists in the current directory'
                                  % options.config_filename)
  client = GClient('.', options)
  if options.spec:
    client.SetConfig(options.spec)
  else:
    base_url = args[0].rstrip('/')
    if not options.name:
      name = base_url.split('/')[-1]
    else:
      # specify an alternate relpath for the given URL.
      name = options.name
    safesync_url = ''
    if len(args) > 1:
      safesync_url = args[1]
    client.SetDefaultConfig(name, base_url, safesync_url)
  client.SaveConfig()
  return 0


def CMDexport(parser, args):
  """Wrapper for svn export for all managed directories."""
  parser.add_option('--deps', dest='deps_os', metavar='OS_LIST',
                    help='override deps for the specified (comma-separated) '
                         'platform(s); \'all\' will process all deps_os '
                         'references')
  (options, args) = parser.parse_args(args)
  if len(args) != 1:
    raise gclient_utils.Error('Need directory name')
  client = GClient.LoadCurrentConfig(options)

  if not client:
    raise gclient_utils.Error('client not configured; see \'gclient config\'')

  if options.verbose:
    # Print out the .gclient file.  This is longer than if we just printed the
    # client dict, but more legible, and it might contain helpful comments.
    print(client.config_content)
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
  parser.add_option('--deps', dest='deps_os', metavar='OS_LIST',
                    help='override deps for the specified (comma-separated) '
                         'platform(s); \'all\' will process all deps_os '
                         'references')
  (options, args) = parser.parse_args(args)
  client = GClient.LoadCurrentConfig(options)
  if not client:
    raise gclient_utils.Error('client not configured; see \'gclient config\'')
  if options.verbose:
    # Print out the .gclient file.  This is longer than if we just printed the
    # client dict, but more legible, and it might contain helpful comments.
    print(client.config_content)
  return client.RunOnDeps('pack', args)


def CMDstatus(parser, args):
  """Show modification status for every dependencies."""
  parser.add_option('--deps', dest='deps_os', metavar='OS_LIST',
                    help='override deps for the specified (comma-separated) '
                         'platform(s); \'all\' will process all deps_os '
                         'references')
  (options, args) = parser.parse_args(args)
  client = GClient.LoadCurrentConfig(options)
  if not client:
    raise gclient_utils.Error('client not configured; see \'gclient config\'')
  if options.verbose:
    # Print out the .gclient file.  This is longer than if we just printed the
    # client dict, but more legible, and it might contain helpful comments.
    print(client.config_content)
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
  parser.add_option('-f', '--force', action='store_true',
                    help='force update even for unchanged modules')
  parser.add_option('-n', '--nohooks', action='store_true',
                    help='don\'t run hooks after the update is complete')
  parser.add_option('-r', '--revision', action='append',
                    dest='revisions', metavar='REV', default=[],
                    help='Enforces revision/hash for the solutions with the '
                         'format src@rev. The src@ part is optional and can be '
                         'skipped. -r can be used multiple times when .gclient '
                         'has multiple solutions configured and will work even '
                         'if the src@ part is skipped.')
  parser.add_option('-H', '--head', action='store_true',
                    help='skips any safesync_urls specified in '
                         'configured solutions and sync to head instead')
  parser.add_option('-D', '--delete_unversioned_trees', action='store_true',
                    help='delete any unexpected unversioned trees '
                         'that are in the checkout')
  parser.add_option('-R', '--reset', action='store_true',
                    help='resets any local changes before updating (git only)')
  parser.add_option('--deps', dest='deps_os', metavar='OS_LIST',
                    help='override deps for the specified (comma-separated) '
                         'platform(s); \'all\' will process all deps_os '
                         'references')
  parser.add_option('-m', '--manually_grab_svn_rev', action='store_true',
                    help='Skip svn up whenever possible by requesting '
                         'actual HEAD revision from the repository')
  (options, args) = parser.parse_args(args)
  client = GClient.LoadCurrentConfig(options)

  if not client:
    raise gclient_utils.Error('client not configured; see \'gclient config\'')

  if options.revisions and options.head:
    # TODO(maruel): Make it a parser.error if it doesn't break any builder.
    print('Warning: you cannot use both --head and --revision')

  if options.verbose:
    # Print out the .gclient file.  This is longer than if we just printed the
    # client dict, but more legible, and it might contain helpful comments.
    print(client.config_content)
  return client.RunOnDeps('update', args)


def CMDupdate(parser, args):
  """Alias for the sync command. Deprecated."""
  return CMDsync(parser, args)

def CMDdiff(parser, args):
  """Displays local diff for every dependencies."""
  parser.add_option('--deps', dest='deps_os', metavar='OS_LIST',
                    help='override deps for the specified (comma-separated) '
                         'platform(s); \'all\' will process all deps_os '
                         'references')
  (options, args) = parser.parse_args(args)
  client = GClient.LoadCurrentConfig(options)
  if not client:
    raise gclient_utils.Error('client not configured; see \'gclient config\'')
  if options.verbose:
    # Print out the .gclient file.  This is longer than if we just printed the
    # client dict, but more legible, and it might contain helpful comments.
    print(client.config_content)
  return client.RunOnDeps('diff', args)


def CMDrevert(parser, args):
  """Revert all modifications in every dependencies."""
  parser.add_option('--deps', dest='deps_os', metavar='OS_LIST',
                    help='override deps for the specified (comma-separated) '
                         'platform(s); \'all\' will process all deps_os '
                         'references')
  parser.add_option('-n', '--nohooks', action='store_true',
                    help='don\'t run hooks after the revert is complete')
  (options, args) = parser.parse_args(args)
  # --force is implied.
  options.force = True
  client = GClient.LoadCurrentConfig(options)
  if not client:
    raise gclient_utils.Error('client not configured; see \'gclient config\'')
  return client.RunOnDeps('revert', args)


def CMDrunhooks(parser, args):
  """Runs hooks for files that have been modified in the local working copy."""
  parser.add_option('--deps', dest='deps_os', metavar='OS_LIST',
                    help='override deps for the specified (comma-separated) '
                         'platform(s); \'all\' will process all deps_os '
                         'references')
  parser.add_option('-f', '--force', action='store_true', default=True,
                    help='Deprecated. No effect.')
  (options, args) = parser.parse_args(args)
  client = GClient.LoadCurrentConfig(options)
  if not client:
    raise gclient_utils.Error('client not configured; see \'gclient config\'')
  if options.verbose:
    # Print out the .gclient file.  This is longer than if we just printed the
    # client dict, but more legible, and it might contain helpful comments.
    print(client.config_content)
  options.force = True
  options.nohooks = False
  return client.RunOnDeps('runhooks', args)


def CMDrevinfo(parser, args):
  """Output revision info mapping for the client and its dependencies.

  This allows the capture of an overall 'revision' for the source tree that
  can be used to reproduce the same tree in the future. It is only useful for
  'unpinned dependencies', i.e. DEPS/deps references without a svn revision
  number or a git hash. A git branch name isn't 'pinned' since the actual
  commit can change.
  """
  parser.add_option('--deps', dest='deps_os', metavar='OS_LIST',
                    help='override deps for the specified (comma-separated) '
                         'platform(s); \'all\' will process all deps_os '
                         'references')
  parser.add_option('-s', '--snapshot', action='store_true',
                    help='creates a snapshot .gclient file of the current '
                         'version of all repositories to reproduce the tree')
  (options, args) = parser.parse_args(args)
  client = GClient.LoadCurrentConfig(options)
  if not client:
    raise gclient_utils.Error('client not configured; see \'gclient config\'')
  client.PrintRevInfo()
  return 0


def Command(name):
  return getattr(sys.modules[__name__], 'CMD' + name, None)


def CMDhelp(parser, args):
  """Prints list of commands or help for a specific command."""
  (_, args) = parser.parse_args(args)
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
  try:
    # Do it late so all commands are listed.
    CMDhelp.usage = ('\n\nCommands are:\n' + '\n'.join([
        '  %-10s %s' % (fn[3:], Command(fn[3:]).__doc__.split('\n')[0].strip())
        for fn in dir(sys.modules[__name__]) if fn.startswith('CMD')]))
    parser = optparse.OptionParser(version='%prog ' + __version__)
    parser.add_option('-v', '--verbose', action='count', default=0,
                      help='Produces additional output for diagnostics. Can be '
                           'used up to three times for more logging info.')
    parser.add_option('--gclientfile', dest='config_filename',
                      default=os.environ.get('GCLIENT_FILE', '.gclient'),
                      help='Specify an alternate %default file')
    # Integrate standard options processing.
    old_parser = parser.parse_args
    def Parse(args):
      (options, args) = old_parser(args)
      level = None
      if options.verbose == 2:
        level = logging.INFO
      elif options.verbose > 2:
        level = logging.DEBUG
      logging.basicConfig(level=level,
          format='%(module)s(%(lineno)d) %(funcName)s:%(message)s')
      options.entries_filename = options.config_filename + '_entries'

      # These hacks need to die.
      if not hasattr(options, 'revisions'):
        # GClient.RunOnDeps expects it even if not applicable.
        options.revisions = []
      if not hasattr(options, 'head'):
        options.head = None
      if not hasattr(options, 'nohooks'):
        options.nohooks = True
      if not hasattr(options, 'deps_os'):
        options.deps_os = None
      if not hasattr(options, 'manually_grab_svn_rev'):
        options.manually_grab_svn_rev = None
      if not hasattr(options, 'force'):
        options.force = None
      return (options, args)
    parser.parse_args = Parse
    # We don't want wordwrapping in epilog (usually examples)
    parser.format_epilog = lambda _: parser.epilog or ''
    if argv:
      command = Command(argv[0])
      if command:
        # 'fix' the usage and the description now that we know the subcommand.
        GenUsage(parser, argv[0])
        return command(parser, argv[1:])
    # Not a known command. Default to help.
    GenUsage(parser, 'help')
    return CMDhelp(parser, argv)
  except gclient_utils.Error, e:
    print >> sys.stderr, 'Error: %s' % str(e)
    return 1


if '__main__' == __name__:
  sys.exit(Main(sys.argv[1:]))

# vim: ts=2:sw=2:tw=80:et:
