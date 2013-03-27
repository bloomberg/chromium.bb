#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import cStringIO
import optparse
import os
import sys

import buildbot_common
from buildbot_common import ErrorExit
import easy_template
from generate_index import LandingPage

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SDK_SRC_DIR = os.path.dirname(SCRIPT_DIR)
SDK_EXAMPLE_DIR = os.path.join(SDK_SRC_DIR, 'examples')
SDK_DIR = os.path.dirname(SDK_SRC_DIR)
SRC_DIR = os.path.dirname(SDK_DIR)
OUT_DIR = os.path.join(SRC_DIR, 'out')
PPAPI_DIR = os.path.join(SRC_DIR, 'ppapi')

use_gyp = False

# Add SDK make tools scripts to the python path.
sys.path.append(os.path.join(SDK_SRC_DIR, 'tools'))
import getos

def Trace(msg):
  if Trace.verbose:
    sys.stderr.write(str(msg) + '\n')
Trace.verbose = False



def ShouldProcessHTML(desc):
  return desc['DEST'] in ('examples', 'tests')


def GenerateSourceCopyList(desc):
  sources = []
  # Add sources for each target
  for target in desc['TARGETS']:
    sources.extend(target['SOURCES'])

  # And HTML and data files
  sources.extend(desc.get('DATA', []))

  if ShouldProcessHTML(desc):
    sources.append('common.js')

  return sources


def RunTemplateFile(srcfile, dstfile, replace):
  dst = cStringIO.StringIO()
  with open(srcfile) as srcf:
    easy_template.RunTemplate(srcf, dst, replace)

  if os.path.exists(dstfile):
    with open(dstfile) as dstf:
      if dstf.read() == dst.getvalue():
        Trace('Generated %s is the same. Skipping' % dstfile)
        return

  with open(dstfile, 'w') as dstf:
    dstf.write(dst.getvalue())


def GetSourcesDict(sources):
  source_map = {}
  for key in ['.c', '.cc']:
    source_list = [fname for fname in sources if fname.endswith(key)]
    if source_list:
      source_map[key] = source_list
    else:
      source_map[key] = []
  return source_map


def GetProjectObjects(source_dict):
  object_list = []
  for key in ['.c', '.cc']:
    for src in source_dict[key]:
      object_list.append(os.path.splitext(src)[0])
  return object_list


def GetPlatforms(plat_list, plat_filter):
  platforms = []
  for plat in plat_list:
    if plat in plat_filter:
      platforms.append(plat)
  return platforms


