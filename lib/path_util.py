# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Handle paths."""

from __future__ import print_function

import collections
import os
import tempfile

from chromite.cbuildbot import constants
from chromite.lib import bootstrap_lib
from chromite.lib import git
from chromite.lib import osutils
from chromite.lib import workspace_lib


GENERAL_CACHE_DIR = '.cache'
CHROME_CACHE_DIR = '.cros_cache'

CHECKOUT_TYPE_UNKNOWN = 'unknown'
CHECKOUT_TYPE_GCLIENT = 'gclient'
CHECKOUT_TYPE_REPO = 'repo'
CHECKOUT_TYPE_SUBMODULE = 'submodule'
CHECKOUT_TYPE_SDK_BOOTSTRAP = 'bootstrap'

CheckoutInfo = collections.namedtuple(
    'CheckoutInfo', ['type', 'root', 'chrome_src_dir'])


def _IsChromiumGobSubmodule(path):
  """Return True if |path| is a git submodule from Chromium."""
  submodule_git = os.path.join(path, '.git')
  return git.IsSubmoduleCheckoutRoot(submodule_git, 'origin',
                                     constants.CHROMIUM_GOB_URL)


def _IsSdkBootstrapCheckout(path):
  """Return True if |path| is an SDK bootstrap.

  A bootstrap is a lone git checkout of chromite. It cannot be managed by repo.
  Underneath this bootstrap chromite, there are several SDK checkouts, each
  managed by repo.
  """
  submodule_git = os.path.join(path, '.git')
  if not git.IsSubmoduleCheckoutRoot(submodule_git, 'origin',
                                     constants.CHROMITE_URL):
    # Not a git checkout of chromite.
    return False

  # This could be an SDK under sdk_checkouts or the parent bootstrap.
  # It'll be an SDK checkout if it has a parent ".repo".
  if git.FindRepoDir(path):
    # It is managed by repo, therefore it is a child SDK checkout.
    return False

  return True


def DetermineCheckout(cwd):
  """Gather information on the checkout we are in.

  There are several checkout types, as defined by CHECKOUT_TYPE_XXX variables.
  This function determines what checkout type |cwd| is in, for example, if |cwd|
  belongs to a `repo` checkout.

  There is a special case when |cwd| is a child SDK checkout of a bootstrap
  chromite (e.g. something under chromite/sdk_checkouts/xxx.yyy.zzz/). This
  case should report that |cwd| belongs to a bootstrap checkout instead of the
  `repo` checkout of the "xxx.yyy.zzz" child SDK.

  Returns:
    A CheckoutInfo object with these attributes:
      type: The type of checkout.  Valid values are CHECKOUT_TYPE_*.
      root: The root of the checkout.
      chrome_src_dir: If the checkout is a Chrome checkout, the path to the
        Chrome src/ directory.
  """
  checkout_type = CHECKOUT_TYPE_UNKNOWN
  root, path = None, None

  # Check for SDK bootstrap first because it goes top to bottom.
  # If we do it bottom to top, we'll hit chromite/sdk_checkouts/*/.repo first
  # and will wrongly conclude that this is a repo checkout. So we go top down
  # to visit chromite/ first.
  for path in osutils.IteratePaths(cwd):
    if _IsSdkBootstrapCheckout(path):
      checkout_type = CHECKOUT_TYPE_SDK_BOOTSTRAP
      break
  else:
    for path in osutils.IteratePathParents(cwd):
      gclient_file = os.path.join(path, '.gclient')
      if os.path.exists(gclient_file):
        checkout_type = CHECKOUT_TYPE_GCLIENT
        break
      if _IsChromiumGobSubmodule(path):
        checkout_type = CHECKOUT_TYPE_SUBMODULE
        break
      repo_dir = os.path.join(path, '.repo')
      if os.path.isdir(repo_dir):
        checkout_type = CHECKOUT_TYPE_REPO
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


def FindCacheDir():
  """Returns the cache directory location based on the checkout type."""
  cwd = os.getcwd()
  checkout = DetermineCheckout(cwd)
  path = None
  if checkout.type == CHECKOUT_TYPE_REPO:
    path = os.path.join(checkout.root, GENERAL_CACHE_DIR)
  elif checkout.type == CHECKOUT_TYPE_SDK_BOOTSTRAP:
    path = os.path.join(checkout.root, bootstrap_lib.SDK_CHECKOUTS,
                        GENERAL_CACHE_DIR)
  elif checkout.type in (CHECKOUT_TYPE_GCLIENT, CHECKOUT_TYPE_SUBMODULE):
    path = os.path.join(checkout.root, CHROME_CACHE_DIR)
  elif checkout.type == CHECKOUT_TYPE_UNKNOWN:
    # We could be in a workspace.
    if workspace_lib.WorkspacePath(cwd):
      bootstrap_root = bootstrap_lib.FindBootstrapPath()
      if bootstrap_root:
        path = os.path.join(bootstrap_root, bootstrap_lib.SDK_CHECKOUTS,
                            GENERAL_CACHE_DIR)
    if not path:
      path = os.path.join(tempfile.gettempdir(), 'chromeos-cache')
  else:
    raise AssertionError('Unexpected type %s' % checkout.type)

  return path


def GetCacheDir():
  """Returns the current cache dir."""
  return os.environ.get(constants.SHARED_CACHE_ENVVAR, FindCacheDir())
