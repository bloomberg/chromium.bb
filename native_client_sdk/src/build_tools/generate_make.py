#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ARCHITECTURES = ['32', '64']


def WriteMakefile(srcpath, dstpath, replacements):
  print 'opening: %s\n' % srcpath
  text = open(srcpath, 'rb').read()
  for key in replacements:
    text = text.replace(key, replacements[key])
  open(dstpath, 'wb').write(text)


def GetExtType(desc):
  if desc['TYPE'] in ['main', 'nexe']:
    ext = '.nexe'
  else:
    ext = '.so'
  return ext


def GenPatsubst(arch, macro, ext, EXT):
  return '$(patsubst %%.%s,%%_%s.o,$(%s_%s))' % (ext, arch, macro, EXT)


def SetVar(varname, values):
  if not values:
    return varname + ':=\n'

  line = varname + ':='
  out = ''
  for value in values:
    if not line:
      line = varname + '+='
    if len(line) + len(value) > 78:
      out += line[:-1] + '\n'
      line = ''
    else:
      line += value + ' '

  if line:
    out += line[:-1] + '\n'
  return out


def GenerateReplacements(desc):
  # Generate target settings
  tools = desc['TOOLS']

  settings = SetVar('VALID_TOOLCHAINS', tools)
  settings+= 'TOOLCHAIN?=%s\n\n' % tools[0]

  targets = []
  rules = ''

  for name in desc['TARGETS']:
    target = desc['TARGETS'][name]
    macro = name.upper()
    ext = GetExtType(target)

    sources = target['SOURCES']
    cc_sources = [fname for fname in sources if fname.endswith('.c')]
    cxx_sources = [fname for fname in sources if fname.endswith('.cc')]

    if cc_sources:
      flags = target.get('CCFLAGS', '')
      settings += SetVar(macro + '_CC', cc_sources)
      settings += SetVar(macro + '_CCFLAGS', flags)

    if cxx_sources:
      flags = target.get('CXXFLAGS', '')
      settings += SetVar(macro + '_CXX', cxx_sources)
      settings += SetVar(macro + '_CCFLAGS', flags)

    flags = target.get('LDFLAGS')
    settings += SetVar(macro + '_LDFLAGS', flags)

    for arch in ARCHITECTURES:
      object_sets = []
      if cc_sources:
        objs = '%s_%s_CC_O' % (macro, arch)
        rules += '%s:=%s\n' % (objs, GenPatsubst(arch, macro, 'c', 'CC'))
        rules += '$(%s) : %%_%s.o : %%.c $(THIS_MAKEFILE)\n' % (objs, arch)
        rules += '\t$(NACL_CC) -o $@ $< -m%s $(%s_CCFLAGS)\n\n' % (arch, macro)
        object_sets.append('$(%s)' % objs)
      if cxx_sources:
        objs = '%s_%s_CXX_O' % (macro, arch)
        rules += '%s:=%s\n' % (objs, GenPatsubst(arch, macro, 'cc', 'CXX'))
        rules += '$(%s) : %%_%s.o : %%.cc $(THIS_MAKEFILE)\n' % (objs, arch)
        rules += '\t$(NACL_CXX) -o $@ $< -m%s $(%s_CXXFLAGS)\n\n' % (arch,
                                                                     macro)
        object_sets.append('$(%s)' % objs)
      target_name = '%s_x86_%s%s' % (name, arch, ext)
      targets.append(target_name)
      rules += '%s : %s\n' % (target_name, ' '.join(object_sets))
      rules += '\t$(NACL_LINK) -o $@ $^ -m%s $(%s_LDFLAGS)\n\n' % (arch, macro)

  targets = 'all : '+ ' '.join(targets) 
  return {
      '__PROJECT_SETTINGS__' : settings,
      '__PROJECT_TARGETS__' : targets,
      '__PROJECT_RULES__' : rules
  }


# 'KEY' : ( <TYPE>, [Accepted Values], <Required?>)
DSC_FORMAT = {
    'TOOLS' : (list, ['host', 'newlib', 'glibc', 'pnacl'], True),
    'TARGETS' : (list, {
        'NAME': (str, '', True),
        'TYPE': (str, ['main', 'nexe', 'so'], True),
        'SOURCES': (list, '', True),
        'CCFLAGS': (list, '', False),
        'CXXFLAGS': (list, '', False),
        'LDFLAGS': (list, '', False)
    }, True),
    'PAGE': (str, '', False),
    'NAME': (str, '', False),
    'DESC': (str, '', False),
    'INFO': (str, '', False)
}


def ErrorMsgFunc(text):
  sys.stderr.write(text + '\n')


def ValidateFormat(src, format, ErrorMsg=ErrorMsgFunc):
  failed = False

  # Verify all required keys are there
  for key in format:
    (exp_type, exp_value, required) = format[key]
    if required and key not in src:
      ErrorMsg('Missing required key %s.' % key)
      failed = True

  # For each provided key, verify it's valid
  for key in src:
    # Verify the key is known
    if key not in format:
      ErrorMsg('Unexpected key %s.' % key)
      failed = True
      continue

    exp_type, exp_value, required = format[key]
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

    # If it's a string and there are expected values, make sure it matches
    if exp_type is str:
      if type(exp_value) is list and exp_value:
        if value not in exp_value:
          ErrorMsg('Value %s not expected for %s.' % (value, key))
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

# TODO(noelallen) : Remove before turning on
testdesc = {
    'TOOLS': ['newlib', 'glibc'],
    'TARGETS': {
        'hello_world' : {
            'TYPE' : 'main',
            'SOURCES' : ['hello_world.c'],
            'CCFLAGS' : '',
            'CXXFLAGS' : '',
            'LDFLAGS' : '',
        },
    },
    'PAGE': 'hello_world.html',
    'NAME': 'Hello World',
    'DESC': """
The Hello World In C example demonstrates the basic structure of all
Native Client applications. This example loads a Native Client module.  The
page tracks the status of the module as it load.  On a successful load, the
module will post a message containing the string "Hello World" back to
JavaScript which will display it as an alert.""",
    'INFO': 'Basic HTML, JavaScript, and module architecture.'
}


def main(argv):
  srcpath = os.path.join(SCRIPT_DIR, 'template.mk')
  repdict = GenerateReplacements(testdesc)
  WriteMakefile(srcpath, 'out.make', repdict)

if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
