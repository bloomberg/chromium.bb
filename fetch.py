#!/usr/bin/env python
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Tool to perform checkouts in one easy command line!

Usage:
  fetch <recipe> [--property=value [--property2=value2 ...]]

This script is a wrapper around various version control and repository
checkout commands. It requires a |recipe| name, fetches data from that
recipe in depot_tools/recipes, and then performs all necessary inits,
checkouts, pulls, fetches, etc.

Optional arguments may be passed on the command line in key-value pairs.
These parameters will be passed through to the recipe's main method.
"""

import json
import os
import subprocess
import sys
import pipes


SCRIPT_PATH = os.path.dirname(os.path.abspath(__file__))


#################################################
# Checkout class definitions.
#################################################
class Checkout(object):
  """Base class for implementing different types of checkouts.

  Attributes:
    |base|: the absolute path of the directory in which this script is run.
    |spec|: the spec for this checkout as returned by the recipe. Different
        subclasses will expect different keys in this dictionary.
    |root|: the directory into which the checkout will be performed, as returned
        by the recipe. This is a relative path from |base|.
  """
  def __init__(self, dryrun, spec, root):
    self.base = os.getcwd()
    self.dryrun = dryrun
    self.spec = spec
    self.root = root

  def exists(self):
    pass

  def init(self):
    pass

  def sync(self):
    pass


class GclientCheckout(Checkout):

  def run_gclient(self, *cmd, **kwargs):
    print 'Running: gclient %s' % ' '.join(pipes.quote(x) for x in cmd)
    if not self.dryrun:
      return subprocess.check_call(
          (sys.executable, os.path.join(SCRIPT_PATH, 'gclient.py')) + cmd,
          **kwargs)


class GitCheckout(Checkout):

  def run_git(self, *cmd, **kwargs):
    print 'Running: git %s' % ' '.join(pipes.quote(x) for x in cmd)
    if not self.dryrun:
      return subprocess.check_call(('git',) + cmd, **kwargs)


class GclientGitSvnCheckout(GclientCheckout, GitCheckout):

  def __init__(self, dryrun, spec, root):
    super(GclientGitSvnCheckout, self).__init__(dryrun, spec, root)
    assert 'solutions' in self.spec
    keys = ['solutions', 'target_os', 'target_os_only']
    gclient_spec = '\n'.join('%s = %s' % (key, self.spec[key])
                             for key in self.spec if key in keys)
    self.spec['gclient_spec'] = gclient_spec
    assert 'svn_url' in self.spec
    assert 'svn_branch' in self.spec
    assert 'svn_ref' in self.spec

  def exists(self):
    return os.path.exists(os.path.join(os.getcwd(), self.root))

  def init(self):
    # Configure and do the gclient checkout.
    self.run_gclient('config', '--spec', self.spec['gclient_spec'])
    self.run_gclient('sync')

    # Configure git.
    wd = os.path.join(self.base, self.root)
    if self.dryrun:
      print "cd %s" % wd
    self.run_git(
        'submodule', 'foreach',
        'git config -f $toplevel/.git/config submodule.$name.ignore all',
        cwd=wd)
    self.run_git('config', 'diff.ignoreSubmodules', 'all', cwd=wd)

    # Configure git-svn.
    self.run_git('svn', 'init', '--prefix=origin/', '-T',
                 self.spec['svn_branch'], self.spec['svn_url'], cwd=wd)
    self.run_git('config', 'svn-remote.svn.fetch', self.spec['svn_branch'] +
                 ':refs/remotes/origin/' + self.spec['svn_ref'], cwd=wd)
    self.run_git('svn', 'fetch', cwd=wd)

    # Configure git-svn submodules, if any.
    submodules = json.loads(self.spec.get('submodule_git_svn_spec', '{}'))
    for path, subspec in submodules.iteritems():
      subspec = submodules[path]
      ospath = os.path.join(*path.split('/'))
      wd = os.path.join(self.base, self.root, ospath)
      if self.dryrun:
        print "cd %s" % wd
      self.run_git('svn', 'init', '--prefix=origin/', '-T',
                   subspec['svn_branch'], subspec['svn_url'], cwd=wd)
      self.run_git('config', '--replace', 'svn-remote.svn.fetch',
                   subspec['svn_branch'] + ':refs/remotes/origin/' +
                   subspec['svn_ref'], cwd=wd)
      self.run_git('svn', 'fetch', cwd=wd)


CHECKOUT_TYPE_MAP = {
    'gclient':         GclientCheckout,
    'gclient_git_svn': GclientGitSvnCheckout,
    'git':             GitCheckout,
}


def CheckoutFactory(type_name, dryrun, spec, root):
  """Factory to build Checkout class instances."""
  class_ = CHECKOUT_TYPE_MAP.get(type_name)
  if not class_:
    raise KeyError('unrecognized checkout type: %s' % type_name)
  return class_(dryrun, spec, root)


#################################################
# Utility function and file entry point.
#################################################
def usage(msg=None):
  """Print help and exit."""
  if msg:
    print 'Error:', msg

  print (
"""
usage: %s [-n|--dry-run] <recipe> [--property=value [--property2=value2 ...]]
""" % os.path.basename(sys.argv[0]))
  sys.exit(bool(msg))


def handle_args(argv):
  """Gets the recipe name from the command line arguments."""
  if len(argv) <= 1:
    usage('Must specify a recipe.')
  if argv[1] in ('-h', '--help', 'help'):
    usage()

  dryrun = False
  if argv[1] in ('-n', '--dry-run'):
    dryrun = True
    argv.pop(1)

  def looks_like_arg(arg):
    return arg.startswith('--') and arg.count('=') == 1

  bad_parms = [x for x in argv[2:] if not looks_like_arg(x)]
  if bad_parms:
    usage('Got bad arguments %s' % bad_parms)

  recipe = argv[1]
  props = argv[2:]
  return dryrun, recipe, props


def run_recipe_fetch(recipe, props, aliased=False):
  """Invoke a recipe's fetch method with the passed-through args
  and return its json output as a python object."""
  recipe_path = os.path.abspath(os.path.join(SCRIPT_PATH, 'recipes', recipe))
  if not os.path.exists(recipe_path + '.py'):
    print "Could not find a recipe for %s" % recipe
    sys.exit(1)

  cmd = [sys.executable, recipe_path + '.py', 'fetch'] + props
  result = subprocess.Popen(cmd, stdout=subprocess.PIPE).communicate()[0]

  spec = json.loads(result)
  if 'alias' in spec:
    assert not aliased
    return run_recipe_fetch(
        spec['alias']['recipe'], spec['alias']['props'] + props, aliased=True)
  cmd = [sys.executable, recipe_path + '.py', 'root']
  result = subprocess.Popen(cmd, stdout=subprocess.PIPE).communicate()[0]
  root = json.loads(result)
  return spec, root


def run(dryrun, spec, root):
  """Perform a checkout with the given type and configuration.

    Args:
      dryrun: if True, don't actually execute the commands
      spec: Checkout configuration returned by the the recipe's fetch_spec
          method (checkout type, repository url, etc.).
      root: The directory into which the repo expects to be checkout out.
  """
  assert 'type' in spec
  checkout_type = spec['type']
  checkout_spec = spec['%s_spec' % checkout_type]
  try:
    checkout = CheckoutFactory(checkout_type, dryrun, checkout_spec, root)
  except KeyError:
    return 1
  if checkout.exists():
    print 'You appear to already have this checkout.'
    print 'Aborting to avoid clobbering your work.'
    return 1
  checkout.init()
  return 0


def main():
  dryrun, recipe, props = handle_args(sys.argv)
  spec, root = run_recipe_fetch(recipe, props)
  return run(dryrun, spec, root)


if __name__ == '__main__':
  sys.exit(main())
