# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

import config_util  # pylint: disable=F0401


# This class doesn't need an __init__ method, so we disable the warning
# pylint: disable=W0232
class Infra(config_util.Config):
  """Basic Config class for the Infrastructure repositories."""

  @staticmethod
  def fetch_spec(props):
    # This is used by [depot_tools]/testing_support/local_rietveld.py
    managed = props.get('managed', 'false').lower() == 'true'
    return {
      'type': 'gclient_git',
      'gclient_git_spec': {
        'solutions': [
          {
            'name'     : 'infra',
            'url'      : 'https://chromium.googlesource.com/infra/infra.git',
            'deps_file': '.DEPS.git',
            'managed'  : managed,
          }
        ],
      },
    }

  @staticmethod
  def expected_root(_props):
    return 'infra'


def main(argv=None):
  return Infra().handle_args(argv)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
