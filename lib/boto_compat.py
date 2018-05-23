# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Boto compatibility functions for cbuildbot.

This module provides compatibility functions to help manage old versions of
boto/gsutil.

NOTE: This should eventually be removed as part of crbug.com/845304.
"""

from __future__ import print_function

import contextlib
import os
import tempfile

from chromite.lib import constants
from chromite.lib import cros_logging as logging
from chromite.lib import osutils

# Path to an updated cacerts.txt file, which will override the cacerts.txt file
# embedded in boto in the wrapped FixBotoCerts context. Relative to chromite/.
BOTO_CACERTS_PATH = 'third_party/boto/boto/cacerts/cacerts.txt'

BOTO_CACERTS_ABS_PATH = os.path.join(
    constants.CHROMITE_DIR,
    os.path.normpath(BOTO_CACERTS_PATH))

BOTO_CFG_CONTENTS = '''
[Boto]
ca_certificates_file = %s
''' % BOTO_CACERTS_ABS_PATH

@contextlib.contextmanager
def FixBotoCerts(activate=True, strict=False):
  """Fix for outdated cacerts.txt file in old versions of boto/gsutil."""

  if not activate:
    logging.info('FixBotoCerts skipped')
    yield
    return

  logging.info('FixBotoCerts started')

  orig_env = os.environ.copy()

  boto_cfg_path = None
  try:
    # Write a boto config file pointing to the cacerts.txt file.
    _, boto_cfg_path = tempfile.mkstemp(prefix='fix_cacerts', suffix='boto.cfg')
    osutils.WriteFile(boto_cfg_path, BOTO_CFG_CONTENTS)
    os.chmod(boto_cfg_path, 0o644)

    # Inject the new boto config file into BOTO_PATH.
    boto_path_env = os.environ.get('BOTO_PATH', '/etc/boto.cfg:~/.boto')
    os.environ['BOTO_PATH'] = '%s:%s' % (boto_path_env, boto_cfg_path)

  except Exception, e:
    if strict:
      raise e
    logging.warning('FixBotoCerts init failed: %s', e)
    # Don't make things worse; let the build continue.

  try:
    yield
  finally:
    # Restore env.
    osutils.SetEnvironment(orig_env)

    # Clean up the boto.cfg file.
    if boto_cfg_path:
      try:
        os.remove(boto_cfg_path)
      except Exception, e:
        if strict:
          raise e
        logging.warning('FixBotoCerts failed removing %s: %s', boto_cfg_path, e)
