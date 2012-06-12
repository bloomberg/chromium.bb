#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import buildbot_common
import optparse
import os
import sys


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SDK_SRC_DIR = os.path.dirname(SCRIPT_DIR)
SDK_EXAMPLE_DIR = os.path.join(SDK_SRC_DIR, 'examples')
SDK_DIR = os.path.dirname(SDK_SRC_DIR)
SRC_DIR = os.path.dirname(SDK_DIR)
OUT_DIR = os.path.join(SRC_DIR, 'out')
PPAPI_DIR = os.path.join(SRC_DIR, 'ppapi')

ARCHITECTURES = ['32', '64']


def WriteMakefile(srcpath, dstpath, replacements):
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
    if len(line) + len(value) > 78:
      out += line[:-1] + '\n'
      line = '%s+=%s ' % (varname, value)
    else:
      line += value + ' '

  if line:
    out += line[:-1] + '\n'
  return out


def GenerateCopyList(desc):
  sources = []

  # Add sources for each target
  for target in desc['TARGETS']:
    sources.extend(target['SOURCES'])

  # And HTML and data files
  sources.append(desc['NAME'] + '.html')
  sources.extend(desc.get('DATA', []))
  return sources


def GenerateReplacements(desc):
  # Generate target settings
  tools = desc['TOOLS']

  prerun = desc.get('PRE', '')
  postlaunch = desc.get('POST', '')
  prelaunch = desc.get('LAUNCH', '')

  settings = SetVar('VALID_TOOLCHAINS', tools)
  settings+= 'TOOLCHAIN?=%s\n\n' % tools[0]

  rules = ''
  targets = []
  remaps = ''
  for target in desc['TARGETS']:
    name = target['NAME']
    macro = name.upper()
    ext = GetExtType(target)

    sources = target['SOURCES']
    cc_sources = [fname for fname in sources if fname.endswith('.c')]
    cxx_sources = [fname for fname in sources if fname.endswith('.cc')]

    if cc_sources:
      flags = target.get('CCFLAGS', ['$(NACL_CCFLAGS)'])
      settings += SetVar(macro + '_CC', cc_sources)
      settings += SetVar(macro + '_CCFLAGS', flags)

    if cxx_sources:
      flags = target.get('CXXFLAGS', ['$(NACL_CXXFLAGS)'])
      settings += SetVar(macro + '_CXX', cxx_sources)
      settings += SetVar(macro + '_CXXFLAGS', flags)

    flags = target.get('LDFLAGS', ['$(NACL_LDFLAGS)'])
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
      if target['TYPE'] == 'so':
        remaps += ' -n %s,%s.so' % (target_name, name)

  nmf = desc['NAME'] + '.nmf'
  nmfs = '%s : %s\n' % (nmf, ' '.join(targets))
  nmfs +='\t$(NMF) $(NMF_ARGS) -o $@ $(NMF_PATHS) $^%s\n' % remaps

  targets = 'all : '+ ' '.join(targets + [nmf]) 
  return {
      '__PROJECT_SETTINGS__' : settings,
      '__PROJECT_TARGETS__' : targets,
      '__PROJECT_RULES__' : rules,
      '__PROJECT_NMFS__' : nmfs,
      '__PROJECT_PRELAUNCH__' : prelaunch,
      '__PROJECT_PRERUN__' : prerun,
      '__PROJECT_POSTLAUNCH__' : postlaunch
  }