# 'KEY' : ( <TYPE>, [Accepted Values], <Required?>)
DSC_FORMAT = {
    'TOOLS' : (list, ['newlib:arm', 'newlib:x64', 'newlib:x86', 'newlib',
                      'glibc', 'pnacl', 'win', 'linux'], True),
    'CONFIGS' : (list, ['Debug', 'Release'], False),
    'PREREQ' : (list, '', False),
    'TARGETS' : (list, {
        'NAME': (str, '', True),
        'TYPE': (str, ['main', 'lib', 'so'], True),
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
    'DEST': (str, ['examples', 'src', 'testlibs', 'tests'], True),
    'NAME': (str, '', False),
    'DATA': (list, '', False),
    'TITLE': (str, '', False),
    'DESC': (str, '', False),
    'FOCUS': (str, '', False),
    'GROUP': (str, '', False),
    'EXPERIMENTAL': (bool, [True, False], False)
}


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


def AddMakeBat(pepperdir, makepath):
  """Create a simple batch file to execute Make.

  Creates a simple batch file named make.bat for the Windows platform at the
  given path, pointing to the Make executable in the SDK."""

  makepath = os.path.abspath(makepath)
  if not makepath.startswith(pepperdir):
    ErrorExit('Make.bat not relative to Pepper directory: ' + makepath)

  makeexe = os.path.abspath(os.path.join(pepperdir, 'tools'))
  relpath = os.path.relpath(makeexe, makepath)

  fp = open(os.path.join(makepath, 'make.bat'), 'wb')
  outpath = os.path.join(relpath, 'make.exe')

  # Since make.bat is only used by Windows, for Windows path style
  outpath = outpath.replace(os.path.sep, '\\')
  fp.write('@%s %%*\n' % outpath)
  fp.close()


def FindFile(name, srcroot, srcdirs):
  checks = []
  for srcdir in srcdirs:
    srcfile = os.path.join(srcroot, srcdir, name)
    srcfile = os.path.abspath(srcfile)
    if os.path.exists(srcfile):
      return srcfile
    else:
      checks.append(srcfile)

  ErrorMsgFunc('%s not found in:\n\t%s' % (name, '\n\t'.join(checks)))
  return None


def IsNexe(desc):
  for target in desc['TARGETS']:
    if target['TYPE'] == 'main':
      return True
  return False


def ProcessHTML(srcroot, dstroot, desc, toolchains):
  name = desc['NAME']
  outdir = os.path.join(dstroot, desc['DEST'], name)
  srcfile = os.path.join(srcroot, 'index.html')
  dstfile = os.path.join(outdir, 'index.html')
  tools = GetPlatforms(toolchains, desc['TOOLS'])

  if use_gyp and getos.GetPlatform() != 'win':
    configs = ['debug', 'release']
  else:
    configs = ['Debug', 'Release']

  if use_gyp:
    path = "build/{tc}-{config}"
  else:
    path = "{tc}/{config}"

  replace = {
    'title': desc['TITLE'],
    'attrs':
        'data-name="%s" data-tools="%s" data-configs="%s" data-path="%s"' % (
        name, ' '.join(tools), ' '.join(configs), path),
  }
  RunTemplateFile(srcfile, dstfile, replace)


def LoadProject(filename, toolchains):
  """Generate a Master Makefile that builds all examples.

  Load a project description file, verifying it conforms and checking
  if it matches the set of requested toolchains.  Return None if the
  project is filtered out."""

  Trace('Processing %s...' % filename)
  # Default src directory is the directory the description was found in
  desc = open(filename, 'r').read()
  desc = eval(desc, {}, {})

  # Verify the format of this file
  if not ValidateFormat(desc, DSC_FORMAT):
    ErrorExit('Failed to validate: ' + filename)

  # Check if we are actually interested in this example
  match = False
  for toolchain in toolchains:
    if toolchain in desc['TOOLS']:
      match = True
      break
  if not match:
    return None

  desc['FILENAME'] = filename
  return desc


def FindAndCopyFiles(src_files, root, search_dirs, dst_dir):
  buildbot_common.MakeDir(dst_dir)
  for src_name in src_files:
    src_file = FindFile(src_name, root, search_dirs)
    if not src_file:
      ErrorExit('Failed to find: ' + src_name)
    dst_file = os.path.join(dst_dir, src_name)
    if os.path.exists(dst_file):
      if os.stat(src_file).st_mtime <= os.stat(dst_file).st_mtime:
        Trace('Skipping "%s", destination "%s" is newer.' % (
            src_file, dst_file))
        continue
    buildbot_common.CopyFile(src_file, dst_file)


def ProcessProject(srcroot, dstroot, desc, toolchains):
  name = desc['NAME']
  out_dir = os.path.join(dstroot, desc['DEST'], name)
  buildbot_common.MakeDir(out_dir)
  srcdirs = desc.get('SEARCH', ['.', '..'])

  # Copy sources to example directory
  sources = GenerateSourceCopyList(desc)
  FindAndCopyFiles(sources, srcroot, srcdirs, out_dir)

  # Copy public headers to the include directory.
  for headers_set in desc.get('HEADERS', []):
    headers = headers_set['FILES']
    header_out_dir = os.path.join(dstroot, headers_set['DEST'])
    FindAndCopyFiles(headers, srcroot, srcdirs, header_out_dir)

  make_path = os.path.join(out_dir, 'Makefile')

  if use_gyp:
    # Process the dsc file to produce gyp input
    dsc = desc['FILENAME']
    dsc2gyp = os.path.join(SDK_SRC_DIR, 'build_tools/dsc2gyp.py')
    gypfile = os.path.join(OUT_DIR, 'tmp', name, name + '.gyp')
    buildbot_common.Run([sys.executable, dsc2gyp, dsc, '-o', gypfile],
                        cwd=out_dir)

    # Run gyp on the generated gyp file
    if sys.platform == 'win32':
      generator = 'msvs'
    else:
      generator = os.path.join(SCRIPT_DIR, "make_simple.py")
    gyp = os.path.join(SDK_SRC_DIR, '..', '..', 'tools', 'gyp', 'gyp')
    if sys.platform == 'win32':
      gyp += '.bat'
    buildbot_common.Run([gyp, '-Gstandalone', '--format',  generator,
                        '--toplevel-dir=.', gypfile], cwd=out_dir)

  if sys.platform == 'win32' or not use_gyp:
    if IsNexe(desc):
      template = os.path.join(SCRIPT_DIR, 'template.mk')
    else:
      template = os.path.join(SCRIPT_DIR, 'library.mk')

    tools = {}
    tool_list = []
    for tool in desc['TOOLS']:
      if ':' in tool:
        tool, arch = tool.split(':')
      else:
        arch = None
      # Ignore tools that are not enabled in this SDK build
      if tool not in toolchains:
        continue
      tools.setdefault(tool, [])
      if tool not in tool_list:
        tool_list.append(tool)
      if arch:
        tools[tool].append(arch)

    template_dict = {
      'pre': desc.get('PRE', ''),
      'tools': tool_list,
      'targets': desc['TARGETS'],
    }
    RunTemplateFile(template, make_path, template_dict)

  outdir = os.path.dirname(os.path.abspath(make_path))
  pepperdir = os.path.dirname(os.path.dirname(outdir))
  AddMakeBat(pepperdir, outdir)
  return (name, desc['DEST'])


def GenerateMasterMakefile(in_path, out_path, projects):
  """Generate a Master Makefile that builds all examples. """
  project_names = [project['NAME'] for project in projects]
  template_dict = { 'projects': project_names }
  RunTemplateFile(in_path, out_path, template_dict)

  outdir = os.path.dirname(os.path.abspath(out_path))
  pepperdir = os.path.dirname(outdir)
  AddMakeBat(pepperdir, outdir)


def main(argv):
  usage = "usage: generate_make [options] <dsc_file ..>"
  parser = optparse.OptionParser(usage=usage)
  parser.add_option('--dstroot', help='Set root for destination.',
      default=os.path.join(OUT_DIR, 'pepper_canary'))
  parser.add_option('--master', help='Create master Makefile.',
      action='store_true')
  parser.add_option('--newlib', help='Create newlib examples.',
      action='store_true')
  parser.add_option('--glibc', help='Create glibc examples.',
      action='store_true')
  parser.add_option('--pnacl', help='Create pnacl examples.',
      action='store_true')
  parser.add_option('--host', help='Create host examples.',
      action='store_true')
  parser.add_option('--experimental', help='Create experimental examples.',
      action='store_true')
  parser.add_option('-v', '--verbose', help='Verbose output',
      action='store_true')

  toolchains = []
  platform = getos.GetPlatform()

  options, args = parser.parse_args(argv)
  if options.verbose:
    Trace.verbose = True
  if options.newlib:
    toolchains.append('newlib')
  if options.glibc:
    toolchains.append('glibc')
  if options.pnacl:
    toolchains.append('pnacl')
  if options.host:
    toolchains.append(platform)

  if not args:
    ErrorExit('Please specify one or more projects to generate Makefiles for.')

  # By default support newlib and glibc
  if not toolchains:
    toolchains = ['newlib', 'glibc', 'pnacl']

  master_projects = {}

  landing_page = LandingPage()
  for i, filename in enumerate(args):
    if i:
      # Print two newlines between each dsc file we process
      Trace('\n')
    desc = LoadProject(filename, toolchains)
    if not desc:
      Trace('Skipping %s, not in [%s].' % (filename, ', '.join(toolchains)))
      continue

    if desc.get('EXPERIMENTAL', False) and not options.experimental:
      Trace('Skipping %s, experimental only.' % (filename,))
      continue

    srcroot = os.path.dirname(os.path.abspath(filename))
    if not ProcessProject(srcroot, options.dstroot, desc, toolchains):
      ErrorExit('\n*** Failed to process project: %s ***' % filename)

    # if this is an example update it's html file.
    if ShouldProcessHTML(desc):
      ProcessHTML(srcroot, options.dstroot, desc, toolchains)

    # if this is an example, update landing page html file.
    if desc['DEST'] == 'examples':
      Trace('Adding desc: %s' % filename)
      landing_page.AddDesc(desc)

    # Create a list of projects for each DEST. This will be used to generate a
    # master makefile.
    master_projects.setdefault(desc['DEST'], []).append(desc)

  # Generate the landing page text file.
  index_html = os.path.join(options.dstroot, 'examples', 'index.html')
  with open(index_html, 'w') as fh:
    fh.write(landing_page.GeneratePage())

  if options.master:
    if use_gyp:
      master_in = os.path.join(SDK_EXAMPLE_DIR, 'Makefile_gyp')
    else:
      master_in = os.path.join(SDK_EXAMPLE_DIR, 'Makefile')
    for dest, projects in master_projects.iteritems():
      master_out = os.path.join(options.dstroot, dest, 'Makefile')
      GenerateMasterMakefile(master_in, master_out, projects)

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))

