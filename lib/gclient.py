# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common functions used for syncing Chrome."""

import pprint
import socket

from chromite.lib import cros_build_lib

SVN_MIRROR_URL = 'svn://svn-mirror.golo.chromium.org'
STATUS_URL = 'https://chromium-status.appspot.com/current?format=json'


def _UseGoloMirror():
  """Check whether to use the golo.chromium.org mirrors.

  This function returns whether or not we should use the mirrors from
  golo.chromium.org, which are only accessible from within that subdomain.
  """
  return socket.getfqdn().endswith('.golo.chromium.org')


def GetBaseURLs():
  """Get the base URLs for checking out Chromium and Chrome."""
  if _UseGoloMirror():
    external_url = '%s/chrome' % SVN_MIRROR_URL
    internal_url = '%s/chrome-internal' % SVN_MIRROR_URL
  else:
    external_url = 'http://src.chromium.org/svn'
    internal_url = 'svn://svn.chromium.org/chrome-internal'

  return external_url, internal_url


def _GetGclientURLs(internal, rev):
  """Get the URLs to use in gclient file.

  Args: See WriteConfigFile below.
  """
  use_trunk = rev is None or isinstance(rev, (int, long))
  external_url, internal_url = GetBaseURLs()
  auxiliary_url = None
  name = 'src' if use_trunk else 'CHROME_DEPS'
  if use_trunk:
    rev_str = '@%s' % rev if rev else ''
    primary_url = '%s/trunk/src%s' % (external_url, rev_str)
    if internal:
      auxiliary_url = '%s/trunk/src-internal' % internal_url
  elif internal:
    # TODO(petermayo): Fall back to the archive directory if needed.
    primary_url = '%s/trunk/tools/buildspec/releases/%s' % (internal_url, rev)
  else:
    primary_url = '%s/releases/%s' % (external_url, rev)
  return primary_url, auxiliary_url, name


def _GetGclientSolutions(internal, use_pdf, rev):
  """Get the solutions array to write to the gclient file.

  Args: See WriteConfigFile below.
  """
  primary_url, auxiliary_url, name = _GetGclientURLs(internal, rev)
  custom_deps, custom_vars = {}, {}
  if not use_pdf:
    custom_deps.update({'src/pdf': None, 'src-pdf': None})
  if _UseGoloMirror():
    custom_vars.update({
      'svn_url': SVN_MIRROR_URL,
      'webkit_trunk': '%s/webkit-readonly/trunk' % SVN_MIRROR_URL,
      'googlecode_url': SVN_MIRROR_URL + '/%s',
      'sourceforge_url': SVN_MIRROR_URL + '/%(repo)s'
    })

  # TODO(petermayo): Update DEPS file to exclude LayoutTests on Chrome OS.
  custom_deps['src/third_party/WebKit/LayoutTests'] = None

  solutions = [{'name': name,
                'url': primary_url,
                'custom_deps': custom_deps,
                'custom_vars': custom_vars}]
  if auxiliary_url is not None:
    # If we're syncing to TOT, we rely on gclient sync --transitive to figure
    # out the approximate revision to pull from the internal DEPS.
    solutions.append({'name': 'aux_src', 'url': auxiliary_url})
  return solutions


def _GetGclientSpec(internal, use_pdf, rev):
  """Return a formatted gclient spec.

  Args: See WriteConfigFile below.
  """
  solutions = _GetGclientSolutions(internal=internal, use_pdf=use_pdf, rev=rev)
  return 'solutions = %s\n' % pprint.pformat(solutions)


def WriteConfigFile(gclient, cwd, internal, use_pdf, rev):
  """Initialize the specified directory as a gclient checkout.

  For gclient documentation, see:
    http://src.chromium.org/svn/trunk/tools/depot_tools/README.gclient

  Args:
    gclient: Path to gclient.
    cwd: Directory to sync.
    internal: Whether you want an internal checkout.
    use_pdf: Whether you want the PDF source code.
    rev: Revision or tag to use. If None, use the latest from trunk. If this is
      a number, use the specified revision. If this is a string, use the
      specified tag.
  """
  spec = _GetGclientSpec(internal=internal, use_pdf=use_pdf, rev=rev)
  cmd = [gclient, 'config', '--spec', spec]
  cros_build_lib.RunCommand(cmd, cwd=cwd)


def Revert(gclient, cwd):
  """Revert all local changes.

  Args:
    gclient: Path to gclient.
    cwd: Directory to revert.
  """
  cros_build_lib.RunCommand([gclient, 'revert', '--nohooks'], cwd=cwd)


def Sync(gclient, cwd, reset=False):
  """Sync the specified directory using gclient.

  Args:
    gclient: Path to gclient.
    cwd: Directory to sync.
    reset: Reset to pristine version of the source code.
  """
  cmd = [gclient, 'sync', '--verbose', '--nohooks', '--transitive',
         '--manually_grab_svn_rev']
  if reset:
    cmd += ['--reset', '--force', '--delete_unversioned_trees']
  cros_build_lib.RunCommand(cmd, cwd=cwd)
