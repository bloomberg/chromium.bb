#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import StringIO
import sys
import os
import optparse

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.append(os.path.join(os.path.dirname(SCRIPT_DIR), 'tools'))

import getos

valid_tools = ['newlib', 'glibc', getos.GetPlatform()]


def Error(msg):
  print(msg)
  sys.exit(1)


PREAMBLE = """\
{
  'includes': ['%s/build_tools/nacl.gypi'],
"""

NEXE_TARGET = """\
    {
      'target_name': '%(NAME)s_x86_32%(EXT)s',
      'product_name': '%(NAME)s_x86_32%(EXT)s',
      'type': '%(GYP_TYPE)s',
      'sources': %(SOURCES)s,
      'libraries': %(LIBS)s,
      'include_dirs': %(INCLUDES)s,
      'cflags': ['-m32', '-pedantic'] + %(CFLAGS)s,
      'make_valid_configurations': ['newlib-debug', 'newlib-release',
                                    'glibc-debug', 'glibc-release'],
      'ldflags': ['-m32', '-L../../lib/x86_32/<(CONFIGURATION_NAME)'],
      'toolset': 'target',
      %(CONFIGS)s
    },
    {
      'target_name': '%(NAME)s_x86_64%(EXT)s',
      'product_name': '%(NAME)s_x86_64%(EXT)s',
      'type': '%(GYP_TYPE)s',
      'sources': %(SOURCES)s,
      'libraries': %(LIBS)s,
      'include_dirs': %(INCLUDES)s,
      'make_valid_configurations': ['newlib-debug', 'newlib-release',
                                    'glibc-debug', 'glibc-release'],
      'cflags': ['-m64', '-pedantic'] + %(CFLAGS)s,
      'ldflags': ['-m64', '-L../../lib/x86_64/<(CONFIGURATION_NAME)'],
      'toolset': 'target',
      %(CONFIGS)s
    },
"""

NLIB_TARGET = """\
    {
      'target_name': '%(NAME)s_x86_32%(EXT)s',
      'product_name': 'lib%(NAME)s%(EXT)s',
      'product_dir': '../../lib/x86_32/<(CONFIGURATION_NAME)',
      'type': '%(GYP_TYPE)s',
      'sources': %(SOURCES)s,
      'libraries': %(LIBS)s,
      'include_dirs': %(INCLUDES)s,
      'cflags': ['-m32', '-pedantic'] + %(CFLAGS)s,
      'make_valid_configurations': ['newlib-debug', 'newlib-release',
                                    'glibc-debug', 'glibc-release'],
      'ldflags': ['-m32'],
      'toolset': 'target',
      %(CONFIGS)s
    },
    {
      'target_name': '%(NAME)s_x86_64%(EXT)s',
      'product_name': 'lib%(NAME)s%(EXT)s',
      'product_dir': '../../lib/x86_64/<(CONFIGURATION_NAME)',
      'type': '%(GYP_TYPE)s',
      'sources': %(SOURCES)s,
      'libraries': %(LIBS)s,
      'include_dirs': %(INCLUDES)s,
      'make_valid_configurations': ['newlib-debug', 'newlib-release',
                                    'glibc-debug', 'glibc-release'],
      'cflags': ['-m64', '-pedantic'] + %(CFLAGS)s,
      'ldflags': ['-m64'],
      'toolset': 'target',
      %(CONFIGS)s
    },
"""

HOST_LIB_TARGET = """\
    {
      'target_name': '%(NAME)s%(EXT)s',
      'type': '%(GYP_TYPE)s',
      'toolset': 'host',
      'sources': %(SOURCES)s,
      'cflags': %(CFLAGS)s,
      'cflags_c': ['-std=gnu99'],
      'include_dirs': %(INCLUDES)s,
      'make_valid_configurations': ['host-debug', 'host-release'],
      'product_dir': '../../lib/%(ARCH)s/<(CONFIGURATION_NAME)',
      'product_name': '%(NAME)s%(EXT)s',
      %(CONFIGS)s
    },
"""

HOST_EXE_TARGET = """\
    {
      'target_name': '%(NAME)s%(EXT)s',
      'type': '%(GYP_TYPE)s',
      'toolset': 'host',
      'sources': %(SOURCES)s,
      'cflags': %(CFLAGS)s,
      'cflags_c': ['-std=gnu99'],
      'ldflags': ['-L../../lib/%(ARCH)s/<(CONFIGURATION_NAME)'],
      'libraries': %(LIBS)s,
      'include_dirs': %(INCLUDES)s,
      'make_valid_configurations': ['host-debug', 'host-release'],
      'msvs_settings': {
        'VCLinkerTool': {
          'AdditionalLibraryDirectories':
            ['../../lib/%(ARCH)s/<(CONFIGURATION_NAME)'],
         }
       },
       %(CONFIGS)s
    },
"""

NMF_TARGET = """\
    {
      'target_name': '%(NAME)s_%(TOOLCHAIN)s.nmf',
      'product_name': '%(NAME)s.nmf',
      'product_dir': '<(PRODUCT_DIR)/%(TOOLCHAIN)s',
      'type': 'none',
      'make_valid_configurations': ['%(TOOLCHAIN)s-debug', '%(TOOLCHAIN)s-release'],
      'actions': [
        {
          'action_name': 'nmf',
          'inputs': ['<(PRODUCT_DIR)/%(NAME)s_x86_32.nexe',
                     '<(PRODUCT_DIR)/%(NAME)s_x86_64.nexe'] + %(SODEPS)s,
          'outputs': ['<(PRODUCT_DIR)/%(NAME)s.nmf'],
          'action': ['../../tools/create_nmf.py', '-t', '%(TOOLCHAIN)s', '-s',
                     '<(PRODUCT_DIR)'] + %(NMFACTION)s,
        },
      ]
    },
"""

TOOLCHAIN_CONFIG = """\
        '%(toolchain)s-release' : {
          'cflags' : ['-O2'],
        },
        '%(toolchain)s-debug' : {
          'cflags' : ['-g', '-O0'],
        },
"""

NEXE_CONFIG = """\
          '%(toolchain)s-release' : {
            'cflags' : ['--%(toolchain)s', '-O2',
                        '-idirafter', '../../include'],
            'ldflags' : ['--%(toolchain)s'],
            'arflags' : ['--%(toolchain)s'],
          },
          '%(toolchain)s-debug' : {
            'cflags' : ['--%(toolchain)s', '-g', '-O0',
                        '-idirafter', '../../include'],
            'ldflags' : ['--%(toolchain)s'],
            'arflags' : ['--%(toolchain)s'],
          },
"""

WIN32_CONFIGS = """\
  'target_defaults': {
    'default_configuration': 'Debug_PPAPI',
    'configurations': {
      'Debug_PPAPI': {
        'msvs_configuration_platform': 'PPAPI',
        'msbuild_configuration_attributes': {
          'ConfigurationType': 'DynamicLibrary'
        },
        'include_dirs': ['../../include/win'],
        'defines': ['_WINDOWS', '_DEBUG', 'WIN32'],
      },
      'Release_PPAPI': {
        'msvs_configuration_platform': 'PPAPI',
        'msbuild_configuration_attributes': {
          'ConfigurationType': 'DynamicLibrary'
        },
        'include_dirs': ['../../include/win'],
        'defines': ['_WINDOWS', 'NDEBUG', 'WIN32'],
      },
      'Debug_NaCl': {
        'msvs_configuration_platform': 'NaCl',
        'msbuild_configuration_attributes': {
          'ConfigurationType': 'Application'
        },
      },
      'Release_NaCl': {
        'msvs_configuration_platform': 'NaCl',
        'msbuild_configuration_attributes': {
          'ConfigurationType': 'Application'
        },
      },
    },
  },
"""


def WriteNaClTargets(output, target, tools):
  configs = "'configurations' : {\n"
  for tc in tools:
    if tc not in valid_tools:
      continue
    if tc in ['newlib', 'glibc']:
      configs += NEXE_CONFIG % {'toolchain': tc}
  configs += "      }"
  target['CONFIGS'] = configs
  if target['TYPE'] == 'lib':
    output.write(NLIB_TARGET % target)
  else:
    output.write(NEXE_TARGET % target)


def ConfigName(toolchain):
  if toolchain == getos.GetPlatform():
    return 'host'
  else:
    return toolchain


