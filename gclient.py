#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

__version__ = "0.6.2"

import copy
import logging
import optparse
import os
import posixpath
import pprint
import re
import sys
import urlparse
import urllib

import breakpad  # pylint: disable=W0611

import fix_encoding
import gclient_scm
import gclient_utils
from third_party.repo.progress import Progress
import subprocess2


def attr(attribute, data):
  """Sets an attribute on a function."""
  def hook(fn):
    setattr(fn, attribute, data)
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


class Dependency(GClientKeywords, gclient_utils.WorkItem):
  """Object that represents a dependency checkout."""

  def __init__(self, parent, name, url, safesync_url, managed, custom_deps,
               custom_vars, deps_file, should_process):
    # Warning: this function can be called from any thread. Both
    # self.dependencies and self.requirements are read and modified from
    # multiple threads at the same time. Sad.
    GClientKeywords.__init__(self)
    gclient_utils.WorkItem.__init__(self, name)

    # These are not mutable:
    self._parent = parent
    self._safesync_url = safesync_url
    self._deps_file = deps_file
    self._should_process = should_process

    # This is in both .gclient and DEPS files:
    self.url = url

    # These are only set in .gclient and not in DEPS files.
    # 'managed' determines whether or not this dependency is synced/updated by
    # gclient after gclient checks it out initially.  The difference between
    # 'managed' and 'should_process' (defined below) is that the user specifies
    # 'managed' via the --unmanaged command-line flag or a .gclient config,
    # where 'should_process' is dynamically set by gclient if it goes over its
    # recursion limit and controls gclient's behavior so it does not misbehave.
    self._managed = managed
    self._custom_vars = custom_vars or {}
    self._custom_deps = custom_deps or {}
    self.deps_hooks = []

    # Calculates properties:
    self.parsed_url = None
    self.dependencies = []
    # A cache of the files affected by the current operation, necessary for
    # hooks.
    self._file_list = []
    # If it is not set to True, the dependency wasn't processed for its child
    # dependency, i.e. its DEPS wasn't read.
    self.deps_parsed = False
    # This dependency has been processed, i.e. checked out
    self.processed = False
    # This dependency had its hook run
    self.hooks_ran = False

    # Post process the url to remove trailing slashes.
    if isinstance(self.url, basestring):
      # urls are sometime incorrectly written as proto://host/path/@rev. Replace
      # it to proto://host/path@rev.
      if self.url.count('@') > 1:
        raise gclient_utils.Error('Invalid url "%s"' % self.url)
      self.url = self.url.replace('/@', '@')

    self._FindDependencies()

    # Sanity checks
    if not self.name and self.parent:
      raise gclient_utils.Error('Dependency without name')
    if not isinstance(self.url,
        (basestring, self.FromImpl, self.FileImpl, None.__class__)):
      raise gclient_utils.Error('dependency url must be either a string, None, '
                                'File() or From() instead of %s' %
                                self.url.__class__.__name__)
    if '/' in self.deps_file or '\\' in self.deps_file:
      raise gclient_utils.Error('deps_file name must not be a path, just a '
                                'filename. %s' % self.deps_file)

  def _FindDependencies(self):
    """Setup self.requirements and find any other dependency who would have self
    as a requirement.
    """
    # self.parent is implicitly a requirement. This will be recursive by
    # definition.
    if self.parent and self.parent.name:
      self._requirements.add(self.parent.name)

    # For a tree with at least 2 levels*, the leaf node needs to depend
    # on the level higher up in an orderly way.
    # This becomes messy for >2 depth as the DEPS file format is a dictionary,
    # thus unsorted, while the .gclient format is a list thus sorted.
    #
    # * _recursion_limit is hard coded 2 and there is no hope to change this
    # value.
    #
    # Interestingly enough, the following condition only works in the case we
    # want: self is a 2nd level node. 3nd level node wouldn't need this since
    # they already have their parent as a requirement.
    root_deps = self.root.dependencies
    if self.parent in root_deps:
      for i in root_deps:
        if i is self.parent:
          break
        if i.name:
          self._requirements.add(i.name)

    if isinstance(self.url, self.FromImpl):
      self._requirements.add(self.url.module_name)

    if self.name and self.should_process:
      def yield_full_tree(root):
        """Depth-first recursion."""
        yield root
        for i in root.dependencies:
          for j in yield_full_tree(i):
            if j.should_process:
              yield j

      for obj in yield_full_tree(self.root):
        if obj is self or not obj.name:
          continue
        # Step 1: Find any requirements self may need.
        if self.name.startswith(posixpath.join(obj.name, '')):
          self._requirements.add(obj.name)
        # Step 2: Find any requirements self may impose.
        if obj.name.startswith(posixpath.join(self.name, '')):
          try:
            # Access to a protected member _requirements of a client class
            # pylint: disable=W0212
            obj.lock.acquire()
            obj._requirements.add(self.name)
          finally:
            obj.lock.release()

  def LateOverride(self, url):
    """Resolves the parsed url from url.

    Manages From() keyword accordingly. Do not touch self.parsed_url nor
    self.url because it may called with other urls due to From()."""
    assert self.parsed_url == None or not self.should_process, self.parsed_url
    overriden_url = self.get_custom_deps(self.name, url)
    if overriden_url != url:
      logging.info('%s, %s was overriden to %s' % (self.name, url,
          overriden_url))
      return overriden_url
    elif isinstance(url, self.FromImpl):
      ref = [
          dep for dep in self.root.subtree(True) if url.module_name == dep.name
      ]
      if not ref:
        raise gclient_utils.Error('Failed to find one reference to %s. %s' % (
            url.module_name, ref))
      # It may happen that len(ref) > 1 but it's no big deal.
      ref = ref[0]
      sub_target = url.sub_target_name or self.name
      # Make sure the referenced dependency DEPS file is loaded and file the
      # inner referenced dependency.
      ref.ParseDepsFile()
      found_dep = None
      for d in ref.dependencies:
        if d.name == sub_target:
          found_dep = d
          break
      if not found_dep:
        raise gclient_utils.Error(
            'Couldn\'t find %s in %s, referenced by %s (parent: %s)\n%s' % (
                sub_target, ref.name, self.name, self.parent.name,
                str(self.root)))

      # Call LateOverride() again.
      parsed_url = found_dep.LateOverride(found_dep.url)
      logging.info('%s, %s to %s' % (self.name, url, parsed_url))
      return parsed_url
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
        scm = gclient_scm.CreateSCM(parent_url, self.root.root_dir, None)
        parsed_url = scm.FullUrlForRelativeUrl(url)
      else:
        parsed_url = url
      logging.info('%s, %s -> %s' % (self.name, url, parsed_url))
      return parsed_url
    elif isinstance(url, self.FileImpl):
      parsed_url = url
      logging.info('%s, %s -> %s (File)' % (self.name, url, parsed_url))
      return parsed_url
    elif url is None:
      return None
    else:
      raise gclient_utils.Error('Unkown url type')

  def ParseDepsFile(self):
    """Parses the DEPS file for this dependency."""
    assert self.processed == True
    if self.deps_parsed:
      logging.debug('%s was already parsed' % self.name)
      return
    self.deps_parsed = True
    # One thing is unintuitive, vars= {} must happen before Var() use.
    local_scope = {}
    var = self.VarImpl(self.custom_vars, local_scope)
    global_scope = {
      'File': self.FileImpl,
      'From': self.FromImpl,
      'Var': var.Lookup,
      'deps_os': {},
    }
    filepath = os.path.join(self.root.root_dir, self.name, self.deps_file)
    if not os.path.isfile(filepath):
      logging.info('%s: No %s file found at %s' % (self.name, self.deps_file,
                                                   filepath))
    else:
      deps_content = gclient_utils.FileRead(filepath)
      logging.debug(deps_content)
      # Eval the content.
      try:
        exec(deps_content, global_scope, local_scope)
      except SyntaxError, e:
        gclient_utils.SyntaxErrorToError(filepath, e)
    deps = local_scope.get('deps', {})
    # load os specific dependencies if defined.  these dependencies may
    # override or extend the values defined by the 'deps' member.
    if 'deps_os' in local_scope:
      enforced_os = self.root.enforced_os
      for deps_os_key in enforced_os:
        os_deps = local_scope['deps_os'].get(deps_os_key, {})
        if len(enforced_os) > 1:
          # Ignore any conflict when including deps for more than one
          # platform, so we collect the broadest set of dependencies
          # available. We may end up with the wrong revision of something for
          # our platform, but this is the best we can do.
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
        raise gclient_utils.Error(
            'The same name "%s" appears multiple times in the deps section' %
                name)
      should_process = self.recursion_limit and self.should_process
      if should_process:
        tree = dict((d.name, d) for d in self.root.subtree(False))
        if name in tree:
          if url == tree[name].url:
            logging.info('Won\'t process duplicate dependency %s' % tree[name])
            # In theory we could keep it as a shadow of the other one. In
            # practice, simply ignore it.
            #should_process = False
            continue
          else:
            raise gclient_utils.Error(
                'Dependency %s specified more than once:\n  %s\nvs\n  %s' %
                (name, tree[name].hierarchy(), self.hierarchy()))
      self.dependencies.append(Dependency(self, name, url, None, None, None,
          None, self.deps_file, should_process))
    logging.debug('Loaded: %s' % str(self))

  # Arguments number differs from overridden method
  # pylint: disable=W0221
  def run(self, revision_overrides, command, args, work_queue, options):
    """Runs 'command' before parsing the DEPS in case it's a initial checkout
    or a revert."""

    def maybeGetParentRevision(options):
      """If we are performing an update and --transitive is set, set the
      revision to the parent's revision. If we have an explicit revision
      do nothing."""
      if command == 'update' and options.transitive and not options.revision:
        _, revision = gclient_utils.SplitUrlRevision(self.parsed_url)
        if not revision:
          options.revision = revision_overrides.get(self.parent.name)
          if options.verbose and options.revision:
            print("Using parent's revision date: %s" % options.revision)
          # If the parent has a revision override, then it must have been
          # converted to date format.
          assert (not options.revision or
                  gclient_utils.IsDateRevision(options.revision))

    def maybeConvertToDateRevision(options):
      """If we are performing an update and --transitive is set, convert the
      revision to a date-revision (if necessary). Instead of having
      -r 101 replace the revision with the time stamp of 101 (e.g.
      "{2011-18-04}").
      This way dependencies are upgraded to the revision they had at the
      check-in of revision 101."""
      if (command == 'update' and
          options.transitive and
          options.revision and
          not gclient_utils.IsDateRevision(options.revision)):
        revision_date = scm.GetRevisionDate(options.revision)
        revision = gclient_utils.MakeDateRevision(revision_date)
        if options.verbose:
          print("Updating revision override from %s to %s." %
                (options.revision, revision))
        revision_overrides[self.name] = revision

    assert self._file_list == []
    if not self.should_process:
      return
    # When running runhooks, there's no need to consult the SCM.
    # All known hooks are expected to run unconditionally regardless of working
    # copy state, so skip the SCM status check.
    run_scm = command not in ('runhooks', None)
    self.parsed_url = self.LateOverride(self.url)
    if run_scm and self.parsed_url:
      if isinstance(self.parsed_url, self.FileImpl):
        # Special support for single-file checkout.
        if not command in (None, 'cleanup', 'diff', 'pack', 'status'):
          options.revision = self.parsed_url.GetRevision()
          scm = gclient_scm.SVNWrapper(self.parsed_url.GetPath(),
                                       self.root.root_dir,
                                       self.name)
          scm.RunCommand('updatesingle', options,
                         args + [self.parsed_url.GetFilename()],
                         self._file_list)
      else:
        # Create a shallow copy to mutate revision.
        options = copy.copy(options)
        options.revision = revision_overrides.get(self.name)
        maybeGetParentRevision(options)
        scm = gclient_scm.CreateSCM(
            self.parsed_url, self.root.root_dir, self.name)
        scm.RunCommand(command, options, args, self._file_list)
        maybeConvertToDateRevision(options)
        self._file_list = [os.path.join(self.name, f.strip())
                           for f in self._file_list]

      # TODO(phajdan.jr): We should know exactly when the paths are absolute.
      # Convert all absolute paths to relative.
      for i in range(len(self._file_list)):
        # It depends on the command being executed (like runhooks vs sync).
        if not os.path.isabs(self._file_list[i]):
          continue
        prefix = os.path.commonprefix(
            [self.root.root_dir.lower(), self._file_list[i].lower()])
        self._file_list[i] = self._file_list[i][len(prefix):]
        # Strip any leading path separators.
        while (self._file_list[i].startswith('\\') or
                self._file_list[i].startswith('/')):
          self._file_list[i] = self._file_list[i][1:]
    self.processed = True
    if self.recursion_limit:
      # Then we can parse the DEPS file.
      self.ParseDepsFile()

      # Parse the dependencies of this dependency.
      for s in self.dependencies:
        work_queue.enqueue(s)

  def RunHooksRecursively(self, options):
    """Evaluates all hooks, running actions as needed. run()
    must have been called before to load the DEPS."""
    assert self.hooks_ran == False
    if not self.should_process or not self.recursion_limit:
      # Don't run the hook when it is above recursion_limit.
      return
    # If "--force" was specified, run all hooks regardless of what files have
    # changed.
    if self.deps_hooks:
      # TODO(maruel): If the user is using git or git-svn, then we don't know
      # what files have changed so we always run all hooks. It'd be nice to fix
      # that.
      if (options.force or
          isinstance(self.parsed_url, self.FileImpl) or
          gclient_scm.GetScmName(self.parsed_url) in ('git', None) or
          os.path.isdir(os.path.join(self.root.root_dir, self.name, '.git'))):
        for hook_dict in self.deps_hooks:
          self._RunHookAction(hook_dict, [])
      else:
        # Run hooks on the basis of whether the files from the gclient operation
        # match each hook's pattern.
        for hook_dict in self.deps_hooks:
          pattern = re.compile(hook_dict['pattern'])
          matching_file_list = [f for f in self.file_list if pattern.search(f)]
          if matching_file_list:
            self._RunHookAction(hook_dict, matching_file_list)
    for s in self.dependencies:
      s.RunHooksRecursively(options)

  def _RunHookAction(self, hook_dict, matching_file_list):
    """Runs the action from a single hook."""
    # A single DEPS file can specify multiple hooks so this function can be
    # called multiple times on a single Dependency.
    #assert self.hooks_ran == False
    self.hooks_ran = True
    logging.debug(hook_dict)
    logging.debug(matching_file_list)
    command = hook_dict['action'][:]
    if command[0] == 'python':
      # If the hook specified "python" as the first item, the action is a
      # Python script.  Run it by starting a new copy of the same
      # interpreter.
      command[0] = sys.executable

    if '$matching_files' in command:
      splice_index = command.index('$matching_files')
      command[splice_index:splice_index + 1] = matching_file_list

    try:
      gclient_utils.CheckCallAndFilterAndHeader(
          command, cwd=self.root.root_dir, always=True)
    except (gclient_utils.Error, subprocess2.CalledProcessError), e:
      # Use a discrete exit status code of 2 to indicate that a hook action
      # failed.  Users of this script may wish to treat hook action failures
      # differently from VC failures.
      print >> sys.stderr, 'Error: %s' % str(e)
      sys.exit(2)

  def subtree(self, include_all):
    """Breadth first"""
    result = []
    dependencies = self.dependencies
    for d in dependencies:
      if d.should_process or include_all:
        result.append(d)
    for d in dependencies:
      result.extend(d.subtree(include_all))
    return result

  def get_custom_deps(self, name, url):
    """Returns a custom deps if applicable."""
    if self.parent:
      url = self.parent.get_custom_deps(name, url)
    # None is a valid return value to disable a dependency.
    return self.custom_deps.get(name, url)

  @property
  def recursion_limit(self):
    """Returns > 0 if this dependency is not too recursed to be processed.

    Immutable so no need to lock."""
    return max(self.parent.recursion_limit - 1, 0)

  @property
  def deps_file(self):
    """Immutable so no need to lock."""
    return self._deps_file

  @property
  def managed(self):
    """Immutable so no need to lock."""
    return self._managed

  @property
  def safesync_url(self):
    """Immutable so no need to lock."""
    return self._safesync_url

  @property
  def should_process(self):
    """True if this dependency should be processed, i.e. checked out."""
    return self._should_process

  @property
  def parent(self):
    """Immutable so no need to lock."""
    return self._parent

  @property
  def custom_vars(self):
    """Immutable so no need to lock."""
    return self._custom_vars.copy()

  @property
  def custom_deps(self):
    """Immutable so no need to lock."""
    return self._custom_deps.copy()

  @property
  @gclient_utils.lockedmethod
  def file_list(self):
    result = self._file_list[:]
    for d in self.dependencies:
      result.extend(d.file_list)
    return tuple(result)

  def __str__(self):
    out = []
    for i in ('name', 'url', 'parsed_url', 'safesync_url', 'custom_deps',
              'custom_vars', 'deps_hooks', 'file_list', 'should_process',
              'processed', 'hooks_ran', 'deps_parsed', 'requirements'):
      # First try the native property if it exists.
      if hasattr(self, '_' + i):
        value = getattr(self, '_' + i, False)
      else:
        value = getattr(self, i, False)
      if value:
        out.append('%s: %s' % (i, value))

    for d in self.dependencies:
      out.extend(['  ' + x for x in str(d).splitlines()])
      out.append('')
    return '\n'.join(out)

  def __repr__(self):
    return '%s: %s' % (self.name, self.url)

  def hierarchy(self):
    """Returns a human-readable hierarchical reference to a Dependency."""
    out = '%s(%s)' % (self.name, self.url)
    i = self.parent
    while i and i.name:
      out = '%s(%s) -> %s' % (i.name, i.url, out)
      i = i.parent
    return out

  @property
  def root(self):
    """Returns the root node, a GClient object."""
    if not self.parent:
      # This line is to signal pylint that it could be a GClient instance.
      return self or GClient(None, None)
    return self.parent.root


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
    "linux3": "unix",
  }

  DEFAULT_CLIENT_FILE_TEXT = ("""\
solutions = [
  { "name"        : "%(solution_name)s",
    "url"         : "%(solution_url)s",
    "deps_file"   : "%(deps_file)s",
    "managed"     : %(managed)s,
    "custom_deps" : {
    },
    "safesync_url": "%(safesync_url)s",
  },
]
""")

  DEFAULT_SNAPSHOT_SOLUTION_TEXT = ("""\
  { "name"        : "%(solution_name)s",
    "url"         : "%(solution_url)s",
    "deps_file"   : "%(deps_file)s",
    "managed"     : %(managed)s,
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
    # Do not change previous behavior. Only solution level and immediate DEPS
    # are processed.
    self._recursion_limit = 2
    Dependency.__init__(self, None, None, None, None, True, None, None,
                        'unused', True)
    self._options = options
    if options.deps_os:
      enforced_os = options.deps_os.split(',')
    else:
      enforced_os = [self.DEPS_OS_CHOICES.get(sys.platform, 'unix')]
    if 'all' in enforced_os:
      enforced_os = self.DEPS_OS_CHOICES.itervalues()
    self._enforced_os = tuple(set(enforced_os))
    self._root_dir = root_dir
    self.config_content = None

  def SetConfig(self, content):
    assert not self.dependencies
    config_dict = {}
    self.config_content = content
    try:
      exec(content, config_dict)
    except SyntaxError, e:
      gclient_utils.SyntaxErrorToError('.gclient', e)
    for s in config_dict.get('solutions', []):
      try:
        tree = dict((d.name, d) for d in self.root.subtree(False))
        if s['name'] in tree:
          raise gclient_utils.Error(
              'Dependency %s specified more than once in .gclient' % s['name'])
        self.dependencies.append(Dependency(
            self, s['name'], s['url'],
            s.get('safesync_url', None),
            s.get('managed', True),
            s.get('custom_deps', {}),
            s.get('custom_vars', {}),
            s.get('deps_file', 'DEPS'),
            True))
      except KeyError:
        raise gclient_utils.Error('Invalid .gclient file. Solution is '
                                  'incomplete: %s' % s)
    # .gclient can have hooks.
    self.deps_hooks = config_dict.get('hooks', [])
    self.deps_parsed = True

  def SaveConfig(self):
    gclient_utils.FileWrite(os.path.join(self.root_dir,
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

  def SetDefaultConfig(self, solution_name, deps_file, solution_url,
                       safesync_url, managed=True):
    self.SetConfig(self.DEFAULT_CLIENT_FILE_TEXT % {
      'solution_name': solution_name,
      'solution_url': solution_url,
      'deps_file': deps_file,
      'safesync_url' : safesync_url,
      'managed': managed,
    })

  def _SaveEntries(self):
    """Creates a .gclient_entries file to record the list of unique checkouts.

    The .gclient_entries file lives in the same directory as .gclient.
    """
    # Sometimes pprint.pformat will use {', sometimes it'll use { ' ... It
    # makes testing a bit too fun.
    result = 'entries = {\n'
    for entry in self.root.subtree(False):
      # Skip over File() dependencies as we can't version them.
      if not isinstance(entry.parsed_url, self.FileImpl):
        result += '  %s: %s,\n' % (pprint.pformat(entry.name),
            pprint.pformat(entry.parsed_url))
    result += '}\n'
    file_path = os.path.join(self.root_dir, self._options.entries_filename)
    logging.info(result)
    gclient_utils.FileWrite(file_path, result)

  def _ReadEntries(self):
    """Read the .gclient_entries file for the given client.

    Returns:
      A sequence of solution names, which will be empty if there is the
      entries file hasn't been created yet.
    """
    scope = {}
    filename = os.path.join(self.root_dir, self._options.entries_filename)
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
    # Do not check safesync_url if one or more --revision flag is specified.
    if not self._options.revisions:
      for s in self.dependencies:
        if not s.managed:
          self._options.revisions.append('%s@unmanaged' % s.name)
        elif s.safesync_url:
          handle = urllib.urlopen(s.safesync_url)
          rev = handle.read().strip()
          handle.close()
          if len(rev):
            self._options.revisions.append('%s@%s' % (s.name, rev))
    if not self._options.revisions:
      return revision_overrides
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
    # Disable progress for non-tty stdout.
    if (command in ('update', 'revert') and sys.stdout.isatty() and not
        self._options.verbose):
      pm = Progress('Syncing projects', 1)
    work_queue = gclient_utils.ExecutionQueue(self._options.jobs, pm)
    for s in self.dependencies:
      work_queue.enqueue(s)
    work_queue.flush(revision_overrides, command, args, options=self._options)

    # Once all the dependencies have been processed, it's now safe to run the
    # hooks.
    if not self._options.nohooks:
      self.RunHooksRecursively(self._options)

    if command == 'update':
      # Notify the user if there is an orphaned entry in their working copy.
      # Only delete the directory if there are no changes in it, and
      # delete_unversioned_trees is set to true.
      entries = [i.name for i in self.root.subtree(False) if i.url]
      for entry, prev_url in self._ReadEntries().iteritems():
        if not prev_url:
          # entry must have been overridden via .gclient custom_deps
          continue
        # Fix path separator on Windows.
        entry_fixed = entry.replace('/', os.path.sep)
        e_dir = os.path.join(self.root_dir, entry_fixed)
        # Use entry and not entry_fixed there.
        if entry not in entries and os.path.exists(e_dir):
          file_list = []
          scm = gclient_scm.CreateSCM(prev_url, self.root_dir, entry_fixed)
          scm.status(self._options, [], file_list)
          modified_files = file_list != []
          if (not self._options.delete_unversioned_trees or
              (modified_files and not self._options.force)):
            # There are modified files in this entry. Keep warning until
            # removed.
            print(('\nWARNING: \'%s\' is no longer part of this client.  '
                   'It is recommended that you manually remove it.\n') %
                      entry_fixed)
          else:
            # Delete the entry
            print('\n________ deleting \'%s\' in \'%s\'' % (
                entry_fixed, self.root_dir))
            gclient_utils.RemoveDirectory(e_dir)
      # record the current list of entries for next time
      self._SaveEntries()
    return 0

  def PrintRevInfo(self):
    if not self.dependencies:
      raise gclient_utils.Error('No solution specified')
    # Load all the settings.
    work_queue = gclient_utils.ExecutionQueue(self._options.jobs, None)
    for s in self.dependencies:
      work_queue.enqueue(s)
    work_queue.flush({}, None, [], options=self._options)

    def GetURLAndRev(dep):
      """Returns the revision-qualified SCM url for a Dependency."""
      if dep.parsed_url is None:
        return None
      if isinstance(dep.parsed_url, self.FileImpl):
        original_url = dep.parsed_url.file_location
      else:
        original_url = dep.parsed_url
      url, _ = gclient_utils.SplitUrlRevision(original_url)
      scm = gclient_scm.CreateSCM(original_url, self.root_dir, dep.name)
      if not os.path.isdir(scm.checkout_path):
        return None
      return '%s@%s' % (url, scm.revinfo(self._options, [], None))

    if self._options.snapshot:
      new_gclient = ''
      # First level at .gclient
      for d in self.dependencies:
        entries = {}
        def GrabDeps(dep):
          """Recursively grab dependencies."""
          for d in dep.dependencies:
            entries[d.name] = GetURLAndRev(d)
            GrabDeps(d)
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
            'deps_file': d.deps_file,
            'safesync_url' : d.safesync_url or '',
            'managed': d.managed,
            'solution_deps': ''.join(custom_deps),
        }
      # Print the snapshot configuration file
      print(self.DEFAULT_SNAPSHOT_FILE_TEXT % {'solution_list': new_gclient})
    else:
      entries = {}
      for d in self.root.subtree(False):
        if self._options.actual:
          entries[d.name] = GetURLAndRev(d)
        else:
          entries[d.name] = d.parsed_url
      keys = sorted(entries.keys())
      for x in keys:
        print('%s: %s' % (x, entries[x]))
    logging.info(str(self))

  def ParseDepsFile(self):
    """No DEPS to parse for a .gclient file."""
    raise gclient_utils.Error('Internal error')

  @property
  def root_dir(self):
    """Root directory of gclient checkout."""
    return self._root_dir

  @property
  def enforced_os(self):
    """What deps_os entries that are to be parsed."""
    return self._enforced_os

  @property
  def recursion_limit(self):
    """How recursive can each dependencies in DEPS file can load DEPS file."""
    return self._recursion_limit


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
  if not args:
    print >> sys.stderr, 'Need to supply a command!'
    return 1
  root_and_entries = gclient_utils.GetGClientRootAndEntries()
  if not root_and_entries:
    print >> sys.stderr, (
        'You need to run gclient sync at least once to use \'recurse\'.\n'
        'This is because .gclient_entries needs to exist and be up to date.')
    return 1
  root, entries = root_and_entries
  scm_set = set()
  for scm in options.scm:
    scm_set.update(scm.split(','))

  # Pass in the SCM type as an env variable
  env = os.environ.copy()

  for path, url in entries.iteritems():
    scm = gclient_scm.GetScmName(url)
    if scm_set and scm not in scm_set:
      continue
    cwd = os.path.normpath(os.path.join(root, path))
    if scm:
      env['GCLIENT_SCM'] = scm
    if url:
      env['GCLIENT_URL'] = url
    subprocess2.call(args, cwd=cwd, env=env)
  return 0


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
  parser.add_option('--deps-file', default='DEPS',
                    help='overrides the default name for the DEPS file for the'
                         'main solutions and all sub-dependencies')
  parser.add_option('--unmanaged', action='store_true', default=False,
                    help='overrides the default behavior to make it possible '
                         'to have the main solution untouched by gclient '
                         '(gclient will check out unmanaged dependencies but '
                         'will never sync them)')
  parser.add_option('--git-deps', action='store_true',
                    help='sets the deps file to ".DEPS.git" instead of "DEPS"')
  (options, args) = parser.parse_args(args)
  if ((options.spec and args) or len(args) > 2 or
      (not options.spec and not args)):
    parser.error('Inconsistent arguments. Use either --spec or one or 2 args')

  client = GClient('.', options)
  if options.spec:
    client.SetConfig(options.spec)
  else:
    base_url = args[0].rstrip('/')
    if not options.name:
      name = base_url.split('/')[-1]
      if name.endswith('.git'):
        name = name[:-4]
    else:
      # specify an alternate relpath for the given URL.
      name = options.name
    deps_file = options.deps_file
    if options.git_deps:
      deps_file = '.DEPS.git'
    safesync_url = ''
    if len(args) > 1:
      safesync_url = args[1]
    client.SetDefaultConfig(name, deps_file, base_url, safesync_url,
                            managed=not options.unmanaged)
  client.SaveConfig()
  return 0


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
                         'if the src@ part is skipped. Note that specifying '
                         '--revision means your safesync_url gets ignored.')
  parser.add_option('-t', '--transitive', action='store_true',
                    help='When a revision is specified (in the DEPS file or '
                          'with the command-line flag), transitively update '
                          'the dependencies to the date of the given revision. '
                          'Only supported for SVN repositories.')
  parser.add_option('-H', '--head', action='store_true',
                    help='skips any safesync_urls specified in '
                         'configured solutions and sync to head instead')
  parser.add_option('-D', '--delete_unversioned_trees', action='store_true',
                    help='delete any dependency that have been removed from '
                         'last sync as long as there is no local modification. '
                         'Coupled with --force, it will remove them even with '
                         'local modifications')
  parser.add_option('-R', '--reset', action='store_true',
                    help='resets any local changes before updating (git only)')
  parser.add_option('-M', '--merge', action='store_true',
                    help='merge upstream changes instead of trying to '
                         'fast-forward or rebase')
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
  """Revert all modifications in every dependencies.

  That's the nuclear option to get back to a 'clean' state. It removes anything
  that shows up in svn status."""
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
  parser.add_option('-a', '--actual', action='store_true',
                    help='gets the actual checked out revisions instead of the '
                         'ones specified in the DEPS and .gclient files')
  parser.add_option('-s', '--snapshot', action='store_true',
                    help='creates a snapshot .gclient file of the current '
                         'version of all repositories to reproduce the tree, '
                         'implies -a')
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


def Parser():
  """Returns the default parser."""
  parser = optparse.OptionParser(version='%prog ' + __version__)
  parser.add_option('-j', '--jobs', default=1, type='int',
                    help='Specify how many SCM commands can run in parallel; '
                          'default=%default')
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
    if options.jobs < 1:
      parser.error('--jobs must be 1 or higher')

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
  return parser


def Main(argv):
  """Doesn't parse the arguments here, just find the right subcommand to
  execute."""
  if sys.hexversion < 0x02050000:
    print >> sys.stderr, (
        '\nYour python version is unsupported, please upgrade.\n')
  try:
    # Make stdout auto-flush so buildbot doesn't kill us during lengthy
    # operations. Python as a strong tendency to buffer sys.stdout.
    sys.stdout = gclient_utils.MakeFileAutoFlush(sys.stdout)
    # Make stdout annotated with the thread ids.
    sys.stdout = gclient_utils.MakeFileAnnotated(sys.stdout)
    # Do it late so all commands are listed.
    # Unused variable 'usage'
    # pylint: disable=W0612
    CMDhelp.usage = ('\n\nCommands are:\n' + '\n'.join([
        '  %-10s %s' % (fn[3:], Command(fn[3:]).__doc__.split('\n')[0].strip())
        for fn in dir(sys.modules[__name__]) if fn.startswith('CMD')]))
    parser = Parser()
    if argv:
      command = Command(argv[0])
      if command:
        # 'fix' the usage and the description now that we know the subcommand.
        GenUsage(parser, argv[0])
        return command(parser, argv[1:])
    # Not a known command. Default to help.
    GenUsage(parser, 'help')
    return CMDhelp(parser, argv)
  except (gclient_utils.Error, subprocess2.CalledProcessError), e:
    print >> sys.stderr, 'Error: %s' % str(e)
    return 1


if '__main__' == __name__:
  fix_encoding.fix_encoding()
  sys.exit(Main(sys.argv[1:]))

# vim: ts=2:sw=2:tw=80:et:
