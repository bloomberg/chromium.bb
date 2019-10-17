# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Interface for calculating dependency graphs via Portage internals"""

from __future__ import print_function

import copy
import os
import sys
import time

# These aren't available outside the SDK.
# pylint: disable=import-error
from _emerge.actions import adjust_configs
from _emerge.actions import load_emerge_config
from _emerge.create_depgraph_params import create_depgraph_params
from _emerge.depgraph import backtrack_depgraph
from _emerge.main import parse_opts
from _emerge.Package import Package
from _emerge.stdout_spinner import stdout_spinner
from portage._global_updates import _global_updates
import portage
# pylint: enable=import-error

from chromite.lib import cros_build_lib
from chromite.lib import cros_event


class DepGraphGenerator(object):
  """Grab dependency information about packages from portage.

  Typical usage:
    deps = DepGraphGenerator()
    deps.Initialize(sys.argv[1:])
    deps_tree, deps_info = deps.GenDependencyTree()
    deps_graph = deps.GenDependencyGraph(deps_tree, deps_info)
    deps.PrintTree(deps_tree)
    PrintDepsMap(deps_graph)
  """

  __slots__ = [
      'board', 'emerge', 'package_db', 'show_output', 'sysroot', 'unpack_only',
      'max_retries', 'include_bdepend'
  ]

  def __init__(self):
    self.board = None
    self.emerge = EmergeData()
    self.package_db = {}
    self.show_output = False
    self.sysroot = None
    self.unpack_only = False
    self.max_retries = int(os.environ.get('PARALLEL_EMERGE_MAX_RETRIES', 1))
    self.include_bdepend = False

  def ParseParallelEmergeArgs(self, argv):
    """Read the parallel emerge arguments from the command-line.

    We need to be compatible with emerge arg format.  We scrape arguments that
    are specific to parallel_emerge, and pass through the rest directly to
    emerge.

    Args:
      argv: arguments list

    Returns:
      Arguments that don't belong to parallel_emerge
    """
    emerge_args = []
    for arg in argv:
      # Specifically match arguments that are specific to parallel_emerge, and
      # pass through the rest.
      if arg.startswith('--board='):
        self.board = arg.replace('--board=', '')
      elif arg.startswith('--sysroot='):
        self.sysroot = arg.replace('--sysroot=', '')
      elif arg.startswith('--workon='):
        workon_str = arg.replace('--workon=', '')
        emerge_args.append('--reinstall-atoms=%s' % workon_str)
        emerge_args.append('--usepkg-exclude=%s' % workon_str)
      elif arg.startswith('--force-remote-binary='):
        force_remote_binary = arg.replace('--force-remote-binary=', '')
        emerge_args.append('--useoldpkg-atoms=%s' % force_remote_binary)
      elif arg.startswith('--retries='):
        self.max_retries = int(arg.replace('--retries=', ''))
      elif arg == '--show-output':
        self.show_output = True
      elif arg == '--rebuild':
        emerge_args.append('--rebuild-if-unbuilt')
      elif arg == '--unpackonly':
        emerge_args.append('--fetchonly')
        self.unpack_only = True
      elif arg.startswith('--eventlogfile='):
        log_file_name = arg.replace('--eventlogfile=', '')
        event_logger = cros_event.getEventFileLogger(log_file_name)
        event_logger.setKind('ParallelEmerge')
        cros_event.setEventLogger(event_logger)
      elif arg == '--include-bdepend':
        self.include_bdepend = True
      else:
        # Not one of our options, so pass through to emerge.
        emerge_args.append(arg)

    # These packages take a really long time to build, so, for expediency, we
    # are blacklisting them from automatic rebuilds because one of their
    # dependencies needs to be recompiled.
    for pkg in ('chromeos-base/chromeos-chrome',):
      emerge_args.append('--rebuild-exclude=%s' % pkg)

    return emerge_args

  def Initialize(self, args):
    """Initializer. Parses arguments and sets up portage state."""

    # Parse and strip out args that are just intended for parallel_emerge.
    emerge_args = self.ParseParallelEmergeArgs(args)

    if self.sysroot and self.board:
      cros_build_lib.Die('--sysroot and --board are incompatible.')

    # Setup various environment variables based on our current board. These
    # variables are normally setup inside emerge-${BOARD}, but since we don't
    # call that script, we have to set it up here. These variables serve to
    # point our tools at /build/BOARD and to setup cross compiles to the
    # appropriate board as configured in toolchain.conf.
    if self.board:
      self.sysroot = os.environ.get('SYSROOT',
                                    cros_build_lib.GetSysroot(self.board))

    if self.sysroot:
      os.environ['PORTAGE_CONFIGROOT'] = self.sysroot
      os.environ['SYSROOT'] = self.sysroot

    # Turn off interactive delays
    os.environ['EBEEP_IGNORE'] = '1'
    os.environ['EPAUSE_IGNORE'] = '1'
    os.environ['CLEAN_DELAY'] = '0'

    # Parse the emerge options.
    action, opts, cmdline_packages = parse_opts(emerge_args, silent=True)

    # Set environment variables based on options. Portage normally sets these
    # environment variables in emerge_main, but we can't use that function,
    # because it also does a bunch of other stuff that we don't want.
    # TODO(davidjames): Patch portage to move this logic into a function we can
    # reuse here.
    if '--debug' in opts:
      os.environ['PORTAGE_DEBUG'] = '1'
    if '--config-root' in opts:
      os.environ['PORTAGE_CONFIGROOT'] = opts['--config-root']
    if '--root' in opts:
      os.environ['ROOT'] = opts['--root']
    elif self.board and 'ROOT' not in os.environ:
      os.environ['ROOT'] = self.sysroot
    if '--accept-properties' in opts:
      os.environ['ACCEPT_PROPERTIES'] = opts['--accept-properties']

    # If we're installing packages to the board, we can disable vardb locks.
    # This is safe because we only run up to one instance of parallel_emerge in
    # parallel.
    # TODO(davidjames): Enable this for the host too.
    if self.sysroot:
      os.environ.setdefault('PORTAGE_LOCKS', 'false')

    # Now that we've setup the necessary environment variables, we can load the
    # emerge config from disk.
    # pylint: disable=unpacking-non-sequence
    settings, trees, mtimedb = load_emerge_config()

    # Add in EMERGE_DEFAULT_OPTS, if specified.
    tmpcmdline = []
    if '--ignore-default-opts' not in opts:
      tmpcmdline.extend(settings['EMERGE_DEFAULT_OPTS'].split())
    tmpcmdline.extend(emerge_args)
    action, opts, cmdline_packages = parse_opts(tmpcmdline)

    # If we're installing to the board, we want the --root-deps option so that
    # portage will install the build dependencies to that location as well.
    if self.sysroot:
      opts.setdefault('--root-deps', True)

    # Check whether our portage tree is out of date. Typically, this happens
    # when you're setting up a new portage tree, such as in setup_board and
    # make_chroot. In that case, portage applies a bunch of global updates
    # here. Once the updates are finished, we need to commit any changes
    # that the global update made to our mtimedb, and reload the config.
    #
    # Portage normally handles this logic in emerge_main, but again, we can't
    # use that function here.
    if _global_updates(trees, mtimedb['updates']):
      mtimedb.commit()
      # pylint: disable=unpacking-non-sequence
      settings, trees, mtimedb = load_emerge_config(trees=trees)

    # Setup implied options. Portage normally handles this logic in
    # emerge_main.
    if '--buildpkgonly' in opts or 'buildpkg' in settings.features:
      opts.setdefault('--buildpkg', True)
    if '--getbinpkgonly' in opts:
      opts.setdefault('--usepkgonly', True)
      opts.setdefault('--getbinpkg', True)
    if 'getbinpkg' in settings.features:
      # Per emerge_main, FEATURES=getbinpkg overrides --getbinpkg=n
      opts['--getbinpkg'] = True
    if '--getbinpkg' in opts or '--usepkgonly' in opts:
      opts.setdefault('--usepkg', True)
    if '--fetch-all-uri' in opts:
      opts.setdefault('--fetchonly', True)
    if '--skipfirst' in opts:
      opts.setdefault('--resume', True)
    if '--buildpkgonly' in opts:
      # --buildpkgonly will not merge anything, so it overrides all binary
      # package options.
      for opt in ('--getbinpkg', '--getbinpkgonly', '--usepkg', '--usepkgonly'):
        opts.pop(opt, None)
    if (settings.get('PORTAGE_DEBUG', '') == '1' and
        'python-trace' in settings.features):
      portage.debug.set_trace(True)

    # Complain about unsupported options
    for opt in ('--ask', '--ask-enter-invalid', '--resume', '--skipfirst'):
      if opt in opts:
        print('%s is not supported by parallel_emerge' % opt)
        sys.exit(1)

    # Make emerge specific adjustments to the config (e.g. colors!)
    adjust_configs(opts, trees)

    # Save our configuration so far in the emerge object
    emerge = self.emerge
    emerge.action, emerge.opts = action, opts
    emerge.settings, emerge.trees, emerge.mtimedb = settings, trees, mtimedb
    emerge.cmdline_packages = cmdline_packages
    root = settings['ROOT']
    emerge.root_config = trees[root]['root_config']

    if '--usepkg' in opts:
      emerge.trees[root]['bintree'].populate('--getbinpkg' in opts)

  def CreateDepgraph(self, emerge, packages):
    """Create an emerge depgraph object."""
    # Setup emerge options.
    emerge_opts = emerge.opts.copy()

    # Ask portage to build a dependency graph. with the options we specified
    # above.
    params = create_depgraph_params(emerge_opts, emerge.action)
    success, depgraph, favorites = backtrack_depgraph(
        emerge.settings, emerge.trees, emerge_opts, params, emerge.action,
        packages, emerge.spinner)
    emerge.depgraph = depgraph

    # Is it impossible to honor the user's request? Bail!
    if not success:
      depgraph.display_problems()
      sys.exit(1)

    emerge.depgraph = depgraph
    emerge.favorites = favorites

    # Prime and flush emerge caches.
    root = emerge.settings['ROOT']
    vardb = emerge.trees[root]['vartree'].dbapi
    if '--pretend' not in emerge.opts:
      vardb.counter_tick()
    vardb.flush_cache()

  def GenDependencyTree(self):
    """Get dependency tree info from emerge.

    Returns:
      Dependency tree
    """
    start = time.time()

    emerge = self.emerge

    # Create a list of packages to merge
    packages = set(emerge.cmdline_packages[:])

    # Tell emerge to be quiet. We print plenty of info ourselves so we don't
    # need any extra output from portage.
    portage.util.noiselimit = -1

    # My favorite feature: The silent spinner. It doesn't spin. Ever.
    # I'd disable the colors by default too, but they look kind of cool.
    emerge.spinner = stdout_spinner()
    emerge.spinner.update = emerge.spinner.update_quiet

    if '--quiet' not in emerge.opts:
      print('Calculating deps...')

    with cros_event.newEvent(task_name='GenerateDepTree'):
      self.CreateDepgraph(emerge, packages)
      depgraph = emerge.depgraph

    # Build our own tree from the emerge digraph.
    deps_tree = {}
    # pylint: disable=protected-access
    digraph = depgraph._dynamic_config.digraph
    root = emerge.settings['ROOT']
    final_db = depgraph._dynamic_config._filtered_trees[root]['graph_db']
    for node, node_deps in digraph.nodes.items():
      # Calculate dependency packages that need to be installed first. Each
      # child on the digraph is a dependency. The "operation" field specifies
      # what we're doing (e.g. merge, uninstall, etc.). The "priorities" array
      # contains the type of dependency (e.g. build, runtime, runtime_post,
      # etc.)
      #
      # Portage refers to the identifiers for packages as a CPV. This acronym
      # stands for Component/Path/Version.
      #
      # Here's an example CPV: chromeos-base/power_manager-0.0.1-r1
      # Split up, this CPV would be:
      #   C -- Component: chromeos-base
      #   P -- Path:      power_manager
      #   V -- Version:   0.0.1-r1
      #
      # We just refer to CPVs as packages here because it's easier.
      deps = {}
      for child, priorities in node_deps[0].items():
        if isinstance(child, Package) and (self.include_bdepend or
                                           child.root == root):
          cpv = str(child.cpv)
          action = str(child.operation)

          # If we're uninstalling a package, check whether Portage is
          # installing a replacement. If so, just depend on the installation
          # of the new package, because the old package will automatically
          # be uninstalled at that time.
          if action == 'uninstall':
            for pkg in final_db.match_pkgs(child.slot_atom):
              cpv = str(pkg.cpv)
              action = 'merge'
              break

          deps[cpv] = dict(
              action=action, deptypes=[str(x) for x in priorities], deps={})

      # We've built our list of deps, so we can add our package to the tree.
      if isinstance(node, Package) and (self.include_bdepend or
                                        child.root == root):
        deps_tree[str(node.cpv)] = dict(action=str(node.operation), deps=deps)

    # Ask portage for its install plan, so that we can only throw out
    # dependencies that portage throws out.
    deps_info = {}
    for pkg in depgraph.altlist():
      if isinstance(pkg, Package):
        # Save off info about the package
        deps_info[str(pkg.cpv)] = {'idx': len(deps_info)}

    seconds = time.time() - start
    if '--quiet' not in emerge.opts:
      print('Deps calculated in %dm%.1fs' % (seconds // 60, seconds % 60))

    return deps_tree, deps_info

  def PrintTree(self, deps, depth=''):
    """Print the deps we have seen in the emerge output.

    Args:
      deps: Dependency tree structure.
      depth: Allows printing the tree recursively, with indentation.
    """
    for entry in sorted(deps):
      action = deps[entry]['action']
      print('%s %s (%s)' % (depth, entry, action))
      self.PrintTree(deps[entry]['deps'], depth=depth + '  ')

  def GenDependencyGraph(self, deps_tree, deps_info):
    """Generate a doubly linked dependency graph.

    Args:
      deps_tree: Dependency tree structure.
      deps_info: More details on the dependencies.

    Returns:
      Deps graph in the form of a dict of packages, with each package
      specifying a "needs" list and "provides" list.
    """
    emerge = self.emerge

    # deps_map is the actual dependency graph.
    #
    # Each package specifies a "needs" list and a "provides" list. The "needs"
    # list indicates which packages we depend on. The "provides" list
    # indicates the reverse dependencies -- what packages need us.
    #
    # We also provide some other information in the dependency graph:
    #  - action: What we're planning on doing with this package. Generally,
    #            "merge", "nomerge", or "uninstall"
    deps_map = {}

    def ReverseTree(packages):
      """Convert tree to digraph.

      Take the tree of package -> requirements and reverse it to a digraph of
      buildable packages -> packages they unblock.

      Args:
        packages: Tree(s) of dependencies.

      Returns:
        Unsanitized digraph.
      """
      binpkg_phases = set(['setup', 'preinst', 'postinst'])
      needed_dep_types = set([
          'blocker', 'buildtime', 'buildtime_slot_op', 'runtime',
          'runtime_slot_op'
      ])
      ignored_dep_types = set(['ignored', 'runtime_post', 'soft'])

      # There's a bug in the Portage library where it always returns 'optional'
      # and never 'buildtime' for the digraph while --usepkg is enabled; even
      # when the package is being rebuilt. To work around this, we treat
      # 'optional' as needed when we are using --usepkg. See crbug.com/756240 .
      if '--usepkg' in self.emerge.opts:
        needed_dep_types.add('optional')
      else:
        ignored_dep_types.add('optional')

      all_dep_types = ignored_dep_types | needed_dep_types
      for pkg in packages:

        # Create an entry for the package
        action = packages[pkg]['action']
        default_pkg = {
            'needs': {},
            'provides': set(),
            'action': action,
            'nodeps': False,
            'binary': False
        }
        this_pkg = deps_map.setdefault(pkg, default_pkg)

        if pkg in deps_info:
          this_pkg['idx'] = deps_info[pkg]['idx']

        # If a package doesn't have any defined phases that might use the
        # dependent packages (i.e. pkg_setup, pkg_preinst, or pkg_postinst),
        # we can install this package before its deps are ready.
        emerge_pkg = self.package_db.get(pkg)
        if emerge_pkg and emerge_pkg.type_name == 'binary':
          this_pkg['binary'] = True
          defined_phases = emerge_pkg.defined_phases
          defined_binpkg_phases = binpkg_phases.intersection(defined_phases)
          if not defined_binpkg_phases:
            this_pkg['nodeps'] = True

        # Create entries for dependencies of this package first.
        ReverseTree(packages[pkg]['deps'])

        # Add dependencies to this package.
        for dep, dep_item in packages[pkg]['deps'].items():
          # We only need to enforce strict ordering of dependencies if the
          # dependency is a blocker, or is a buildtime or runtime dependency.
          # (I.e., ignored, optional, and runtime_post dependencies don't
          # depend on ordering.)
          dep_types = dep_item['deptypes']
          if needed_dep_types.intersection(dep_types):
            deps_map[dep]['provides'].add(pkg)
            this_pkg['needs'][dep] = '/'.join(dep_types)

          # Verify we processed all appropriate dependency types.
          unknown_dep_types = set(dep_types) - all_dep_types
          if unknown_dep_types:
            print('Unknown dependency types found:')
            print('  %s -> %s (%s)' % (pkg, dep, '/'.join(unknown_dep_types)))
            sys.exit(1)

          # If there's a blocker, Portage may need to move files from one
          # package to another, which requires editing the CONTENTS files of
          # both packages. To avoid race conditions while editing this file,
          # the two packages must not be installed in parallel, so we can't
          # safely ignore dependencies. See https://crbug.com/202428.
          if 'blocker' in dep_types:
            this_pkg['nodeps'] = False

    def FindCycles():
      """Find cycles in the dependency tree.

      Returns:
        A dict mapping cyclic packages to a dict of the deps that cause
        cycles. For each dep that causes cycles, it returns an example
        traversal of the graph that shows the cycle.
      """

      def FindCyclesAtNode(pkg, cycles, unresolved, resolved):
        """Find cycles in cyclic dependencies starting at specified package.

        Args:
          pkg: Package identifier.
          cycles: A dict mapping cyclic packages to a dict of the deps that
                  cause cycles. For each dep that causes cycles, it returns an
                  example traversal of the graph that shows the cycle.
          unresolved: Nodes that have been visited but are not fully processed.
          resolved: Nodes that have been visited and are fully processed.
        """
        pkg_cycles = cycles.get(pkg)
        if pkg in resolved and not pkg_cycles:
          # If we already looked at this package, and found no cyclic
          # dependencies, we can stop now.
          return
        unresolved.append(pkg)
        for dep in deps_map[pkg]['needs']:
          if dep in unresolved:
            idx = unresolved.index(dep)
            mycycle = unresolved[idx:] + [dep]
            for i in range(len(mycycle) - 1):
              pkg1, pkg2 = mycycle[i], mycycle[i + 1]
              cycles.setdefault(pkg1, {}).setdefault(pkg2, mycycle)
          elif not pkg_cycles or dep not in pkg_cycles:
            # Looks like we haven't seen this edge before.
            FindCyclesAtNode(dep, cycles, unresolved, resolved)
        unresolved.pop()
        resolved.add(pkg)

      cycles, unresolved, resolved = {}, [], set()
      for pkg in deps_map:
        FindCyclesAtNode(pkg, cycles, unresolved, resolved)
      return cycles

    def RemoveUnusedPackages():
      """Remove installed packages, propagating dependencies."""
      # Schedule packages that aren't on the install list for removal
      rm_pkgs = set(deps_map.keys()) - set(deps_info.keys())

      # Remove the packages we don't want, simplifying the graph and making
      # it easier for us to crack cycles.
      for pkg in sorted(rm_pkgs):
        this_pkg = deps_map[pkg]
        needs = this_pkg['needs']
        provides = this_pkg['provides']
        for dep in needs:
          dep_provides = deps_map[dep]['provides']
          dep_provides.update(provides)
          dep_provides.discard(pkg)
          dep_provides.discard(dep)
        for target in provides:
          target_needs = deps_map[target]['needs']
          target_needs.update(needs)
          target_needs.pop(pkg, None)
          target_needs.pop(target, None)
        del deps_map[pkg]

    def PrintCycleBreak(basedep, dep, mycycle):
      """Print details about a cycle that we are planning on breaking.

      We are breaking a cycle where dep needs basedep. mycycle is an
      example cycle which contains dep -> basedep.
      """

      needs = deps_map[dep]['needs']
      depinfo = needs.get(basedep, 'deleted')

      # It's OK to swap install order for blockers, as long as the two
      # packages aren't installed in parallel. If there is a cycle, then
      # we know the packages depend on each other already, so we can drop the
      # blocker safely without printing a warning.
      if depinfo == 'blocker':
        return

      # Notify the user that we're breaking a cycle.
      print('Breaking %s -> %s (%s)' % (dep, basedep, depinfo))

      # Show cycle.
      for i in range(len(mycycle) - 1):
        pkg1, pkg2 = mycycle[i], mycycle[i + 1]
        needs = deps_map[pkg1]['needs']
        depinfo = needs.get(pkg2, 'deleted')
        if pkg1 == dep and pkg2 == basedep:
          depinfo = depinfo + ', deleting'
        print('  %s -> %s (%s)' % (pkg1, pkg2, depinfo))

    def SanitizeTree():
      """Remove circular dependencies.

      We prune all dependencies involved in cycles that go against the emerge
      ordering. This has a nice property: we're guaranteed to merge
      dependencies in the same order that portage does.

      Because we don't treat any dependencies as "soft" unless they're killed
      by a cycle, we pay attention to a larger number of dependencies when
      merging. This hurts performance a bit, but helps reliability.
      """
      start = time.time()
      cycles = FindCycles()
      while cycles:
        for dep, mycycles in cycles.items():
          for basedep, mycycle in mycycles.items():
            if deps_info[basedep]['idx'] >= deps_info[dep]['idx']:
              if '--quiet' not in emerge.opts:
                PrintCycleBreak(basedep, dep, mycycle)
              del deps_map[dep]['needs'][basedep]
              deps_map[basedep]['provides'].remove(dep)
        cycles = FindCycles()
      seconds = time.time() - start
      if '--quiet' not in emerge.opts and seconds >= 0.1:
        print('Tree sanitized in %dm%.1fs' % (seconds // 60, seconds % 60))

    def FindRecursiveProvides(pkg, seen):
      """Find all nodes that require a particular package.

      Assumes that graph is acyclic.

      Args:
        pkg: Package identifier.
        seen: Nodes that have been visited so far.
      """
      if pkg in seen:
        return
      seen.add(pkg)
      info = deps_map[pkg]
      info['tprovides'] = info['provides'].copy()
      for dep in info['provides']:
        FindRecursiveProvides(dep, seen)
        info['tprovides'].update(deps_map[dep]['tprovides'])

    ReverseTree(deps_tree)

    # We need to remove unused packages so that we can use the dependency
    # ordering of the install process to show us what cycles to crack.
    RemoveUnusedPackages()
    SanitizeTree()
    seen = set()
    for pkg in deps_map:
      FindRecursiveProvides(pkg, seen)
    return deps_map

  def PrintInstallPlan(self, deps_map):
    """Print an emerge-style install plan.

    The install plan lists what packages we're installing, in order.
    It's useful for understanding what parallel_emerge is doing.

    Args:
      deps_map: The dependency graph.
    """

    def InstallPlanAtNode(target, deps_map):
      nodes = []
      nodes.append(target)
      for dep in deps_map[target]['provides']:
        del deps_map[dep]['needs'][target]
        if not deps_map[dep]['needs']:
          nodes.extend(InstallPlanAtNode(dep, deps_map))
      return nodes

    deps_map = copy.deepcopy(deps_map)
    install_plan = []
    plan = set()
    for target, info in deps_map.items():
      if not info['needs'] and target not in plan:
        for item in InstallPlanAtNode(target, deps_map):
          plan.add(item)
          install_plan.append(self.package_db[item])

    for pkg in plan:
      del deps_map[pkg]

    if deps_map:
      print('Cyclic dependencies:', ' '.join(deps_map))
      PrintDepsMap(deps_map)
      sys.exit(1)

    self.emerge.depgraph.display(install_plan)


class EmergeData(object):
  """This simple struct holds various emerge variables.

  This struct helps us easily pass emerge variables around as a unit.
  These variables are used for calculating dependencies and installing
  packages.
  """

  __slots__ = [
      'action', 'cmdline_packages', 'depgraph', 'favorites', 'mtimedb', 'opts',
      'root_config', 'scheduler_graph', 'settings', 'spinner', 'trees'
  ]

  def __init__(self):
    # The action the user requested. If the user is installing packages, this
    # is None. If the user is doing anything other than installing packages,
    # this will contain the action name, which will map exactly to the
    # long-form name of the associated emerge option.
    #
    # Example: If you call parallel_emerge --unmerge package, the action name
    #          will be "unmerge"
    self.action = None

    # The list of packages the user passed on the command-line.
    self.cmdline_packages = None

    # The emerge dependency graph. It'll contain all the packages involved in
    # this merge, along with their versions.
    self.depgraph = None

    # The list of candidates to add to the world file.
    self.favorites = None

    # A dict of the options passed to emerge. This dict has been cleaned up
    # a bit by parse_opts, so that it's a bit easier for the emerge code to
    # look at the options.
    #
    # Emerge takes a few shortcuts in its cleanup process to make parsing of
    # the options dict easier. For example, if you pass in "--usepkg=n", the
    # "--usepkg" flag is just left out of the dictionary altogether. Because
    # --usepkg=n is the default, this makes parsing easier, because emerge
    # can just assume that if "--usepkg" is in the dictionary, it's enabled.
    #
    # These cleanup processes aren't applied to all options. For example, the
    # --with-bdeps flag is passed in as-is.  For a full list of the cleanups
    # applied by emerge, see the parse_opts function in the _emerge.main
    # package.
    self.opts = None

    # A dictionary used by portage to maintain global state. This state is
    # loaded from disk when portage starts up, and saved to disk whenever we
    # call mtimedb.commit().
    #
    # This database contains information about global updates (i.e., what
    # version of portage we have) and what we're currently doing. Portage
    # saves what it is currently doing in this database so that it can be
    # resumed when you call it with the --resume option.
    #
    # parallel_emerge does not save what it is currently doing in the mtimedb,
    # so we do not support the --resume option.
    self.mtimedb = None

    # The portage configuration for our current root. This contains the portage
    # settings (see below) and the three portage trees for our current root.
    # (The three portage trees are explained below, in the documentation for
    #  the "trees" member.)
    self.root_config = None

    # The scheduler graph is used by emerge to calculate what packages to
    # install. We don't actually install any deps, so this isn't really used,
    # but we pass it in to the Scheduler object anyway.
    self.scheduler_graph = None

    # Portage settings for our current session. Most of these settings are set
    # in make.conf inside our current install root.
    self.settings = None

    # The spinner, which spews stuff to stdout to indicate that portage is
    # doing something. We maintain our own spinner, so we set the portage
    # spinner to "silent" mode.
    self.spinner = None

    # The portage trees. There are separate portage trees for each root. To get
    # the portage tree for the current root, you can look in self.trees[root],
    # where root = self.settings["ROOT"].
    #
    # In each root, there are three trees: vartree, porttree, and bintree.
    #  - vartree: A database of the currently-installed packages.
    #  - porttree: A database of ebuilds, that can be used to build packages.
    #  - bintree: A database of binary packages.
    self.trees = None


def PrintDepsMap(deps_map):
  """Print dependency graph, for each package list it's prerequisites."""
  for i in sorted(deps_map):
    print('%s: (%s) needs' % (i, deps_map[i]['action']))
    needs = deps_map[i]['needs']
    for j in sorted(needs):
      print('    %s' % (j))
    if not needs:
      print('    no dependencies')