def ProcessDSC(filename, outfile=None):
  if not os.path.exists(filename):
    Error("file not found: %s" % filename)

  desc = open(filename).read()
  desc = eval(desc, {}, {})
  if not desc.get('TARGETS'):
    Error("no TARGETS found in dsc")

  if not outfile:
    outfile = desc['NAME'] + '.gyp'
    outfile = os.path.join(os.path.dirname(filename), outfile)

  output = StringIO.StringIO()

  srcdir = os.path.dirname(SCRIPT_DIR)
  output.write(PREAMBLE % srcdir.replace("\\", '/'))

  win32 = sys.platform in ('win32', 'cygwin')
  if win32:
    output.write(WIN32_CONFIGS)
  else:
    for tc in desc['TOOLS']:
      if tc in valid_tools:
        default = '%s-debug' % ConfigName(tc)
        break

    output.write("""\
  'target_defaults': {
    'default_configuration': '%s',
    'configurations' : {\n""" % default)

    for tc in desc['TOOLS']:
      if tc not in valid_tools:
        continue
      output.write(TOOLCHAIN_CONFIG % {'toolchain': ConfigName(tc)})

    output.write("    }\n  },\n")

  output.write("\n  'targets': [\n")

  # make a list of all the so target names so that the nmf rules
  # can depend on them all
  sofiles = []
  soremap = []
  for target in desc['TARGETS']:
    if target['TYPE'] == 'so':
      name = target['NAME']
      sofiles.append('<(PRODUCT_DIR)/%s_x86_64.so' % name)
      sofiles.append('<(PRODUCT_DIR)/%s_x86_32.so' % name)
      soremap += ['-n', '%s_x86_64.so,%s.so' % (name, name)]
      soremap += ['-n', '%s_x86_32.so,%s.so' % (name, name)]


  # iterate through dsc targets generating gyp targets
  for target in desc['TARGETS']:
    target.setdefault('INCLUDES', [])
    target['INCLUDES'] = [x.replace("$(NACL_SDK_ROOT)", "../..")
                          for x in target['INCLUDES']]

    libs = target.get('LIBS', [])
    if win32:
      libs = [l for l in libs if l not in ('ppapi', 'ppapi_cpp')]
      target['LIBS'] = ['-l' + l + '.lib' for l in libs]
    else:
      target['LIBS'] = ['-l' + l for l in libs]
    if target['TYPE'] == 'so':
      if win32:
        target['EXT'] = ''
      else:
        target['EXT'] = '.so'
      target['GYP_TYPE'] = 'shared_library'
    elif target['TYPE'] == 'lib':
      if win32:
        target['EXT'] = ''
      else:
        target['EXT'] = '.a'
      target['GYP_TYPE'] = 'static_library'
    elif target['TYPE'] == 'main':
      target['EXT'] = '.nexe'
      target['GYP_TYPE'] = 'executable'
    else:
      Error("unknown type: %s" % target['TYPE'])

    target['CFLAGS'] = target.get('CXXFLAGS', [])

    if not win32 and ('newlib' in desc['TOOLS'] or 'glibc' in desc['TOOLS']):
      WriteNaClTargets(output, target, desc['TOOLS'])
      if target['TYPE'] == 'main':
        target['SODEPS'] = sofiles
        target['NMFACTION'] = ['-o', '<@(_outputs)', '-L<(NMF_PATH1)',
                               '-L<(NMF_PATH2)', '-D', '<(OBJDUMP)',
                               '<@(_inputs)']
        target['NMFACTION'] += soremap
        if 'newlib' in desc['TOOLS']:
          target['TOOLCHAIN'] = 'newlib'
          output.write(NMF_TARGET % target)
        if 'glibc' in desc['TOOLS']:
          target['TOOLCHAIN'] = 'glibc'
          output.write(NMF_TARGET % target)

    if win32 or getos.GetPlatform() in desc['TOOLS']:
      target['ARCH'] = 'x86_32'
      target['INCLUDES'].append('../../include')
      if win32:
        target['HOST'] = 'win'
        target['CONFIGS'] = ''
        target['CFLAGS'] = []
      else:
        target['CONFIGS'] = ''
        target['HOST'] = 'linux'
        target['CFLAGS'].append('-fPIC')
      if target['TYPE'] == 'main':
        target['GYP_TYPE'] = 'shared_library'
        if win32:
          target['EXT'] = ''
        else:
          target['EXT'] = '.so'
        output.write(HOST_EXE_TARGET % target)
      else:
        output.write(HOST_LIB_TARGET % target)

  output.write('  ],\n}\n')

  print('Writing: ' + outfile)
  open(outfile, 'w').write(output.getvalue())


def main(args):
  parser = optparse.OptionParser()
  parser.add_option('-o', help='Set output filename.', dest='output')
  options, args = parser.parse_args(args)
  if not args:
    Error('No .dsc file specified.')

  if options.output:
    outdir = os.path.dirname(options.output)
    if not os.path.exists(outdir):
      os.makedirs(outdir)

  assert len(args) == 1
  ProcessDSC(args[0], options.output)


if __name__ == '__main__':
  main(sys.argv[1:])