# 'KEY' : ( <TYPE>, [Accepted Values], <Required?>)
DSC_FORMAT = {
    'TOOLS' : (list, ['newlib', 'glibc', 'pnacl'], True),
    'TARGETS' : (list, {
        'NAME': (str, '', True),
        'TYPE': (str, ['main', 'nexe', 'so'], True),
        'SOURCES': (list, '', True),
        'CCFLAGS': (list, '', False),
        'CXXFLAGS': (list, '', False),
        'LDFLAGS': (list, '', False)
    }, True),
    'POST': (str, '', False),
    'PRE': (str, '', False),
    'DEST': (str, ['examples', 'src'], True),
    'NAME': (str, '', False),
    'DATA': (list, '', False),
    'TITLE': (str, '', False),
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


def AddMakeBat(pepperdir, makepath):
  """Create a simple batch file to execute Make.

  Creates a simple batch file named make.bat for the Windows platform at the
  given path, pointing to the Make executable in the SDK."""

  makepath = os.path.abspath(makepath)
  if not makepath.startswith(pepperdir):
    buildbot_common.ErrorExit('Make.bat not relative to Pepper directory: ' +
                              makepath)

  makeexe = os.path.abspath(os.path.join(pepperdir, 'tools'))
  relpath = os.path.relpath(makeexe, makepath)

  fp = open(os.path.join(makepath, 'make.bat'), 'wb')
  outpath = os.path.join(relpath, 'make.exe')

  # Since make.bat is only used by Windows, for Windows path style
  outpath = outpath.replace(os.path.sep, '\\')
  fp.write('@%s %%*\n' % outpath)
  fp.close()


def ProcessProject(dstroot, template, filename):
  print '\n\nProcessing %s...' % filename
  # Default src directory is the directory the description was found in 
  src_dir = os.path.dirname(os.path.abspath(filename))
  desc = open(filename, 'rb').read()
  desc = eval(desc, {}, {})
  if not ValidateFormat(desc, DSC_FORMAT):
    return (None, None)

  name = desc['NAME']
  out_dir = os.path.join(dstroot, desc['DEST'], name)
  buildbot_common.MakeDir(out_dir)

  # Copy sources to example directory
  sources = GenerateCopyList(desc)
  for src_name in sources:
    src_file = os.path.join(src_dir, src_name)
    dst_file = os.path.join(out_dir, src_name)
    buildbot_common.CopyFile(src_file, dst_file)

  # Add Makefile
  repdict = GenerateReplacements(desc)
  make_path = os.path.join(out_dir, 'Makefile')
  WriteMakefile(template, make_path, repdict)

  outdir = os.path.dirname(os.path.abspath(make_path))
  pepperdir = os.path.dirname(os.path.dirname(outdir))
  AddMakeBat(pepperdir, outdir)
  return (name, desc['DEST'])


def GenerateExamplesMakefile(in_path, out_path, examples):
  """Generate a Makefile that includes only the examples supported by this
  SDK."""
  # Line wrap the PROJECTS variable
  wrap_width = 80
  projects_text = SetVar('PROJECTS', examples)

  out_makefile_text = ''
  wrote_projects_text = False
  snipping = False
  for line in open(in_path, 'r'):
    if line.startswith('# =SNIP='):
      snipping = not snipping
      continue

    if snipping:
      if not wrote_projects_text:
        out_makefile_text += projects_text
      wrote_projects_text = True
    else:
      out_makefile_text += line
  open(out_path, 'w').write(out_makefile_text)
  
  outdir = os.path.dirname(os.path.abspath(out_path))
  pepperdir = os.path.dirname(outdir)
  AddMakeBat(pepperdir, outdir)


def main(argv):
  parser = optparse.OptionParser()
  parser.add_option('--dstroot', help='Set root for destination.',
      dest='dstroot', default=OUT_DIR)
  parser.add_option('--template', help='Set the makefile template.',
      dest='template', default=os.path.join(SCRIPT_DIR, 'template.mk'))
  parser.add_option('--master', help='Create master Makefile.',
      action='store_true', dest='master', default=False)

  examples = []
  options, args = parser.parse_args(argv)
  for filename in args:
    name, dest = ProcessProject(options.dstroot, options.template, filename)
    if not name:
      print '\n*** Failed to process project: %s ***' % filename
      return 1

    if dest == 'examples':
      examples.append(name)

  if options.master:
    master_in = os.path.join(SDK_EXAMPLE_DIR, 'Makefile')
    master_out = os.path.join(options.dstroot, 'examples', 'Makefile')
    GenerateExamplesMakefile(master_in, master_out, examples)

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
