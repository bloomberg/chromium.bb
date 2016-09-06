# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This is left for compatibility with hooks that still use GYP_DEFINES."""

import glob
import os
import shlex
import sys

script_dir = os.path.dirname(os.path.realpath(__file__))
chrome_src = os.path.abspath(os.path.join(script_dir, os.pardir))

def GetSupplementalFiles():
  """Returns a list of the supplemental files that are included in all GYP
  sources."""
  return glob.glob(os.path.join(chrome_src, '*', 'supplement.gypi'))


def ProcessGypDefinesItems(items):
  """Converts a list of strings to a list of key-value pairs."""
  result = []
  for item in items:
    tokens = item.split('=', 1)
    # Some GYP variables have hyphens, which we don't support.
    if len(tokens) == 2:
      result += [(tokens[0], tokens[1])]
    else:
      # No value supplied, treat it as a boolean and set it. Note that we
      # use the string '1' here so we have a consistent definition whether
      # you do 'foo=1' or 'foo'.
      result += [(tokens[0], '1')]
  return result


def GetGypVars(supplemental_files):
  """Returns a dictionary of all GYP vars."""
  # Find the .gyp directory in the user's home directory.
  home_dot_gyp = os.environ.get('GYP_CONFIG_DIR', None)
  if home_dot_gyp:
    home_dot_gyp = os.path.expanduser(home_dot_gyp)
  if not home_dot_gyp:
    home_vars = ['HOME']
    if sys.platform in ('cygwin', 'win32'):
      home_vars.append('USERPROFILE')
    for home_var in home_vars:
      home = os.getenv(home_var)
      if home != None:
        home_dot_gyp = os.path.join(home, '.gyp')
        if not os.path.exists(home_dot_gyp):
          home_dot_gyp = None
        else:
          break

  if home_dot_gyp:
    include_gypi = os.path.join(home_dot_gyp, "include.gypi")
    if os.path.exists(include_gypi):
      supplemental_files += [include_gypi]

  # GYP defines from the supplemental.gypi files.
  supp_items = []
  for supplement in supplemental_files:
    with open(supplement, 'r') as f:
      try:
        file_data = eval(f.read(), {'__builtins__': None}, None)
      except SyntaxError, e:
        e.filename = os.path.abspath(supplement)
        raise
      variables = file_data.get('variables', [])
      for v in variables:
        supp_items += [(v, str(variables[v]))]

  # GYP defines from the environment.
  env_items = ProcessGypDefinesItems(
      shlex.split(os.environ.get('GYP_DEFINES', '')))

  return dict(supp_items + env_items)


def main():
  print 'gyp_chromium no longer works. Use GN.'
  sys.exit(1)


if __name__ == '__main__':
  sys.exit(main())
