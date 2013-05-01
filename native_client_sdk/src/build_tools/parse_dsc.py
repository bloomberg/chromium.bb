# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import fnmatch
import optparse
import os
import sys

# 'KEY' : ( <TYPE>, [Accepted Values], <Required?>)
DSC_FORMAT = {
    'DISABLE': (bool, [True, False], False),
    'DISABLE_PACKAGE': (bool, [True, False], False),
    'TOOLS' : (list, ['newlib:arm', 'newlib:x64', 'newlib:x86', 'newlib',
                      'glibc', 'pnacl', 'win', 'linux'], True),
    'CONFIGS' : (list, ['Debug', 'Release'], False),
    'PREREQ' : (list, '', False),
    'TARGETS' : (list, {
        'NAME': (str, '', True),
        # main = nexe target
        # lib = library target
        # so = shared object target, automatically added to NMF
        # so-standalone =  shared object target, not put into NMF
        'TYPE': (str, ['main', 'lib', 'so', 'so-standalone'], True),
        'SOURCES': (list, '', True),
        'CCFLAGS': (list, '', False),
        'CXXFLAGS': (list, '', False),
        'DEFINES': (list, '', False),
        'LDFLAGS': (list, '', False),
        'INCLUDES': (list, '', False),
        'LIBS' : (list, '', False),
        'DEPS' : (list, '', False)
    }, True),
    'HEADERS': (list, {
        'FILES': (list, '', True),
        'DEST': (str, '', True),
    }, False),
    'SEARCH': (list, '', False),
    'POST': (str, '', False),
    'PRE': (str, '', False),
    'DEST': (str, ['examples/getting_started', 'examples/api',
                   'examples/demo', 'examples/tutorial',
                   'src', 'testlibs', 'tests'], True),
    'NAME': (str, '', False),
    'DATA': (list, '', False),
    'TITLE': (str, '', False),
    'GROUP': (str, '', False),
    'EXPERIMENTAL': (bool, [True, False], False),
}

def IgnoreMsgFunc(test):
  pass


def ErrorMsgFunc(text):
  sys.stderr.write(text + '\n')


def ValidateFormat(src, dsc_format, ErrorMsg=ErrorMsgFunc):
  failed = False

  # Verify all required keys are there
  for key in dsc_format:
    (exp_type, exp_value, required) = dsc_format[key]
    if required and key not in src:
      ErrorMsg('Missing required key %s.' % key)
      failed = True

  # For each provided key, verify it's valid
  for key in src:
    # Verify the key is known
    if key not in dsc_format:
      ErrorMsg('Unexpected key %s.' % key)
      failed = True
      continue

    exp_type, exp_value, required = dsc_format[key]
    value = src[key]

    # Verify the key is of the expected type
    if exp_type != type(value):
      ErrorMsg('Key %s expects %s not %s.' % (
          key, exp_type.__name__.upper(), type(value).__name__.upper()))
      failed = True
      continue

    # Verify the value is non-empty if required
    if required and not value:
      ErrorMsg('Expected non-empty value for %s.' % key)
      failed = True
      continue

    # If it's a bool, the expected values are always True or False.
    if exp_type is bool:
      continue

    # If it's a string and there are expected values, make sure it matches
    if exp_type is str:
      if type(exp_value) is list and exp_value:
        if value not in exp_value:
          ErrorMsg("Value '%s' not expected for %s." % (value, key))
          failed = True
      continue

    # if it's a list, then we need to validate the values
    if exp_type is list:
      # If we expect a dictionary, then call this recursively
      if type(exp_value) is dict:
        for val in value:
          if not ValidateFormat(val, exp_value, ErrorMsg):
            failed = True
        continue
      # If we expect a list of strings
      if type(exp_value) is str:
        for val in value:
          if type(val) is not str:
            ErrorMsg('Value %s in %s is not a string.' % (val, key))
            failed = True
        continue
      # if we expect a particular string
      if type(exp_value) is list:
        for val in value:
          if val not in exp_value:
            ErrorMsg('Value %s not expected in %s.' % (val, key))
            failed = True
        continue

    # If we got this far, it's an unexpected type
    ErrorMsg('Unexpected type %s for key %s.' % (str(type(src[key])), key))
    continue
  return not failed


def LoadProject(filename, ErrorMsg=ErrorMsgFunc, verbose=False):
  if verbose:
    errmsg = ErrorMsgFunc
  else:
    errmsg = IgnoreMsgFunc

  with open(filename, 'r') as descfile:
    desc = eval(descfile.read(), {}, {})
    if desc.get('DISABLE', False):
      return None
    if not ValidateFormat(desc, DSC_FORMAT, errmsg):
      ErrorMsg('Failed to validate: ' + filename)
      return None
    desc['FILEPATH'] = os.path.abspath(filename)
  return desc


def AcceptProject(desc, filters):
  # Check if we should filter node this on toolchain
  if not filters:
    return True

  for key, expected in filters.iteritems():
    # For any filtered key which is unspecified, assumed False
    value = desc.get(key, False)

    # If we provide an expected list, match at least one
    if type(expected) != list:
      expected = set([expected])
    if type(value) != list:
      value = set([value])

    if not set(expected) & set(value):
      return False

  # If we fall through, then we matched the filters
  return True


def PruneTree(tree, filters):
  out = collections.defaultdict(list)
  for branch, projects in tree.iteritems():
    for desc in projects:
      if AcceptProject(desc, filters):
        out[branch].append(desc)

  return out


def LoadProjectTree(srcpath, toolchains=None, verbose=False, filters=None,
                    ErrorMsg=ErrorMsgFunc, InfoMsg=ErrorMsgFunc):
  # Build the tree
  out = collections.defaultdict(list)
  for root, _, files in os.walk(srcpath):
    for filename in files:
      if fnmatch.fnmatch(filename, '*.dsc'):
        filepath = os.path.join(root, filename)
        desc = LoadProject(filepath, ErrorMsg, verbose)
        if desc:
          key = desc['DEST']
          out[key].append(desc)

  # Filter if needed
  if filters:
    out = PruneTree(out, filters)
  return out


def PrintProjectTree(tree, InfoMsg=ErrorMsgFunc):
  for key in tree:
    print key + ':'
    for val in tree[key]:
      print '\t' + val['NAME']


def main(argv):
  parser = optparse.OptionParser()
  parser.add_option('-e', '--experimental',
      help='build experimental examples and libraries', action='store_true')
  parser.add_option('-t', '--toolchain',
      help='Build using toolchain. Can be passed more than once.',
      action='append')

  options, files = parser.parse_args(argv[1:])
  filters = {}

  if len(files):
    parser.error('Not expecting files.')
    return 1

  if options.toolchain:
    filters['TOOLS'] = options.toolchain

  if not options.experimental:
    filters['EXPERIMENTAL'] = False

  tree = LoadProjectTree('.', filters=filters)
  PrintProjectTree(tree)

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
