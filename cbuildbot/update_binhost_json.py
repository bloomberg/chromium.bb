# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Update the binhost json. Used by buildbots only."""

from __future__ import print_function

import os

from chromite.cbuildbot import binhost
from chromite.cbuildbot import constants
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import git


def _ParseArguments(argv):
  parser = commandline.ArgumentParser(description=__doc__)
  parser.add_argument('--buildroot', default=constants.SOURCE_ROOT,
                      help='Root directory where source is checked out to.')
  parser.add_argument('--skip-regen', default=True, dest='regen',
                      action='store_false',
                      help='Don\'t regenerate configs that have already been'
                           'generated.')
  opts = parser.parse_args(argv)
  opts.Freeze()
  return opts


def main(argv):
  cros_build_lib.AssertInsideChroot()
  opts = _ParseArguments(argv)

  logging.info('Generating board configs. This takes about 2m...')
  for key in sorted(binhost.GetChromePrebuiltConfigs()):
    binhost.GenConfigsForBoard(key.board, regen=opts.regen, error_code_ok=True)

  # Fetch all compat IDs.
  fetcher = binhost.CompatIdFetcher()
  keys = binhost.GetChromePrebuiltConfigs().keys()
  compat_ids = fetcher.FetchCompatIds(keys)

  # Save the PFQ configs.
  pfq_configs = binhost.PrebuiltMapping.Get(keys, compat_ids)
  filename = binhost.PrebuiltMapping.GetFilename(opts.buildroot, 'chrome')
  pfq_configs.Dump(filename)
  git.AddPath(filename)
  git.Commit(os.path.dirname(filename), 'Update PFQ config dump',
             allow_empty=True)
